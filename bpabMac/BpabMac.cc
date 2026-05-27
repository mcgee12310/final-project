#include "BpabMac.h"
#include <omnetpp.h>
#include <sstream>
#include <cmath>
#include "BpabTraCIManager.h"

Define_Module(BpabMac);

#define WEBLOG(msgArgs) \
    do { \
        std::ostringstream _ss; \
        _ss << msgArgs; \
        trace() << _ss.str(); \
        BpabTraCIManager::writeToUnifiedLog(simTime().dbl(), self, "MAC_EVENT", _ss.str()); \
        if (BpabTraCIManager::tcpClientSocket != -1) { \
            std::ostringstream _netSs; \
            _netSs << simTime().dbl() << " MAC " << _ss.str() << "\n"; \
            std::string _msg = _netSs.str(); \
            ::send(BpabTraCIManager::tcpClientSocket, _msg.c_str(), _msg.length(), 0); \
        } \
    } while(0)

static const char* dirName(int d) {
    switch (d) {
        case EAST:  return "EAST";
        case WEST:  return "WEST";
        case NORTH: return "NORTH";
        case SOUTH: return "SOUTH";
        case INTER: return "INTER";
        default:    return "UNKNOWN";
    }
}

// Trả về hướng đối diện
static int oppositeDir(int d) {
    switch (d) {
        case EAST:  return WEST;
        case WEST:  return EAST;
        case NORTH: return SOUTH;
        case SOUTH: return NORTH;
        default:    return -1;
    }
}

// Trả về 2 hướng giao cắt với trục đã cho
static void crossDirs(int axisDir, int &crossA, int &crossB) {
    if (axisDir == EAST || axisDir == WEST) {
        crossA = NORTH;
        crossB = SOUTH;
    } else {
        crossA = EAST;
        crossB = WEST;
    }
}

// ─────────────────────────────────────────────
// startup
// ─────────────────────────────────────────────
void BpabMac::startup() {
    VirtualMac::startup();

    cModule* node     = getParentModule()->getParentModule();
    cModule* mobility = node->getSubmodule("MobilityManager");
    if (!mobility) error("MobilityManager not found!");
    mobilityModule = check_and_cast<VirtualMobilityManager*>(mobility);

    maxIterations = par("maxIterations");
    rangeR        = par("rangeR");
    widthW        = hasPar("widthW")      ? (double)par("widthW")      : 50.0;
    minProgress   = hasPar("minProgress") ? (double)par("minProgress") : 20.0;
    slotDuration  = par("slotDuration");
    maxRetries    = par("maxRetries");

    retryCount        = 0;
    bpabMacState      = BPAB_IDLE;
    currentIteration  = 0;
    heardBB           = false;
    heardCTB          = false;
    heardAnyCTB       = false;
    isTransmitting    = false;
    packetToBroadcast = NULL;
    myInterRole       = INTER_ROLE_NONE;
    crossIteration    = 0;
    crossWon          = false;
    crossHeardBB      = false;
    crossIsTransmitting = false;
    lastRTBDirection  = EAST;
    lastDataDestId    = -1;

    myX = mobilityModule->getLocation().x;
    myY = mobilityModule->getLocation().y;
    lastX = myX; lastY = myY;
    transmissionDirection = EAST;
    setTimer(9, 1.0);

    WEBLOG("EVENT:POS | Node:" << self << " | x:" << myX << " | y:" << myY);
}

BpabMac::~BpabMac() {
    if (packetToBroadcast) cancelAndDelete(packetToBroadcast);
}

// ─────────────────────────────────────────────
// fromNetworkLayer
// ─────────────────────────────────────────────
void BpabMac::fromNetworkLayer(cPacket *msg, int destination) {
    if (bpabMacState != BPAB_IDLE) {
        WEBLOG("EVENT:REJECT | Node:" << self << " | Reason:MAC_BUSY");
        delete msg;
        return;
    }
    preparePacket(msg);

    if (BpabTraCIManager::isNodeAtIntersection(self)) {
        WEBLOG("EVENT:INIT_INTER_SOURCE | Node:" << self);
        incomingBranchDir = -1; // Nguồn tại giao lộ: không loại trừ hướng nào
        sendInterRTB();
    } else {
        transmissionDirection = calculateTransmissionDirection();
        sendRTB();
    }
}

// ─────────────────────────────────────────────
// Hàm phụ trợ giao lộ
// ─────────────────────────────────────────────

// Tính timeout node nguồn chờ toàn bộ tranh chấp UMBP
// = SIFS + (2N+1)τ (cross bắt đầu) + N*2τ (cross chạy) + SIFS + CW_MAX*τ (CW)
double BpabMac::interTotalTimeout(int /*excludeDir*/) {
    double sifs   = SIFS_SLOTS * slotDuration;
    double oppEnd = maxIterations * 2.0 * slotDuration; // opposite kết thúc sau 2Nτ
    // cross bắt đầu sau SIFS + (2N+1)τ, chạy N*2τ, sau đó SIFS + CW_MAX
    double crossEnd = sifs + (2 * maxIterations + 1) * slotDuration
                    + maxIterations * 2.0 * slotDuration
                    + sifs + CW_MAX * slotDuration;
    double total = (oppEnd > crossEnd ? oppEnd : crossEnd) + slotDuration * 5;
    return total;
}

// Hướng của node relay so với tâm nguồn (dùng góc phần tư)
int BpabMac::getInterBranch(double mX, double mY, double sX, double sY) {
    double dx = mX - sX;
    double dy = mY - sY;
    if (fabs(dx) >= fabs(dy)) return (dx > 0) ? EAST : WEST;
    return (dy > 0) ? NORTH : SOUTH;
}

// ─────────────────────────────────────────────
// fromRadioLayer
// ─────────────────────────────────────────────
void BpabMac::fromRadioLayer(cPacket *msg, double rssi, double lqi) {
    BPABPacket *pkt = dynamic_cast<BPABPacket*>(msg);
    if (!pkt) return;

    switch (pkt->getBpabType()) {

    // ── RTB ──────────────────────────────────
    case BPAB_RTB: {
        // ACK ngầm
        if (bpabMacState == BPAB_WAIT_ACK) {
            if (pkt->getSourceId() == lastDataDestId) {
                cancelTimer(8);
                bpabMacState = BPAB_IDLE;
                WEBLOG("EVENT:ACK_RECEIVED | Node:" << self << " | Status:SUCCESS_IMPLICIT");
                if (packetToBroadcast) { delete packetToBroadcast; packetToBroadcast = NULL; }
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
            }
            return;
        }

        if (bpabMacState != BPAB_IDLE) return;

        double mX  = mobilityModule->getLocation().x;
        double mY  = mobilityModule->getLocation().y;
        double sX  = pkt->getSourceX();
        double sY  = pkt->getSourceY();
        int    dir = pkt->getDirection();

        // ── RTB INTER (giao lộ UMBP) ──
        if (dir == INTER) {
            int excludeDir = pkt->getIncomingDir(); // hướng nguồn đến (-1 nếu nguồn tại giao lộ)
            int myBranch   = getInterBranch(mX, mY, sX, sY);

            // Loại trừ hướng nguồn đến
            if (excludeDir != -1 && myBranch == excludeDir) return;

            // Kiểm tra isValidForwardNode theo hướng của nhánh mình
            if (!isValidForwardNode(mX, mY, sX, sY, myBranch, rangeR)) {
                WEBLOG("EVENT:FILTER | Node:" << self << " | Status:WRONG_DIRECTION");
                return;
            }

            double dist = sqrt((mX-sX)*(mX-sX) + (mY-sY)*(mY-sY));

            interSrcId        = pkt->getSourceId();
            interSrcX         = sX;
            interSrcY         = sY;
            myInterBranch     = myBranch;
            incomingBranchDir = excludeDir;

            // Xác định vai trò: opposite hay cross
            int oppDir = (excludeDir == -1) ? -1 : oppositeDir(excludeDir);
            int cA, cB;
            if (excludeDir != -1) crossDirs(excludeDir, cA, cB);
            else                  crossDirs(EAST, cA, cB); // nguồn tại giao lộ: 4 hướng đều cross

            if (excludeDir != -1 && myBranch == oppDir) {
                // ── Vai trò OPPOSITE: binary contention thông thường ──
                myInterRole = INTER_ROLE_OPPOSITE;

                srcId            = interSrcId;
                this->srcX       = sX;
                this->srcY       = sY;
                this->srcDirection = myBranch; // dùng hướng nhánh làm srcDirection
                limitL           = 0;
                limitU           = rangeR;
                currentIteration = 0;
                heardBB          = false;
                isTransmitting   = false;
                bpabMacState     = BPAB_INTER_CONTENDING;

                WEBLOG("EVENT:INTER_JOIN_OPPOSITE | Node:" << self
                       << " | Branch:" << dirName(myBranch) << " | Dist:" << dist);
                WEBLOG("EVENT:STATE | Node:" << self << " | State:CONTENDING");

                // Bắt đầu ngay sau SIFS nhỏ (đồng bộ với slot boundary)
                double now      = simTime().dbl();
                double rtbTime  = pkt->getRtbSentTime();
                double boundary = rtbTime + slotDuration;
                while (boundary <= now + slotDuration * 0.01) boundary += slotDuration;
                slotStartTime = boundary;
                setTimer(1, boundary - now);

            } else {
                // ── Vai trò CROSS_A hoặc CROSS_B ──
                myInterRole = (myBranch == cA) ? INTER_ROLE_CROSS_A : INTER_ROLE_CROSS_B;

                crossIteration    = 0;
                limitLCross       = 0;
                limitUCross       = rangeR;
                crossHeardBB      = false;
                crossIsTransmitting = false;
                crossWon          = false;
                bpabMacState      = BPAB_INTER_CONTENDING;

                WEBLOG("EVENT:INTER_JOIN_CROSS | Node:" << self
                       << " | Branch:" << dirName(myBranch)
                       << " | Role:" << (myInterRole == INTER_ROLE_CROSS_A ? "CROSS_A" : "CROSS_B")
                       << " | Dist:" << dist);
                WEBLOG("EVENT:STATE | Node:" << self << " | State:CONTENDING");

                // Cross bắt đầu sau SIFS + (2N+1)τ tính từ rtbSentTime
                double sifs       = SIFS_SLOTS * slotDuration;
                double crossStart = pkt->getRtbSentTime()
                                  + sifs
                                  + (2 * maxIterations + 1) * slotDuration;
                double now = simTime().dbl();
                if (crossStart < now + slotDuration * 0.01)
                    crossStart = now + slotDuration * 0.01;
                setTimer(11, crossStart - now);
            }
            break;
        }

        // ── RTB có hướng thông thường ──
        if (!isValidForwardNode(mX, mY, sX, sY, dir, rangeR)) {
            WEBLOG("EVENT:FILTER | Node:" << self << " | Status:WRONG_DIRECTION");
            return;
        }

        // Bypass: node tại giao lộ nhận RTB từ node thường → preempt
        if (BpabTraCIManager::isNodeAtIntersection(self)
            && !BpabTraCIManager::isNodeAtIntersection(pkt->getSourceId())) {

            WEBLOG("EVENT:INTERSECTION_FAST_TRACK | Node:" << self << " | Action:PREEMPT_BEFORE_BB");
            srcId              = pkt->getSourceId();
            this->srcX         = sX;
            this->srcY         = sY;
            this->srcDirection = dir;
            cancelTimer(1); cancelTimer(2);
            toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));
            heardCTB     = false;
            bpabMacState = BPAB_PRE_CTB;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:PRE_CTB");
            double backoff = uniform(0.0, slotDuration * 0.3);
            WEBLOG("EVENT:WINNER | Node:" << self << " | Backoff:" << backoff
                   << " | Type:INTERSECTION_PREEMPT");
            setTimer(6, backoff);
            return;
        }

        // Node thường: binary contention
        WEBLOG("EVENT:JOIN_BPAB | Node:" << self
               << " | Src:" << pkt->getSourceId()
               << " | Dir:" << dirName(dir));
        srcId = pkt->getSourceId(); this->srcX = sX; this->srcY = sY;
        limitL = 0; limitU = rangeR;
        this->srcDirection = dir;
        currentIteration = 0; heardBB = false; isTransmitting = false;
        bpabMacState = BPAB_CONTENDING;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:CONTENDING");

        {
            double now = simTime().dbl();
            double boundary = pkt->getRtbSentTime() + slotDuration;
            while (boundary <= now + slotDuration * 0.01) boundary += slotDuration;
            slotStartTime = boundary;
            setTimer(1, slotStartTime - now);
        }
        break;
    }

    // ── CTB ──────────────────────────────────
    case BPAB_CTB: {
        int winnerId = pkt->getSourceId();

        if (bpabMacState == BPAB_PRE_CTB) {
            WEBLOG("EVENT:BACKDOWN | Node:" << self << " | Reason:HEARD_OTHER_CTB");
            heardCTB = true;
            break;
        }
        if (bpabMacState == BPAB_CONTENDING) {
            cancelTimer(1); cancelTimer(2);
            WEBLOG("EVENT:DROP_OUT | Node:" << self << " | Reason:HEARD_CTB_WHILE_CONTENDING");
            bpabMacState = BPAB_IDLE;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
            break;
        }
        // Node INTER_CONTENDING nhận CTB từ cùng nhánh → rút lui
        if (bpabMacState == BPAB_INTER_CONTENDING) {
            if (pkt->getDirection() == INTER
                && pkt->getIncomingDir() == myInterBranch
                && pkt->getDestinationId() == interSrcId) {
                cancelTimer(11); cancelTimer(12); cancelTimer(13); cancelTimer(14);
                WEBLOG("EVENT:INTER_DROP | Node:" << self
                       << " | Reason:OTHER_CTB_SAME_BRANCH | Branch:" << dirName(myInterBranch));
                bpabMacState = BPAB_IDLE;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
            }
            break;
        }
        // Node nguồn giao lộ nhận CTB
        if (bpabMacState == BPAB_WAIT_CTB && lastRTBDirection == INTER) {
            heardAnyCTB = true;
            WEBLOG("EVENT:INTER_HEARD_CTB | Node:" << self
                   << " | From:" << winnerId
                   << " | Branch:" << dirName(pkt->getIncomingDir()));
            break;
        }
        // Node đường thẳng nhận CTB
        if (bpabMacState == BPAB_WAIT_CTB && packetToBroadcast) {
            if (pkt->getDestinationId() == self) {
                retryCount = 0;
                cancelTimer(3); cancelTimer(4);
                sendData(winnerId);
            }
        }
        break;
    }

    // ── DATA ─────────────────────────────────
    case BPAB_DATA: {
        cPacket *netPkt = pkt->decapsulate();

        // DATA giao lộ broadcast
        if (pkt->getDestinationId() == BROADCAST_INTER) {
            if (bpabMacState != BPAB_WAIT_DATA) {
                delete netPkt; break;
            }
            cancelTimer(5);
            WEBLOG("EVENT:INTER_RCV_DATA | Node:" << self
                   << " | Branch:" << dirName(myInterBranch));
            bpabMacState = BPAB_IDLE;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
            preparePacket(netPkt);
            transmissionDirection = myInterBranch;
            // Jitter nhỏ để tránh đụng độ khi nhiều relay cùng gửi RTB
            double branchOffset = 0;

            switch (myInterBranch) {
                case EAST:  branchOffset = 0; break;
                case NORTH: branchOffset = slotDuration * 20; break;
                case SOUTH: branchOffset = slotDuration * 40; break;
                case WEST:  branchOffset = slotDuration * 60; break;
            }

            double jitter = branchOffset +
                            uniform(slotDuration * 1,
                                    slotDuration * 5);
            WEBLOG("EVENT:INTER_RELAY_START | Node:" << self
                   << " | Dir:" << dirName(transmissionDirection)
                   << " | Jitter:" << jitter);
            setTimer(7, jitter);
            break;
        }

        // DATA unicast đường thẳng
        if (pkt->getDestinationId() == self) {
            retryCount = 0;
            cancelTimer(5);
            WEBLOG("EVENT:RCV_DATA | Status:SUCCESS_RELAY | Node:" << self);
            preparePacket(netPkt);

            if (BpabTraCIManager::isNodeAtIntersection(self)) {

                double cX = mobilityModule->getLocation().x;
                double cY = mobilityModule->getLocation().y;

                int inDir = getIncomingBranch(this->srcX, this->srcY, cX, cY);

                WEBLOG("EVENT:INTERSECTION_LOGIC | Node:" << self
                       << " | ExcludeDir:" << dirName(inDir));

                incomingBranchDir = inDir;

                sendInterRTB();

            } else {
                transmissionDirection = this->srcDirection;
                sendRTB();
            }
        } else {
            delete netPkt;
        }
        break;
    }

    // ── BLACK BURST ───────────────────────────
    case BPAB_BLACK_BURST: {
        // Binary contention đường thẳng
        if (bpabMacState == BPAB_CONTENDING && !isTransmitting)
            heardBB = true;
        // Opposite contention (dùng heardBB cũng được vì cùng cơ chế)
        if (bpabMacState == BPAB_INTER_CONTENDING
            && myInterRole == INTER_ROLE_OPPOSITE && !isTransmitting)
            heardBB = true;
        // Cross contention
        if (bpabMacState == BPAB_INTER_CONTENDING
            && (myInterRole == INTER_ROLE_CROSS_A || myInterRole == INTER_ROLE_CROSS_B)
            && !crossIsTransmitting)
            crossHeardBB = true;
        break;
    }

    default: break;
    }
}

// ─────────────────────────────────────────────
// handleRadioControlMessage
// ─────────────────────────────────────────────
int BpabMac::handleRadioControlMessage(cMessage *msg) {
    RadioControlMessage *r = dynamic_cast<RadioControlMessage*>(msg);
    if (r && r->getRadioControlMessageKind() == CARRIER_SENSE_INTERRUPT) {
        if (bpabMacState == BPAB_CONTENDING && !isTransmitting)
            heardBB = true;
        if (bpabMacState == BPAB_INTER_CONTENDING) {
            if (myInterRole == INTER_ROLE_OPPOSITE && !isTransmitting)
                heardBB = true;
            if ((myInterRole == INTER_ROLE_CROSS_A || myInterRole == INTER_ROLE_CROSS_B)
                && !crossIsTransmitting)
                crossHeardBB = true;
        }
        return 1;
    }
    return 0;
}

// ─────────────────────────────────────────────
// timerFiredCallback
// ─────────────────────────────────────────────
void BpabMac::timerFiredCallback(int timerIndex) {
    switch (timerIndex) {

    // ── Timer 1: bắt đầu round binary ──
    case 1: {
        // Dùng cho cả BPAB_CONTENDING và BPAB_INTER_CONTENDING (opposite role)
        if (bpabMacState != BPAB_CONTENDING
            && !(bpabMacState == BPAB_INTER_CONTENDING
                 && myInterRole == INTER_ROLE_OPPOSITE)) break;

        if (currentIteration >= maxIterations) {
            endContention(true);
            break;
        }

        myX = mobilityModule->getLocation().x;
        myY = mobilityModule->getLocation().y;

        // Với opposite role, dùng srcDirection = myInterBranch
        int checkDir = (bpabMacState == BPAB_INTER_CONTENDING)
                       ? myInterBranch : srcDirection;
        double cX = (bpabMacState == BPAB_INTER_CONTENDING) ? interSrcX : srcX;
        double cY = (bpabMacState == BPAB_INTER_CONTENDING) ? interSrcY : srcY;

        if (!isValidForwardNode(myX, myY, cX, cY, checkDir, rangeR)) {
            endContention(false);
            break;
        }

        double mid = (limitL + limitU) / 2.0;
        heardBB = false;
        WEBLOG("EVENT:CONTENSION_ROUND | Round:" << currentIteration
               << " | Node:" << self << " | Range:[" << limitL << "," << limitU << "]");

        if (myDistanceToSrc > mid) {
            WEBLOG("EVENT:SEND_BB | Node:" << self << " | Dist:" << myDistanceToSrc);
            isTransmitting = true;
            toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));
            sendBlackBurst();
        } else {
            isTransmitting = false;
            toRadioLayer(createRadioCommand(SET_STATE, RX));
            toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));
        }
        setTimer(2, slotDuration);
        break;
    }

    // ── Timer 2: kết thúc round binary ──
    case 2: {
        if (bpabMacState != BPAB_CONTENDING
            && !(bpabMacState == BPAB_INTER_CONTENDING
                 && myInterRole == INTER_ROLE_OPPOSITE)) break;

        double mid = (limitL + limitU) / 2.0;
        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

        if (!isTransmitting) {
            if (heardBB) {
                WEBLOG("EVENT:DROP_OUT | Node:" << self << " | Reason:HEARD_BB");
                endContention(false);
                break;
            }
            limitU = mid;
            WEBLOG("EVENT:ITERATION_KEEP | Node:" << self
                   << " | Round:" << currentIteration << " | Action:SHRINK_UPPER");
        } else {
            limitL = mid;
            WEBLOG("EVENT:ITERATION_KEEP | Node:" << self
                   << " | Round:" << currentIteration << " | Action:SHRINK_LOWER");
        }

        currentIteration++;
        double delay = (slotStartTime + currentIteration * 2.0 * slotDuration)
                       - simTime().dbl();
        if (delay < slotDuration * 0.01) delay = slotDuration * 0.01;
        setTimer(1, delay);
        break;
    }

    // ── Timer 3: timeout RTB ──
    case 3: {
        if (bpabMacState != BPAB_WAIT_CTB) break;

        if (lastRTBDirection == INTER) {
            if (heardAnyCTB) {
                WEBLOG("EVENT:INTER_TIMEOUT_SEND_DATA | Node:" << self);
                sendInterBroadcastData();
            } else if (retryCount < maxRetries) {
                retryCount++;
                WEBLOG("EVENT:INTER_RETRY_RTB | Node:" << self << " | Retry:" << retryCount);
                sendInterRTB();
            } else {
                WEBLOG("EVENT:DROP_PKT | Node:" << self << " | Reason:INTER_NO_RELAY");
                if (packetToBroadcast) { delete packetToBroadcast; packetToBroadcast = NULL; }
                retryCount = 0; heardAnyCTB = false;
                bpabMacState = BPAB_IDLE;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
            }
            break;
        }

        if (packetToBroadcast && retryCount < maxRetries) {
            retryCount++;
            WEBLOG("EVENT:RETRY_RTB | Node:" << self << " | Retry:" << retryCount);
            sendRTB();
        } else {
            WEBLOG("EVENT:DROP_PKT | Node:" << self << " | Reason:EMPTY_BRANCH_TIMEOUT");
            if (packetToBroadcast) { delete packetToBroadcast; packetToBroadcast = NULL; }
            retryCount = 0;
            bpabMacState = BPAB_IDLE;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
        }
        break;
    }

    case 4: {
        toRadioLayer(createRadioCommand(SET_STATE, RX));
        break;
    }

    case 5: {
        if (bpabMacState != BPAB_WAIT_DATA) break;
        WEBLOG("EVENT:TIMEOUT | Node:" << self << " | Context:WAIT_DATA | Status:LOST_PACKET");
        bpabMacState = BPAB_IDLE;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
        break;
    }

    case 6: {
        if (bpabMacState != BPAB_PRE_CTB) break;
        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));
        if (heardCTB) {
            WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE | Reason:LOST_BACKOFF");
            heardCTB = false; bpabMacState = BPAB_IDLE; break;
        }
        WEBLOG("EVENT:SEND_CTB | From:" << self << " | To:" << this->srcId);
        sendCTB();
        break;
    }

    case 7: {
        if (bpabMacState == BPAB_IDLE && packetToBroadcast != NULL) {
            WEBLOG("EVENT:JITTER_SEND_RTB | Node:" << self
                   << " | Dir:" << dirName(transmissionDirection));
            sendRTB();
        } else {
            toRadioLayer(createRadioCommand(SET_STATE, RX));
        }
        break;
    }

    case 8: {
        if (bpabMacState != BPAB_WAIT_ACK) break;
        WEBLOG("EVENT:ACK_TIMEOUT | Node:" << self << " | Action:RESTART_CONTENTION");
        bpabMacState = BPAB_IDLE;
        if (packetToBroadcast) {
            retryCount = 0;
            WEBLOG("EVENT:RESTART | Node:" << self << " | Reason:NO_ACK_RECEIVED");
            cPacket *netPkt = packetToBroadcast->decapsulate();
            delete packetToBroadcast; packetToBroadcast = NULL;
            preparePacket(netPkt);
            sendRTB();
        }
        break;
    }

    case 9: {
        calculateTransmissionDirection();
        setTimer(9, 1.0);
        break;
    }

    // ── Timer 11: bắt đầu 1 mini-slot cross ──
    // Logic xen kẽ: CROSS_A phát ở slot lẻ, lắng nghe ở slot chẵn; CROSS_B ngược lại
    // crossIteration chạy từ 0 đến maxIterations-1
    // Slot lẻ/chẵn xác định bởi (crossIteration % 2)
    case 11: {
        if (bpabMacState != BPAB_INTER_CONTENDING) break;
        if (crossIteration >= maxIterations) {
            // Đã xong N vòng → node còn lại trong FA là winner → vào CW
            if (!crossWon) {
                WEBLOG("EVENT:INTER_CROSS_WINNER_CANDIDATE | Node:" << self
                       << " | Branch:" << dirName(myInterBranch)
                       << " | Iter:" << crossIteration);
                crossWon = true;
            }
            // Chờ SIFS trước khi vào CW
            double sifs = SIFS_SLOTS * slotDuration;
            setTimer(13, sifs);
            break;
        }

        // Xác định lượt phát trong slot này
        // CROSS_A phát khi crossIteration chẵn, CROSS_B phát khi lẻ
        bool myTurnToTransmit =
            (myInterRole == INTER_ROLE_CROSS_A && crossIteration % 2 == 0) ||
            (myInterRole == INTER_ROLE_CROSS_B && crossIteration % 2 == 1);

        double midCross = (limitLCross + limitUCross) / 2.0;
        crossHeardBB = false;

        WEBLOG("EVENT:INTER_CROSS_SLOT | Node:" << self
               << " | Branch:" << dirName(myInterBranch)
               << " | Iter:" << crossIteration
               << " | Range:[" << limitLCross << "," << limitUCross << "]"
               << " | MyTurn:" << myTurnToTransmit);

        if (myTurnToTransmit) {
            // Lượt phát BB
            if (myDistanceToSrc > midCross) {
                WEBLOG("EVENT:INTER_CROSS_SEND_BB | Node:" << self
                       << " | Dist:" << myDistanceToSrc);
                crossIsTransmitting = true;
                toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));
                sendBlackBurst();
            } else {
                crossIsTransmitting = false;
                toRadioLayer(createRadioCommand(SET_STATE, RX));
                toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));
            }
        } else {
            // Lượt lắng nghe
            crossIsTransmitting = false;
            toRadioLayer(createRadioCommand(SET_STATE, RX));
            toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));
        }
        setTimer(12, slotDuration);
        break;
    }

    // ── Timer 12: kết thúc mini-slot cross ──
    case 12: {
        if (bpabMacState != BPAB_INTER_CONTENDING) break;

        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

        bool myTurnToTransmit =
            (myInterRole == INTER_ROLE_CROSS_A && crossIteration % 2 == 0) ||
            (myInterRole == INTER_ROLE_CROSS_B && crossIteration % 2 == 1);

        double midCross = (limitLCross + limitUCross) / 2.0;

        if (myTurnToTransmit) {
            // Xử lý kết quả lượt phát
            if (crossIsTransmitting) {
                limitLCross = midCross;
                WEBLOG("EVENT:INTER_CROSS_KEEP | Node:" << self
                       << " | Round:" << crossIteration << " | Action:SHRINK_LOWER");
            } else {
                if (crossHeardBB) {
                    // Node xa hơn đã phát → mình thua
                    WEBLOG("EVENT:INTER_CROSS_DROP | Node:" << self
                           << " | Reason:HEARD_BB | Round:" << crossIteration);
                    bpabMacState = BPAB_IDLE;
                    WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
                    break;
                }
                limitUCross = midCross;
                WEBLOG("EVENT:INTER_CROSS_KEEP | Node:" << self
                       << " | Round:" << crossIteration << " | Action:SHRINK_UPPER");
            }
        }
        // Nếu không phải lượt phát: chỉ cập nhật iteration, không thay đổi range
        // (range của mình được cập nhật khi đến lượt phát)

        crossIteration++;
        setTimer(11, slotDuration * 0.01); // ngay lập tức sang slot tiếp
        break;
    }

    // ── Timer 13: SIFS trước CW contention ──
    case 13: {
        if (bpabMacState != BPAB_INTER_CONTENDING) break;

        // Random CW backoff
        int cwSlots   = CW_MIN + (int)(uniform(0, CW_MAX - CW_MIN));
        double backoff = cwSlots * slotDuration;
        WEBLOG("EVENT:INTER_CW_BACKOFF | Node:" << self
               << " | Branch:" << dirName(myInterBranch)
               << " | CW:" << cwSlots);

        toRadioLayer(createRadioCommand(SET_STATE, RX));
        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));
        setTimer(14, backoff);
        break;
    }

    // ── Timer 14: hết CW → phát CTB ──
    case 14: {
        if (bpabMacState != BPAB_INTER_CONTENDING) break;

        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

        WEBLOG("EVENT:INTER_SEND_CTB | Node:" << self
               << " | Branch:" << dirName(myInterBranch)
               << " | To:" << interSrcId);

        BPABPacket *ctb = new BPABPacket("BPAB_CTB_INTER", MAC_LAYER_PACKET);
        ctb->setBpabType(BPAB_CTB);
        ctb->setSourceId(self);
        ctb->setDestinationId(interSrcId);
        ctb->setDirection(INTER);
        ctb->setIncomingDir(myInterBranch); // encode nhánh để nguồn biết
        ctb->setByteLength(1);
        toRadioLayer(ctb);
        toRadioLayer(createRadioCommand(SET_STATE, TX));

        bpabMacState = BPAB_WAIT_DATA;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_DATA");

        // Timeout chờ DATA broadcast
        setTimer(5, interTotalTimeout(incomingBranchDir) + slotDuration * 10);
        break;
    }

    default: break;
    }
}

// ─────────────────────────────────────────────
// endContention — dùng cho cả binary thường và opposite role
// ─────────────────────────────────────────────
void BpabMac::endContention(bool won) {
    cancelTimer(1); cancelTimer(2);
    toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

    if (won) {
        toRadioLayer(createRadioCommand(SET_STATE, RX));
        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));

        if (bpabMacState == BPAB_INTER_CONTENDING) {
            // Opposite role thắng → chờ 2Nτ rồi phát CTB
            double waitTime = 2.0 * maxIterations * slotDuration;
            WEBLOG("EVENT:INTER_OPPOSITE_WINNER | Node:" << self
                   << " | Branch:" << dirName(myInterBranch)
                   << " | Wait2N:" << waitTime);
            // Dùng timer 13 (SIFS path) cho opposite: trực tiếp vào CW
            setTimer(13, waitTime);
        } else {
            double backoff = uniform(slotDuration, slotDuration * 3.0);
            WEBLOG("EVENT:WINNER | Node:" << self << " | Backoff:" << backoff << " | Type:NORMAL");
            bpabMacState = BPAB_PRE_CTB;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:PRE_CTB");
            setTimer(6, backoff);
        }
    } else {
        if (bpabMacState != BPAB_INTER_CONTENDING) {
            if (packetToBroadcast) { delete packetToBroadcast; packetToBroadcast = NULL; }
        }
        bpabMacState = BPAB_IDLE;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
    }
    currentIteration = 0; heardBB = false; isTransmitting = false;
}

// ─────────────────────────────────────────────
// sendInterRTB — 1 RTB INTER duy nhất
// ─────────────────────────────────────────────
void BpabMac::sendInterRTB() {
    BPABPacket *rtb = new BPABPacket("BPAB_RTB_INTER", MAC_LAYER_PACKET);
    rtb->setBpabType(BPAB_RTB);
    rtb->setSourceId(self);
    rtb->setRtbSentTime(simTime().dbl());
    rtb->setSourceX(mobilityModule->getLocation().x);
    rtb->setSourceY(mobilityModule->getLocation().y);
    rtb->setDirection(INTER);
    rtb->setIncomingDir(incomingBranchDir); // -1 nếu nguồn tại giao lộ
    rtb->setLimitL(0);
    rtb->setLimitU(rangeR);
    rtb->setByteLength(10);

    lastRTBDirection = INTER;
    heardAnyCTB      = false;
    bpabMacState     = BPAB_WAIT_CTB;

    toRadioLayer(rtb);
    toRadioLayer(createRadioCommand(SET_STATE, TX));

    WEBLOG("EVENT:SEND_RTB_INTER | Node:" << self
           << " | ExcludeDir:" << dirName(incomingBranchDir));
    WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_CTB");

    setTimer(4, slotDuration * 0.1);
    setTimer(3, interTotalTimeout(incomingBranchDir));
}

// ─────────────────────────────────────────────
// sendInterBroadcastData — 1 DATA broadcast
// ─────────────────────────────────────────────
void BpabMac::sendInterBroadcastData() {
    if (!packetToBroadcast) return;

    WEBLOG("EVENT:INTER_BROADCAST_DATA | Node:" << self);
    packetToBroadcast->setDestinationId(BROADCAST_INTER);
    packetToBroadcast->setBpabType(BPAB_DATA);

    toRadioLayer(packetToBroadcast->dup());
    toRadioLayer(createRadioCommand(SET_STATE, TX));

    delete packetToBroadcast;
    packetToBroadcast = NULL;
    heardAnyCTB       = false;
    retryCount        = 0;
    bpabMacState      = BPAB_IDLE;
    WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
}

// ─────────────────────────────────────────────
// Các hàm không thay đổi
// ─────────────────────────────────────────────
bool BpabMac::isValidForwardNode(double myX, double myY,
                                  double srcX, double srcY,
                                  int direction, double rangeR) {
    double dx = myX - srcX, dy = myY - srcY;
    double adx = fabs(dx), ady = fabs(dy);
    double halfWidth = widthW / 2.0;

    switch (direction) {
        case EAST:
            if (dx > minProgress && dx <= rangeR && ady <= halfWidth)
                { myDistanceToSrc = dx; return true; }
            break;
        case WEST:
            if (dx < -minProgress && adx <= rangeR && ady <= halfWidth)
                { myDistanceToSrc = adx; return true; }
            break;
        case NORTH:
            if (dy > minProgress && dy <= rangeR && adx <= halfWidth)
                { myDistanceToSrc = dy; return true; }
            break;
        case SOUTH:
            if (dy < -minProgress && ady <= rangeR && adx <= halfWidth)
                { myDistanceToSrc = ady; return true; }
            break;
        case INTER: {
            double d = sqrt(dx*dx + dy*dy);
            if (d > minProgress && d <= rangeR)
                { myDistanceToSrc = d; return true; }
            break;
        }
        default: return false;
    }
    return false;
}

void BpabMac::preparePacket(cPacket *netPkt) {
    BPABPacket *pkt = new BPABPacket("BPAB_DATA", MAC_LAYER_PACKET);
    pkt->setByteLength(netPkt->getByteLength());
    pkt->setPayload("BPAB_PAYLOAD");
    pkt->encapsulate(netPkt);
    packetToBroadcast = pkt;
}

void BpabMac::sendRTB() {
    BPABPacket *rtbPkt = new BPABPacket("BPAB_RTB", MAC_LAYER_PACKET);
    rtbPkt->setBpabType(BPAB_RTB);
    rtbPkt->setSourceId(self);
    rtbPkt->setRtbSentTime(simTime().dbl());
    rtbPkt->setSourceX(mobilityModule->getLocation().x);
    rtbPkt->setSourceY(mobilityModule->getLocation().y);
    rtbPkt->setDirection(transmissionDirection);
    rtbPkt->setLimitL(0);
    rtbPkt->setLimitU(rangeR);
    rtbPkt->setByteLength(10);

    lastRTBDirection = transmissionDirection;
    bpabMacState     = BPAB_WAIT_CTB;
    toRadioLayer(rtbPkt);
    toRadioLayer(createRadioCommand(SET_STATE, TX));
    WEBLOG("EVENT:SEND_RTB | From:" << self << " | Dir:" << dirName(transmissionDirection));
    WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_CTB");

    setTimer(4, slotDuration * 0.1);
    setTimer(3, slotDuration * (2 * maxIterations + 20));
}

void BpabMac::sendBlackBurst() {
    BPABPacket *bb = new BPABPacket("BLACK_BURST", MAC_LAYER_PACKET);
    bb->setBpabType(BPAB_BLACK_BURST);
    bb->setByteLength(1);
    toRadioLayer(bb);
    toRadioLayer(createRadioCommand(SET_STATE, TX));
}

void BpabMac::sendCTB() {
    BPABPacket *ctb = new BPABPacket("BPAB_CTB", MAC_LAYER_PACKET);
    ctb->setBpabType(BPAB_CTB);
    ctb->setSourceId(self);
    ctb->setDestinationId(this->srcId);
    ctb->setByteLength(1);
    toRadioLayer(ctb);
    toRadioLayer(createRadioCommand(SET_STATE, TX));
    bpabMacState = BPAB_WAIT_DATA;
    WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_DATA");
    setTimer(4, slotDuration * 0.1);
    setTimer(5, slotDuration * 10);
}

void BpabMac::sendData(int winnerId) {
    WEBLOG("EVENT:SEND_DATA | From:" << self << " | To:" << winnerId);
    lastDataDestId = winnerId;
    packetToBroadcast->setDestinationId(winnerId);
    packetToBroadcast->setBpabType(BPAB_DATA);
    toRadioLayer(packetToBroadcast->dup());
    toRadioLayer(createRadioCommand(SET_STATE, TX));
    bpabMacState = BPAB_WAIT_ACK;
    WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_ACK");
    setTimer(8, slotDuration * 20);
}

int BpabMac::calculateTransmissionDirection() {
    myX = mobilityModule->getLocation().x;
    myY = mobilityModule->getLocation().y;
    double dx = myX - lastX, dy = myY - lastY;
    if (fabs(dx) > 0.01 || fabs(dy) > 0.01) {
        if (fabs(dx) > fabs(dy))
            transmissionDirection = (dx > 0) ? WEST : EAST;
        else
            transmissionDirection = (dy > 0) ? SOUTH : NORTH;
    }
    lastX = myX; lastY = myY;
    return transmissionDirection;
}

int BpabMac::getIncomingBranch(double sX, double sY, double mX, double mY) {
    double dx = sX - mX, dy = sY - mY;
    if (fabs(dx) > fabs(dy))
        return (dx > 0) ? EAST : WEST;
    else
        return (dy > 0) ? NORTH : SOUTH;
}
