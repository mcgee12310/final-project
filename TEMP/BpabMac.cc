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

// Chuyển direction int → chuỗi log
static const char* dirName(int d) {
    switch (d) {
        case EAST:  return "EAST";
        case WEST:  return "WEST";
        case NORTH: return "NORTH";
        case SOUTH: return "SOUTH";
        case INTER: return "INTER_ALL_BRANCHES";
        default:    return "UNKNOWN";
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
    minProgress   = hasPar("minProgress") ? (double)par("minProgress") : 10.0;
    slotDuration  = par("slotDuration");
    maxRetries    = par("maxRetries");

    retryCount       = 0;
    bpabMacState     = BPAB_IDLE;
    currentIteration = 0;
    heardBB          = false;
    heardCTB         = false;
    heardAnyCTB      = false;
    isTransmitting   = false;
    packetToBroadcast = NULL;

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

    // Nếu Node Nguồn nằm tại giao lộ, phát tỏa 4 hướng (-1 nghĩa là không loại trừ hướng nào)
    if (BpabTraCIManager::isNodeAtIntersection(self)) {
        WEBLOG("EVENT:INIT_INTER_SOURCE | Node:" << self << " | Action:START_MULTI_DIR");
        sendInterRTB(-1);
    } else {
        WEBLOG("EVENT:INIT_SOURCE | Node:" << self << " | CalcDir:" << dirName(transmissionDirection));
        transmissionDirection = calculateTransmissionDirection();
        sendRTB();
    }
}

// ─────────────────────────────────────────────
// Hàm phụ trợ giao lộ (Toán học Không gian & Thời gian)
// ─────────────────────────────────────────────

// Xác định nhánh của Node so với tâm giao lộ
int BpabMac::getInterBranch(double mX, double mY, double sX, double sY) {
    double dx = mX - sX;
    double dy = mY - sY;
    if (fabs(dx) >= fabs(dy)) return (dx > 0) ? EAST : WEST;
    return (dy > 0) ? NORTH : SOUTH;
}

// Xác định vùng tranh chấp (Zone) theo bán kính
int BpabMac::getInterZone(double dist) {
    double r3 = rangeR / 3.0;
    if (dist > 2.0 * r3) return 0;   // Vùng ngoài cùng (Outer)
    if (dist > r3)       return 1;   // Vùng giữa (Mid)
    return 2;                        // Vùng trong cùng (Inner)
}

// Tính toán thời gian bắt đầu của nhánh trong lịch xoay vòng
int BpabMac::getBranchSlotOffset(int branch, int excludeDir) {
    int dirs[4] = { NORTH, EAST, SOUTH, WEST };
    int idx = 0;
    for (int i = 0; i < 4; i++) {
        if (dirs[i] == excludeDir) continue;
        if (dirs[i] == branch) return idx * 12; // (3 vùng * 4 slots)
        idx++;
    }
    return 0;
}

// Tính tổng thời gian timeout cho Node giao lộ đợi tất cả các nhánh
double BpabMac::interTotalTimeout(int excludeDir) {
    int numBranches = (excludeDir == -1) ? 4 : 3;
    // (Số nhánh * 6 slots/nhánh) + 4 slots dự phòng
    return slotDuration * (numBranches * 12 + 4);
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

        // ── RTB TẠI GIAO LỘ ──
        if (dir == INTER) {
            double dist = sqrt((mX-sX)*(mX-sX) + (mY-sY)*(mY-sY));
            if (dist < minProgress || dist > rangeR) return;

            int excludeDir = pkt->getIncomingDir();
            int myBranch   = getInterBranch(mX, mY, sX, sY);
            if (myBranch == excludeDir) return;
            if (!isValidForwardNode(mX, mY, sX, sY, myBranch, rangeR)) return;

            int myZone = getInterZone(dist);

            interSrcId        = pkt->getSourceId();
            interSrcX         = sX;
            interSrcY         = sY;
            myInterBranch     = myBranch;
            incomingBranchDir = excludeDir;
            interZonePhase    = 0;
            heardBBInZone     = false;
            isTransmitting    = false;

            bpabMacState = BPAB_INTER_CONTENDING;
            WEBLOG("EVENT:INTER_JOIN | Node:" << self << " | Branch:" << dirName(myBranch)
                               << " | ZonePhase:" << myZone << " | Dist:" << dist);
            WEBLOG("EVENT:STATE | Node:" << self << " | State:CONTENDING");

            int branchSlotOffset = getBranchSlotOffset(myBranch, excludeDir);
            double now        = simTime().dbl();
            double rtbTime    = pkt->getRtbSentTime();
            double zoneStart  = rtbTime + slotDuration * (branchSlotOffset + 1);

            while (zoneStart <= now + slotDuration * 0.01) zoneStart += slotDuration;

            interSlotStart = zoneStart;
            myInterZone    = myZone;

            setTimer(11, zoneStart - now);
            break;
        }

        // ── RTB ĐƯỜNG THẲNG BÌNH THƯỜNG ──
        if (!isValidForwardNode(mX, mY, sX, sY, dir, rangeR)) return;

        if (BpabTraCIManager::isNodeAtIntersection(self)) {
            // Ngăn chặn Fast-track nếu xe gửi (nguồn) CŨNG nằm trong giao lộ
            // (Đề phòng xe đang đi từ giao lộ ra bị giật ngược lại)
            if (!BpabTraCIManager::isNodeAtIntersection(pkt->getSourceId())) {
                WEBLOG("EVENT:INTERSECTION_FAST_TRACK | Node:" << self << " | Action:PREEMPT_BEFORE_BB");

                srcId = pkt->getSourceId();
                this->srcX = sX;
                this->srcY = sY;
                this->srcDirection = dir; // Kế thừa hướng để lát tính nhánh bị loại trừ (excludeDir)

                cancelTimer(1); cancelTimer(2);
                toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

                heardCTB = false;
                bpabMacState = BPAB_PRE_CTB;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:PRE_CTB");

                // Hẹn giờ phát CTB cực sớm: [0.0 đến 0.3 * slotDuration]
                double interBackoff = uniform(0.0, slotDuration * 0.3);
                setTimer(6, interBackoff);
                WEBLOG("EVENT:WINNER | Node:" << self << " | Backoff:" << interBackoff << " | Type:INTERSECTION_PREEMPT");
                return; // Thoát ngay, KHÔNG chạy xuống logic chia nhị phân của xe thường!
            }
        }

        // Tiếp tục logic cho xe thường tham gia chia nhị phân
        WEBLOG("EVENT:JOIN_BPAB | Node:" << self << " | Dir:" << dirName(dir) << " | Src:" << pkt->getSourceId());
        srcId = pkt->getSourceId(); this->srcX = sX; this->srcY = sY;
        limitL = 0; limitU = rangeR;
        this->srcDirection = dir;
        currentIteration = 0; heardBB = false; isTransmitting = false;
        bpabMacState = BPAB_CONTENDING;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:CONTENDING");

        double now = simTime().dbl();
        double boundary = pkt->getRtbSentTime() + slotDuration;
        while (boundary <= now + slotDuration * 0.01) boundary += slotDuration;
        slotStartTime = boundary;
        setTimer(1, slotStartTime - now);
        break;
    }

    // ── CTB ──────────────────────────────────
    case BPAB_CTB: {
        int winnerId = pkt->getSourceId();

        if (bpabMacState == BPAB_PRE_CTB) {
            WEBLOG("EVENT:BACKDOWN | Node:" << self << " | Reason:HEARD_OTHER_CTB | Flow:" << pkt->getDestinationId());
            heardCTB = true;
            break;
        }

        if (bpabMacState == BPAB_CONTENDING) {
            cancelTimer(1); cancelTimer(2);
            WEBLOG("EVENT:DROP_OUT | Node:" << self << " | Reason:HEARD_PREEMPTIVE_CTB | Flow:" << pkt->getDestinationId());
            bpabMacState = BPAB_IDLE;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
            break;
        }

        if (bpabMacState == BPAB_INTER_CONTENDING) {
            // Nếu nghe được CTB giao lộ gửi về cho CÙNG một tâm ngã tư của mình
            if (pkt->getDestinationId() == this->interSrcId && pkt->getDirection() == INTER) {

                // CHỈ DROP_OUT nếu gói CTB này đến từ một xe thuộc CÙNG NHÁNH với mình
                if (pkt->getIncomingDir() == this->myInterBranch) {
                    cancelTimer(11); cancelTimer(12); cancelTimer(13);
                    WEBLOG("EVENT:DROP_OUT | Node:" << self << " | Reason:HEARD_OTHER_CTB_IN_SAME_BRANCH | Branch:" << dirName(myInterBranch));
                    bpabMacState = BPAB_IDLE;
                    WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
                    break;
                }
            }
        }

        // ── Node Giao lộ nhận được CTB từ các nhánh ──
        if (bpabMacState == BPAB_WAIT_CTB && lastRTBDirection == INTER) {
            heardAnyCTB = true;
            WEBLOG("EVENT:INTER_HEARD_CTB | Node:" << self << " | From:" << winnerId);
            break;
        }

        // Node đường thẳng nhận CTB
        if (bpabMacState == BPAB_WAIT_CTB && packetToBroadcast) {
            if (pkt->getDestinationId() == self) {
                WEBLOG("EVENT:RCV_CTB | Node:" << self << " | From:" << winnerId << " | Action:PREPARE_DATA");
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

        // ── Nhận DATA Giao lộ (Broadcast cho mọi nhánh thắng) ──
        if (pkt->getDestinationId() == BROADCAST_INTER) {
            if (bpabMacState != BPAB_WAIT_DATA) {
                delete netPkt;
                break;
            }
            cancelTimer(5); // Tắt còi báo rớt gói
            WEBLOG("EVENT:INTER_RCV_DATA | Node:" << self << " | Branch:" << dirName(myInterBranch));

            bpabMacState = BPAB_IDLE;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
            preparePacket(netPkt);

            // Tiếp tục gửi RTB đường thẳng theo hướng nhánh của nó
            transmissionDirection = myInterBranch;
            double jitterDelay = uniform(slotDuration * 0.1, slotDuration * 2.5);
            WEBLOG("EVENT:INTER_RELAY_JITTER | Node:" << self << " | Dir:" << dirName(transmissionDirection) << " | Jitter:" << jitterDelay);

            setTimer(7, jitterDelay); // Giao phó cho Timer 4 bóp cò
            break;
        }

        // ── Nhận DATA Đường thẳng (Unicast) ──
        if (pkt->getDestinationId() == self) {
            retryCount = 0;
            cancelTimer(5);
            WEBLOG("EVENT:RCV_DATA | Status:SUCCESS_RELAY | Node:" << self);
            preparePacket(netPkt);

            if (BpabTraCIManager::isNodeAtIntersection(self)) {
                // =================================================================
                // [FIX LỖI LẶP VÔ TẬN]: Chỉ phát Đa Hướng nếu nguồn ở NGOÀI giao lộ
                // =================================================================
                if (!BpabTraCIManager::isNodeAtIntersection(this->srcId)) {
                    double cX = mobilityModule->getLocation().x;
                    double cY = mobilityModule->getLocation().y;
                    int inDir = getIncomingBranch(this->srcX, this->srcY, cX, cY);

                    WEBLOG("EVENT:INTERSECTION_LOGIC | Node:" << self << " | ExcludeBranch:" << dirName(inDir) << " | Action:MULTI_DIR_BROADCAST");
                    sendInterRTB(inDir);
                } else {
                    // Nếu nguồn cũng ở trong giao lộ (tức là gói tin đang đi ra ngoài)
                    // -> Tiếp tục truyền thẳng, tránh việc kích hoạt đa hướng lần 2
                    WEBLOG("EVENT:INTERSECTION_BYPASS | Node:" << self << " | Reason:SENDER_ALREADY_IN_INTERSECTION_KEEP_STRAIGHT");
                    transmissionDirection = this->srcDirection;
                    sendRTB();
                }
            } else {
                // Kế thừa hướng
                WEBLOG("EVENT:NORMAL_RELAY | Node:" << self << " | Dir:" << dirName(transmissionDirection));
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
        if (bpabMacState == BPAB_CONTENDING && !isTransmitting) {
            heardBB = true;
        }
        if (bpabMacState == BPAB_INTER_CONTENDING && !isTransmitting) {
            heardBBInZone = true;
        }
        break;
    }

    default: break;
    }
}

// ─────────────────────────────────────────────
// handleRadioControlMessage
// ─────────────────────────────────────────────
int BpabMac::handleRadioControlMessage(cMessage *msg) {
    RadioControlMessage *radioMsg = dynamic_cast<RadioControlMessage*>(msg);
    if (radioMsg && radioMsg->getRadioControlMessageKind() == CARRIER_SENSE_INTERRUPT) {
        if (bpabMacState == BPAB_CONTENDING && !isTransmitting) heardBB = true;
        if (bpabMacState == BPAB_INTER_CONTENDING && !isTransmitting) heardBBInZone = true;
        return 1;
    }
    return 0;
}

// ─────────────────────────────────────────────
// timerFiredCallback
// ─────────────────────────────────────────────
void BpabMac::timerFiredCallback(int timerIndex) {
    switch (timerIndex) {

    // ── Timer 1 & 2: Binary Contention (Đường thẳng) ──
    case 1: {
        if (bpabMacState != BPAB_CONTENDING) break;
        if (currentIteration >= maxIterations) { endContention(true); break; }

        myX = mobilityModule->getLocation().x;
        myY = mobilityModule->getLocation().y;
        double mid = (limitL + limitU) / 2.0;
        heardBB = false;

        WEBLOG("EVENT:CONTENSION_ROUND | Round:" << currentIteration << " | Node:" << self << " | Range:[" << limitL << "," << limitU << "]");

        if (myDistanceToSrc > mid) {
            WEBLOG("EVENT:SEND_BB | Node:" << self << " | Dist:" << myDistanceToSrc << " | Flow:" << this->srcId);
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
    case 2: {
        if (bpabMacState != BPAB_CONTENDING) break;
        double mid = (limitL + limitU) / 2.0;
        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

        if (!isTransmitting) {
            if (heardBB) {
                WEBLOG("EVENT:DROP_OUT | Node:" << self << " | Reason:HEARD_BB");
                endContention(false);
                break;
            }
            limitU = mid;
            WEBLOG("EVENT:ITERATION_KEEP | Node:" << self << " | Round:" << currentIteration << " | Action:SHRINK_UPPER");
        } else {
            limitL = mid;
            WEBLOG("EVENT:ITERATION_KEEP | Node:" << self << " | Round:" << currentIteration << " | Action:SHRINK_LOWER");
        }
        currentIteration++;
        double delay = (slotStartTime + currentIteration * 2.0 * slotDuration) - simTime().dbl();
        if (delay < slotDuration * 0.01) delay = slotDuration * 0.01;
        setTimer(1, delay);
        break;
    }

    // ── Timer 3: Timeout chờ CTB (Đường thẳng & Giao lộ) ──
    case 3: {
        if (bpabMacState != BPAB_WAIT_CTB) break;

        // Xử lý Timeout Giao lộ
        if (lastRTBDirection == INTER) {
            if (heardAnyCTB) {
                // Nếu nghe được bất kỳ CTB nào -> Phát DATA Broadcast
                WEBLOG("EVENT:INTER_TIMEOUT | Action:SENDING_BROADCAST_DATA_TO_ALL_WINNERS");
                sendInterBroadcastData();
            } else {
                // Không có ai -> Phát lại RTB Giao lộ
                if (retryCount < maxRetries) {
                    retryCount++;
                    WEBLOG("EVENT:INTER_RETRY_RTB | Node:" << self << " | Retry:" << retryCount);
                    sendInterRTB(incomingBranchDir);
                } else {
                    WEBLOG("EVENT:DROP_PKT | Node:" << self << " | Reason:INTER_NO_RELAY");
                    if (packetToBroadcast) { delete packetToBroadcast; packetToBroadcast = NULL; }
                    retryCount = 0; heardAnyCTB = false; bpabMacState = BPAB_IDLE;
                    WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
                }
            }
            break;
        }

        // Xử lý Timeout Đường thẳng
        if (packetToBroadcast && retryCount < maxRetries) {
            retryCount++;
            WEBLOG("EVENT:RETRY_RTB | Node:" << self << " | Retry:" << retryCount);
            sendRTB();
        } else {
            WEBLOG("EVENT:DROP_PKT | Node:" << self << " | Reason:EMPTY_BRANCH_TIMEOUT");
            if (packetToBroadcast) { delete packetToBroadcast; packetToBroadcast = NULL; }
            retryCount = 0; bpabMacState = BPAB_IDLE;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
        }
        break;
    }

    case 4:{
        toRadioLayer(createRadioCommand(SET_STATE, RX));
        break;
    }

    case 5: {
        if (bpabMacState != BPAB_WAIT_DATA) break;
        WEBLOG("EVENT:TIMEOUT | Node:" << self << " | Context:WAIT_DATA | Status:LOST_PACKET");
        bpabMacState = BPAB_IDLE;
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

    case 7:{
        if (bpabMacState == BPAB_IDLE && packetToBroadcast != NULL) {
            WEBLOG("EVENT:JITTER_EXPIRED_SEND_RTB | Node:" << self << " | Dir:" << dirName(transmissionDirection));
            sendRTB();
        } else {
            // Trả lại chức năng cũ của Castalia: Chuyển Radio sang trạng thái Nhận
            toRadioLayer(createRadioCommand(SET_STATE, RX));
        }
        break;
    }

    case 8: {
        if (bpabMacState != BPAB_WAIT_ACK) break;
        WEBLOG("EVENT:ACK_TIMEOUT | Node:" << self << " | Action:RESTART_CONTENTION");
        bpabMacState = BPAB_IDLE;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
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

    // ── Timer 11: Zone-slot Phân rã Vòng Cung (Giao lộ) ──
    case 11: {
        if (bpabMacState != BPAB_INTER_CONTENDING) break;

        WEBLOG("EVENT:INTER_ZONE_SLOT | Node:" << self << " | Branch:" << dirName(myInterBranch)
               << " | Phase:" << interZonePhase << " | MyZone:" << myInterZone << " | HeardBB:" << heardBBInZone);

        if (interZonePhase == 0) { // Outer
            if (myInterZone == 0) {
                WEBLOG("EVENT:INTER_SEND_BB | Node:" << self << " | Zone:Outer | Flow:" << interSrcId);
                isTransmitting = true;
                toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));
                sendBlackBurst();

                // [THAY ĐỔI]: Dùng Timer 13 để chờ BB phát xong (0.1 slot) rồi mới chuyển sang RX
                setTimer(13, slotDuration * 0.1);
            } else {
                isTransmitting = false;
                heardBBInZone = false;
                toRadioLayer(createRadioCommand(SET_STATE, RX));
                toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));

                interZonePhase = 1;
                setTimer(11, slotDuration * 4);
            }
        }
        else if (interZonePhase == 1) { // Mid
            if (heardBBInZone) {
                WEBLOG("EVENT:INTER_DROP | Node:" << self << " | Reason:OUTER_WON");
                bpabMacState = BPAB_IDLE;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
                break;
            }
            if (myInterZone == 1) {
                WEBLOG("EVENT:INTER_SEND_BB | Node:" << self << " | Zone:Mid | Flow:" << interSrcId);
                isTransmitting = true;
                toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));
                sendBlackBurst();

                setTimer(13, slotDuration * 0.1); // [THAY ĐỔI]
            } else {
                heardBBInZone = false;
                toRadioLayer(createRadioCommand(SET_STATE, RX));
                toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));

                interZonePhase = 2;
                setTimer(11, slotDuration * 4);
            }
        }
        else { // Inner
            if (heardBBInZone) {
                WEBLOG("EVENT:INTER_DROP | Node:" << self << " | Reason:MID_WON");
                bpabMacState = BPAB_IDLE;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
                break;
            }
            if (myInterZone == 2) {
                WEBLOG("EVENT:INTER_SEND_BB | Node:" << self << " | Zone:Inner (Default Win) | Flow:" << interSrcId);
                isTransmitting = true;
                toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));
                sendBlackBurst();

                setTimer(13, slotDuration * 0.1); // [THAY ĐỔI]
            } else {
                WEBLOG("EVENT:INTER_DROP | Node:" << self << " | Reason:NO_ZONES_LEFT");
                bpabMacState = BPAB_IDLE;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
            }
        }
        break;
    }

    // ── Timer 12: Gửi CTB ngay sau BlackBurst của Vùng Vòng Cung ──
    case 12: {
        if (bpabMacState != BPAB_INTER_CONTENDING) break;

        WEBLOG("EVENT:SEND_CTB | Node:" << self << " | Branch:" << dirName(myInterBranch));

        BPABPacket *ctb = new BPABPacket("BPAB_CTB_INTER", MAC_LAYER_PACKET);
        ctb->setBpabType(BPAB_CTB);
        ctb->setSourceId(self);
        ctb->setDestinationId(interSrcId);
        ctb->setDirection(INTER);
        ctb->setIncomingDir(myInterBranch);
        ctb->setByteLength(1);
        toRadioLayer(ctb);
        toRadioLayer(createRadioCommand(SET_STATE, TX));

        bpabMacState = BPAB_WAIT_DATA;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_DATA");

        // Thời gian đợi DATA đủ dài để Node nguồn nghe CTB từ TẤT CẢ các nhánh còn lại
        setTimer(5, interTotalTimeout(incomingBranchDir) + slotDuration * 10);
        break;
    }

    case 13: {
        if (bpabMacState != BPAB_INTER_CONTENDING) break;

        // Kết thúc BB, lập tức mở chế độ Nhận (RX) để có thể nghe CTB của xe khác
        isTransmitting = false;
        toRadioLayer(createRadioCommand(SET_STATE, RX));
        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));

        // Random Backoff (0.01 -> 0.8 slotDuration). Ai số nhỏ sẽ phát CTB trước.
        double backoff = uniform(slotDuration, slotDuration * 3.0);
        WEBLOG("EVENT:INTER_BACKOFF | Node:" << self << " | Value:" << backoff);

        setTimer(12, backoff);
        break;
    }

    default: break;
    }
}

// ─────────────────────────────────────────────
// Phát lệnh điều phối Ngã Tư (Multi-Directional)
// ─────────────────────────────────────────────
void BpabMac::sendInterRTB(int excludeDir) {
    incomingBranchDir = excludeDir;

    BPABPacket *rtb = new BPABPacket("BPAB_RTB_INTER", MAC_LAYER_PACKET);
    rtb->setBpabType(BPAB_RTB);
    rtb->setSourceId(self);
    rtb->setRtbSentTime(simTime().dbl());
    rtb->setSourceX(mobilityModule->getLocation().x);
    rtb->setSourceY(mobilityModule->getLocation().y);

    rtb->setDirection(INTER);
    rtb->setIncomingDir(excludeDir);
    rtb->setLimitL(0);
    rtb->setLimitU(rangeR);
    rtb->setByteLength(10);

    lastRTBDirection = INTER;
    heardAnyCTB      = false;
    bpabMacState     = BPAB_WAIT_CTB;

    toRadioLayer(rtb);
    toRadioLayer(createRadioCommand(SET_STATE, TX));

    WEBLOG("EVENT:SEND_RTB | Node:" << self << " | ExcludeDir:" << dirName(excludeDir));
    WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_CTB");

    setTimer(4, slotDuration * 0.1);
    // Timeout tổng = Thời gian cần cho 3 (hoặc 4) nhánh chạy xong tranh chấp Radial Zoning
    setTimer(3, interTotalTimeout(excludeDir));
}

// ─────────────────────────────────────────────
// Phát gói DATA duy nhất cho các Relay ở mọi nhánh
// ─────────────────────────────────────────────
void BpabMac::sendInterBroadcastData() {
    if (!packetToBroadcast) return;

    WEBLOG("EVENT:SEND_DATA | Node:" << self);

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
// Các hàm tiện ích logic
// ─────────────────────────────────────────────
bool BpabMac::isValidForwardNode(double myX, double myY, double srcX, double srcY, int direction, double rangeR) {
    double dx = myX - srcX, dy = myY - srcY;
    double adx = fabs(dx), ady = fabs(dy);
    double halfWidth = widthW / 2.0;

    switch (direction) {
        case EAST:  if (dx > minProgress && dx <= rangeR && ady <= halfWidth) { myDistanceToSrc = dx; return true; } break;
        case WEST:  if (dx < -minProgress && adx <= rangeR && ady <= halfWidth) { myDistanceToSrc = adx; return true; } break;
        case NORTH: if (dy > minProgress && dy <= rangeR && adx <= halfWidth) { myDistanceToSrc = dy; return true; } break;
        case SOUTH: if (dy < -minProgress && ady <= rangeR && adx <= halfWidth) { myDistanceToSrc = ady; return true; } break;
        case INTER: {
            double d = sqrt(dx*dx + dy*dy);
            if (d > minProgress && d <= rangeR) { myDistanceToSrc = d; return true; }
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

void BpabMac::endContention(bool won) {
    cancelTimer(1); cancelTimer(2);
    toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

    if (won) {
        toRadioLayer(createRadioCommand(SET_STATE, RX));
        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));
        double backoff = uniform(slotDuration, slotDuration * 3.0);
        WEBLOG("EVENT:WINNER | Node:" << self << " | Backoff:" << backoff << " | Type:NORMAL");
        bpabMacState = BPAB_PRE_CTB;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:PRE_CTB");
        setTimer(6, backoff);
    } else {
        if (packetToBroadcast) { delete packetToBroadcast; packetToBroadcast = NULL; }
        bpabMacState = BPAB_IDLE;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
    }
    currentIteration = 0; heardBB = false; isTransmitting = false;
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
