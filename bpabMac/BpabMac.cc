#include "BpabMac.h"
#include <omnetpp.h>
#include <sstream>
#include "BpabTraCIManager.h"

Define_Module(BpabMac);

#define WEBLOG(msgArgs) \
    do { \
        std::ostringstream _ss; \
        _ss << msgArgs; \
        trace() << _ss.str(); \
        /* --- GHI VÀO FILE LOG CHUNG --- */ \
        BpabTraCIManager::writeToUnifiedLog(simTime().dbl(), self, "MAC_EVENT", _ss.str()); \
        /* --- GỬI SANG BIẾN STATIC CỦA TRACIMANAGER (CŨ) --- */ \
        if (BpabTraCIManager::tcpClientSocket != -1) { \
            std::ostringstream _netSs; \
            _netSs << simTime().dbl() << " MAC " << _ss.str() << "\n"; \
            std::string _msg = _netSs.str(); \
            ::send(BpabTraCIManager::tcpClientSocket, _msg.c_str(), _msg.length(), 0); \
        } \
    } while(0)

void BpabMac::startup() {
    VirtualMac::startup();

    cModule* node = getParentModule()->getParentModule();
    cModule* mobility = node->getSubmodule("MobilityManager");

    if (!mobility) {
        error("MobilityManager not found!");
    }

    mobilityModule = check_and_cast<VirtualMobilityManager*>(mobility);

    maxIterations = par("maxIterations");
    rangeR = par("rangeR");
    if (hasPar("widthW")) {
        widthW = par("widthW");
    } else {
        widthW = 50.0;
    }
    if (hasPar("minProgress")) {
        minProgress = par("minProgress");
    } else {
        minProgress = 10.0;
    }
    slotDuration = par("slotDuration");

    retryCount = 0;
    maxRetries = par("maxRetries");

    bpabMacState = BPAB_IDLE;
    currentIteration = 0;

    heardBB = false;
    heardCTB = false;
    isTransmitting = false;

    packetToBroadcast = NULL;

    myX = mobilityModule->getLocation().x;
    myY = mobilityModule->getLocation().y;
    lastX = myX;
    lastY = myY;
    transmissionDirection = EAST;
    setTimer(9, 1.0);

    WEBLOG("EVENT:POS | Node:" << self << " | x:" << myX << " | y:" << myY);
}

BpabMac::~BpabMac() {
    if (packetToBroadcast) cancelAndDelete(packetToBroadcast);
}

// --- 1. XU LY GOI TIN TU TANG MANG (Network Layer) ---
void BpabMac::fromNetworkLayer(cPacket *msg, int destination) {
    if (bpabMacState != BPAB_IDLE) {
        WEBLOG("EVENT:REJECT | Node:" << self << " | Reason:MAC_BUSY");
        delete msg;
        return;
    }
    preparePacket(msg);
    sendRTB();
}

// --- 2. XU LY GOI TIN NHAN DUOC TU RADIO ---
void BpabMac::fromRadioLayer(cPacket *msg, double rssi, double lqi) {
    BPABPacket *pkt = dynamic_cast<BPABPacket*>(msg);
    if (!pkt) return;

    switch (pkt->getBpabType()) {
        case BPAB_RTB: {
            if (bpabMacState == BPAB_WAIT_ACK) {
                cancelTimer(8);
                bpabMacState = BPAB_IDLE;

                WEBLOG("EVENT:ACK_RECEIVED | Node:" << self << " | Status:SUCCESS");

                if (BpabTraCIManager::isNodeAtIntersection(self) && !branchQueue.empty()) {
                    processNextBranch();
                } else {
                    bpabMacState = BPAB_IDLE;
                    if (packetToBroadcast) {
                        delete packetToBroadcast;
                        packetToBroadcast = NULL;
                    }
                    WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
                }
                return;
            }

            myX = mobilityModule->getLocation().x;
            myY = mobilityModule->getLocation().y;
            double srcX = pkt->getSourceX();
            double srcY = pkt->getSourceY();

            if (!isValidForwardNode(myX, myY, srcX, srcY,
                                    pkt->getDirection(), rangeR)) {
                return;
            }

            if (bpabMacState == BPAB_CONTENDING) {
                return;
            }

            bool isAtInter = BpabTraCIManager::isNodeAtIntersection(self);
            WEBLOG("EVENT:CHECK_INTER_STATE | Node:" << self
                   << " | isNodeAtIntersection_Result:" << (isAtInter ? "TRUE" : "FALSE"));

            if (isAtInter) {
                // -------------------------------------------------------
                // FAST TRACK: phát CTB trước cả khi xe thường gửi BB
                // Không đi qua endContention() để tránh dùng window [0,3*slot]
                // -------------------------------------------------------
                WEBLOG("EVENT:INTERSECTION_FAST_TRACK | Node:" << self
                       << " | Action:PREEMPT_BEFORE_BB");

                // Lưu thông tin nguồn
                srcId        = pkt->getSourceId();
                this->srcX   = srcX;
                this->srcY   = srcY;

                // Hủy mọi timer contention còn sót (phòng hờ)
                cancelTimer(1);
                cancelTimer(2);
                toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

                // Chuyển thẳng sang PRE_CTB, bỏ hoàn toàn binary partition
                heardCTB     = false;
                bpabMacState = BPAB_PRE_CTB;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:PRE_CTB");

                // Backoff [0, 0.3*slot]: đảm bảo phát CTB TRƯỚC Round 0 của xe thường
                // Window xe thường bắt đầu ở slotDuration (Round 0),
                // nên 0.3*slot không bao giờ chồng lên đó.
                // Nếu có nhiều node giao lộ cùng nhận RTB, heardCTB trong case 6
                // sẽ tự phân xử — node nào roll số nhỏ hơn thắng.
                double interBackoff = uniform(0.0, slotDuration * 0.3);
                WEBLOG("EVENT:WINNER | Node:" << self
                       << " | Backoff:" << interBackoff
                       << " | Type:INTERSECTION_PREEMPT");
                setTimer(6, interBackoff);
                break;
            }

            // --- Xe thường: tham gia binary contention bình thường ---
            const char* dirStr = (pkt->getDirection() == EAST)  ? "EAST"  :
                                 (pkt->getDirection() == WEST)  ? "WEST"  :
                                 (pkt->getDirection() == NORTH) ? "NORTH" : "SOUTH";

            WEBLOG("EVENT:JOIN_BPAB | Node:" << self
                    << " | Src:" << pkt->getSourceId()
                    << " | Dir:" << dirStr
                    << " | MaxR:" << rangeR
                    << " | Width:" << widthW);

            srcId              = pkt->getSourceId();
            this->srcX         = srcX;
            this->srcY         = srcY;
            limitL             = 0;
            limitU             = rangeR;
            this->srcDirection = pkt->getDirection();
            currentIteration   = 0;
            heardBB            = false;
            isTransmitting     = false;
            bpabMacState       = BPAB_CONTENDING;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:CONTENDING");

            double now = simTime().dbl();
            double rtbSentTime = pkt->getRtbSentTime();

            double boundary = rtbSentTime + slotDuration;
            double epsilon  = slotDuration * 0.01;
            while (boundary <= now + epsilon) {
                boundary += slotDuration;
            }
            slotStartTime = boundary;
            double delay = slotStartTime - now;
            setTimer(1, delay);
            break;
        }

        case BPAB_CTB: {
            int winnerId = pkt->getSourceId();

            if (bpabMacState == BPAB_PRE_CTB) {
                // Một node khác (giao lộ hoặc thường) đã thắng trước
                WEBLOG("EVENT:BACKDOWN | Node:" << self << " | Reason:HEARD_OTHER_CTB");
                heardCTB = true;
                break;
            }
            else if (bpabMacState == BPAB_CONTENDING) {
                // Nhận CTB trong lúc đang contend → dừng ngay, không cần phân biệt
                // winnerId là giao lộ hay xe thường; giao lộ đã preempt rồi nên
                // bất kỳ CTB nào đến đây đều hợp lệ.
                cancelTimer(1);
                cancelTimer(2);
                WEBLOG("EVENT:DROP_OUT | Node:" << self << " | Reason:HEARD_CTB_WHILE_CONTENDING");
                bpabMacState = BPAB_IDLE;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
            }
            else if (bpabMacState == BPAB_WAIT_CTB && packetToBroadcast) {
                retryCount = 0;
                cancelTimer(3);
                cancelTimer(4);
                sendData(winnerId);
            }
            break;
        }

        case BPAB_DATA: {
            cPacket *netPkt = pkt->decapsulate();
            toNetworkLayer(netPkt->dup());

            if (pkt->getDestinationId() == self) {
                retryCount = 0;
                cancelTimer(5);
                WEBLOG("EVENT:RCV_DATA | Status:SUCCESS_RELAY | Node:" << self);
                preparePacket(netPkt);

                if (BpabTraCIManager::isNodeAtIntersection(self)) {
                    while (!branchQueue.empty()) branchQueue.pop();

                    double currentX = mobilityModule->getLocation().x;
                    double currentY = mobilityModule->getLocation().y;

                    int incomingDir = getIncomingBranch(this->srcX, this->srcY,
                                                        currentX, currentY);

                    WEBLOG("EVENT:INTERSECTION_LOGIC | Node:" << self
                           << " | IncomingBranch:" << incomingDir
                           << " | Action:EXCLUDING_THIS_BRANCH");

                    if (incomingDir != EAST)  branchQueue.push(EAST);
                    if (incomingDir != WEST)  branchQueue.push(WEST);
                    if (incomingDir != NORTH) branchQueue.push(NORTH);
                    if (incomingDir != SOUTH) branchQueue.push(SOUTH);

                    processNextBranch();
                } else {
                    sendRTB();
                }
            } else {
                delete netPkt;
            }
            break;
        }

        case BPAB_BLACK_BURST: {
            if (bpabMacState == BPAB_CONTENDING && !isTransmitting) {
                heardBB = true;
                WEBLOG("EVENT:CS_DETECTED | Node:" << self << " | Type:BB_PACKET");
            }
            break;
        }

        default:
            break;
    }
}

int BpabMac::handleRadioControlMessage(cMessage *msg) {
    RadioControlMessage *radioMsg = dynamic_cast<RadioControlMessage*>(msg);
    if (radioMsg && radioMsg->getRadioControlMessageKind() == CARRIER_SENSE_INTERRUPT) {
        if (bpabMacState == BPAB_CONTENDING && !isTransmitting) {
            heardBB = true;
        }
        return 1;
    }
    return 0;
}

void BpabMac::timerFiredCallback(int timerIndex) {
    switch (timerIndex) {
        case 1: {
            if (bpabMacState != BPAB_CONTENDING) break;

            if (currentIteration >= maxIterations) {
                endContention(true);
                break;
            }

            myX = mobilityModule->getLocation().x;
            myY = mobilityModule->getLocation().y;

            if (!isValidForwardNode(myX, myY, srcX, srcY, srcDirection, rangeR)) {
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

        case 2: {
            if (bpabMacState != BPAB_CONTENDING) break;

            double mid = (limitL + limitU) / 2.0;
            toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

            if (!isTransmitting) {
                if (heardBB) {
                    WEBLOG("EVENT:DROP_OUT | Node:" << self << " | Reason:HEARD_BB");
                    endContention(false);
                    break;
                } else {
                    limitU = mid;
                    WEBLOG("EVENT:ITERATION_KEEP | Node:" << self
                           << " | Round:" << currentIteration << " | Action:SHRINK_UPPER");
                }
            } else {
                limitL = mid;
                WEBLOG("EVENT:ITERATION_KEEP | Node:" << self
                       << " | Round:" << currentIteration << " | Action:SHRINK_LOWER");
            }

            currentIteration++;
            double nextRoundStart = slotStartTime + currentIteration * (2.0 * slotDuration);
            double now   = simTime().dbl();
            double delay = nextRoundStart - now;

            double epsilon = slotDuration * 0.01;
            if (delay < epsilon) delay = epsilon;

            setTimer(1, delay);
            break;
        }

        case 3: {
            if (bpabMacState != BPAB_WAIT_CTB) break;

            if (packetToBroadcast && retryCount < maxRetries) {
                // Logic retry giữ nguyên
            } else {
                WEBLOG("EVENT:DROP_PKT | Node:" << self << " | Reason:MAX_RETRY_REACHED");

                if (BpabTraCIManager::isNodeAtIntersection(self) && !branchQueue.empty()) {
                    processNextBranch();
                } else {
                    if (packetToBroadcast) {
                        delete packetToBroadcast;
                        packetToBroadcast = NULL;
                    }
                    retryCount = 0;
                    bpabMacState = BPAB_IDLE;
                    WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE | Reason:DROP_PKT");
                }
            }
            break;
        }

        case 4: {
            if (bpabMacState != BPAB_WAIT_CTB) break;
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
                // Node giao lộ khác đã thắng trước trong cùng window [0, 0.3*slot]
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE | Reason:LOST_BACKOFF");
                heardCTB     = false;
                bpabMacState = BPAB_IDLE;
                break;
            }

            WEBLOG("EVENT:SEND_CTB | From:" << self << " | To:BROADCAST");
            sendCTB();
            break;
        }

        case 7: {
            if (bpabMacState != BPAB_WAIT_DATA) break;
            toRadioLayer(createRadioCommand(SET_STATE, RX));
            break;
        }

        case 8: {
            if (bpabMacState == BPAB_WAIT_ACK) {
                WEBLOG("EVENT:ACK_TIMEOUT | Node:" << self << " | Action:RESTART_CONTENTION");
                bpabMacState = BPAB_IDLE;

                if (packetToBroadcast) {
                    WEBLOG("EVENT:RESTART | Node:" << self << " | Reason:NO_ACK_RECEIVED");
                    retryCount = 0;
                    cPacket *netPkt = packetToBroadcast->decapsulate();
                    delete packetToBroadcast;
                    packetToBroadcast = NULL;
                    preparePacket(netPkt);
                    sendRTB();
                }
            }
            break;
        }

        case 9: {
            calculateTransmissionDirection();
            setTimer(9, 1.0);
            break;
        }

        default:
            WEBLOG("Unknown timer: " << timerIndex);
            break;
    }
}

bool BpabMac::isValidForwardNode(double myX, double myY,
                                 double srcX, double srcY,
                                 int direction,
                                 double rangeR) {
    double dx = myX - srcX;
    double dy = myY - srcY;

    double absDx    = fabs(dx);
    double absDy    = fabs(dy);
    double halfWidth = widthW / 2.0;

    switch (direction) {
        case EAST:
            if (dx > minProgress && dx <= rangeR && absDy <= halfWidth) {
                myDistanceToSrc = dx;
                return true;
            }
            break;

        case WEST:
            if (dx < -minProgress && absDx <= rangeR && absDy <= halfWidth) {
                myDistanceToSrc = absDx;
                return true;
            }
            break;

        case NORTH:
            if (dy > minProgress && dy <= rangeR && absDx <= halfWidth) {
                myDistanceToSrc = dy;
                return true;
            }
            break;

        case SOUTH:
            if (dy < -minProgress && absDy <= rangeR && absDx <= halfWidth) {
                myDistanceToSrc = absDy;
                return true;
            }
            break;

        case INTER: {
            double radialDist = sqrt(dx*dx + dy*dy);
            if (radialDist > minProgress && radialDist <= rangeR) {
                myDistanceToSrc = radialDist;
                return true;
            }
            break;
        }

        default:
            return false;
    }

    return false;
}

// --- Đóng gói netPkt vào packetToBroadcast ---
void BpabMac::preparePacket(cPacket *netPkt) {
    BPABPacket *pkt = new BPABPacket("BPAB_DATA", MAC_LAYER_PACKET);
    pkt->setByteLength(netPkt->getByteLength());
    pkt->setPayload("BPAB_PAYLOAD");
    pkt->encapsulate(netPkt);
    packetToBroadcast = pkt;
}

// --- Phát RTB ---
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

    bpabMacState = BPAB_WAIT_CTB;
    toRadioLayer(rtbPkt);
    toRadioLayer(createRadioCommand(SET_STATE, TX));
    WEBLOG("EVENT:SEND_RTB | From:" << self << " | To:BROADCAST");
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

// --- Kết thúc contention ---
// CHỈ được gọi bởi xe thường (binary partition).
// Node giao lộ KHÔNG đi qua đây nữa — chúng set PRE_CTB trực tiếp trong case BPAB_RTB.
void BpabMac::endContention(bool won) {
    cancelTimer(1);
    cancelTimer(2);
    toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

    if (won) {
        toRadioLayer(createRadioCommand(SET_STATE, RX));
        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));

        // Xe thường dùng window [slotDuration, 3*slotDuration].
        // Giao lộ dùng [0, 0.3*slot], nên hai window không bao giờ chồng nhau.
        // Xe thường sẽ nghe CTB của giao lộ trong case 6 → heardCTB=true → nhường.
        double backoff = uniform(slotDuration, slotDuration * 3.0);
        WEBLOG("EVENT:WINNER | Node:" << self << " | Backoff:" << backoff << " | Type:NORMAL");

        bpabMacState = BPAB_PRE_CTB;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:PRE_CTB");
        setTimer(6, backoff);
    } else {
        if (packetToBroadcast) {
            delete packetToBroadcast;
            packetToBroadcast = NULL;
        }
        bpabMacState = BPAB_IDLE;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
    }

    currentIteration = 0;
    heardBB          = false;
    isTransmitting   = false;
}

// --- Đóng gói và phát gói CTB ra radio ---
void BpabMac::sendCTB() {
    BPABPacket *ctb = new BPABPacket("BPAB_CTB", MAC_LAYER_PACKET);
    ctb->setBpabType(BPAB_CTB);
    ctb->setSourceId(self);
    ctb->setByteLength(1);

    toRadioLayer(ctb);
    toRadioLayer(createRadioCommand(SET_STATE, TX));

    bpabMacState = BPAB_WAIT_DATA;
    WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_DATA");
    setTimer(5, slotDuration * 10);
    setTimer(7, slotDuration * 0.1);
}

// --- Gửi gói DATA tới node thắng contention ---
void BpabMac::sendData(int winnerId) {
    WEBLOG("EVENT:SEND_DATA | From:" << self << " | To:" << winnerId);
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

    double dx = myX - lastX;
    double dy = myY - lastY;

    if (fabs(dx) > 0.01 || fabs(dy) > 0.01) {
        if (fabs(dx) > fabs(dy)) {
            transmissionDirection = (dx > 0) ? WEST : EAST;
        } else {
            transmissionDirection = (dy > 0) ? SOUTH : NORTH;
        }
    }

    lastX = myX;
    lastY = myY;

    return transmissionDirection;
}

void BpabMac::processNextBranch() {
    if (!branchQueue.empty()) {
        transmissionDirection = branchQueue.front();
        branchQueue.pop();

        retryCount = 0;

        WEBLOG("EVENT:BRANCH_SWITCH | Node:" << self << " | NextDir:" << transmissionDirection);

        sendRTB();
    } else {
        if (packetToBroadcast) {
            delete packetToBroadcast;
            packetToBroadcast = NULL;
        }
        bpabMacState = BPAB_IDLE;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE | Reason:ALL_BRANCHES_DONE");
    }
}

int BpabMac::getIncomingBranch(double sX, double sY, double mX, double mY) {
    double dx = sX - mX;
    double dy = sY - mY;

    if (fabs(dx) > fabs(dy)) {
        return (dx > 0) ? EAST : WEST;
    } else {
        return (dy > 0) ? NORTH : SOUTH;
    }
}
