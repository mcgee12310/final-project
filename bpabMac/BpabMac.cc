#include "BpabMac.h"
#include <omnetpp.h>
#include <sstream>
#include "BpabTraCIManager.h"

Define_Module(BpabMac);

// Ä�Ãƒ Sá»¬A: ThÃªm hÃ m writeToUnifiedLog vÃ o ngay dÆ°á»›i trace()
#define WEBLOG(msgArgs) \
    do { \
        std::ostringstream _ss; \
        _ss << msgArgs; \
        trace() << _ss.str(); \
        /* --- GHI VÃ€O FILE LOG CHUNG --- */ \
        BpabTraCIManager::writeToUnifiedLog(simTime().dbl(), self, "MAC_EVENT", _ss.str()); \
        /* --- Gá»ŒI SANG BIáº¾N STATIC Cá»¦A TRACIMANAGER (CÅ¨) --- */ \
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
        widthW = 50.0; // Máº·c Ä‘á»‹nh hÃ¬nh chá»¯ nháº­t rá»™ng 50m
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
    startBpabTransmission(msg);
}

// --- 2. XU LY GOI TIN NHAN DUOC TU RADIO ---
void BpabMac::fromRadioLayer(cPacket *msg, double rssi, double lqi) {
    BPABPacket *pkt = dynamic_cast<BPABPacket*>(msg);
    if (!pkt) return;

    switch (pkt->getBpabType()) {
        case BPAB_RTB: {
            WEBLOG("EVENT:RCV | Type:RTB | From:" << pkt->getSourceId() << " | To:" << self);

            if (bpabMacState == BPAB_WAIT_ACK) {
                cancelTimer(8); // Tắt còi báo động timeout
                bpabMacState = BPAB_IDLE; // Trở về trạng thái nghỉ

                // Đã thành công nên có thể vứt bỏ gói tin gốc đi
                if (packetToBroadcast) {
                    delete packetToBroadcast;
                    packetToBroadcast = NULL;
                }
                WEBLOG("EVENT:ACK_RECEIVED | Node:" << self << " | Status:SUCCESS");
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE");
                return; // Kết thúc luôn, không tham gia tranh chấp nữa
            }

            myX = mobilityModule->getLocation().x;
            myY = mobilityModule->getLocation().y;
            double srcX = pkt->getSourceX();
            double srcY = pkt->getSourceY();

            if (!isValidForwardNode(myX, myY, srcX, srcY,
                                    pkt->getDirection(), rangeR)) {
                WEBLOG("EVENT:FILTER | Node:" << self << " | Status:WRONG_DIRECTION");
                return;
            }

            if (bpabMacState == BPAB_CONTENDING) {
                WEBLOG("EVENT:FILTER | Node:" << self << " | Status:ALREADY_CONTENDING");
                return;
            }

            const char* dirStr = (pkt->getDirection() == EAST) ? "EAST" :
                                 (pkt->getDirection() == WEST) ? "WEST" :
                                 (pkt->getDirection() == NORTH) ? "NORTH" : "SOUTH";

            WEBLOG("EVENT:JOIN_BPAB | Node:" << self
                    << " | Src:" << pkt->getSourceId()
                    << " | Dir:" << dirStr
                    << " | MaxR:" << rangeR
                    << " | Width:" << widthW);

            srcId            = pkt->getSourceId();
            this->srcX       = srcX;
            this->srcY       = srcY;
            limitL           = 0;
            limitU           = rangeR;
            currentIteration = 0;
            heardBB          = false;
            isTransmitting   = false;
            bpabMacState     = BPAB_CONTENDING;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:CONTENDING");
            setTimer(1, slotDuration);
            break;
        }

        case BPAB_CTB: {
            int winnerId = pkt->getSourceId();
            WEBLOG("EVENT:RCV_CTB | From:" << winnerId << " | To:" << self);

            if (bpabMacState == BPAB_PRE_CTB) {
                WEBLOG("EVENT:BACKDOWN | Node:" << self << " | Reason:HEARD_OTHER_CTB");
                heardCTB = true;
                break;
            }

            else if (bpabMacState == BPAB_WAIT_CTB && packetToBroadcast) {
                retryCount = 0;
                cancelTimer(3);
                cancelTimer(4);

                WEBLOG("EVENT:SEND_DATA | From:" << self << " | To:" << winnerId);
                packetToBroadcast->setDestinationId(winnerId);
                packetToBroadcast->setBpabType(BPAB_DATA);

                // QUAN TRỌNG: Gửi bản sao (dup) đi để giữ lại bản gốc phòng trường hợp cần Retry
                toRadioLayer(packetToBroadcast->dup());
                toRadioLayer(createRadioCommand(SET_STATE, TX));

                // Chuyển sang trạng thái chờ ACK và bật Timer 8
                bpabMacState = BPAB_WAIT_ACK;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_ACK");
                setTimer(8, 0.05);
            } else {
                WEBLOG("EVENT:RCV_CTB | Status:IGNORED | Node:" << self);
            }
            break;
        }

        case BPAB_DATA: {
            cPacket *netPkt = pkt->decapsulate();
            WEBLOG("EVENT:RCV_DATA | Status:UP_TO_APP | Node:" << self);
            toNetworkLayer(netPkt->dup());

            if (pkt->getDestinationId() == self) {
                retryCount = 0;
                cancelTimer(5);
                WEBLOG("EVENT:RCV_DATA | Status:SUCCESS_RELAY | Node:" << self);
                startBpabTransmission(netPkt);
            } else {
                delete netPkt;
                WEBLOG("EVENT:RCV_DATA | Status:PASSIVE_RECEIVE | Node:" << self);
            }
            break;
        }

        case BPAB_BLACK_BURST: {
            // fallback: náº¿u packet khÃ´ng bá»‹ collision thÃ¬ detect luÃ´n
            if (bpabMacState == BPAB_CONTENDING && !isTransmitting) {
                heardBB = true;
                WEBLOG("EVENT:CS_DETECTED | Node:" << self << " | Type:BB_PACKET");
            }
            break;
        }

        default:{
            WEBLOG("nghe duoc gi do");
        }
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
                WEBLOG("EVENT:WINNER_FINAL | Node:" << self << " | Rounds:" << maxIterations);
                endContention(true);
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
                WEBLOG("EVENT:LISTEN_BB | Node:" << self);
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
                    WEBLOG("EVENT:ITERATION_KEEP | Node:" << self << " | Round:" << currentIteration << " | Action:SHRINK_UPPER");
                }
            } else {
                limitL = mid;
                WEBLOG("EVENT:ITERATION_KEEP | Node:" << self << " | Round:" << currentIteration << " | Action:SHRINK_LOWER");
            }

            currentIteration++;
            setTimer(1, slotDuration);
            break;
        }

        case 3: {
            if (bpabMacState != BPAB_WAIT_CTB) break;

            if (packetToBroadcast && retryCount < maxRetries) {
                retryCount++;
                WEBLOG("EVENT:RETRY | Node:" << self << " | Count:" << retryCount);
                cPacket *netPkt = packetToBroadcast->decapsulate();
                delete packetToBroadcast;
                packetToBroadcast = NULL;
                bpabMacState = BPAB_IDLE;
                startBpabTransmission(netPkt);
            } else {
                WEBLOG("EVENT:DROP_PKT | Node:" << self << " | Reason:MAX_RETRY_REACHED");
                if (packetToBroadcast) {
                    delete packetToBroadcast;
                    packetToBroadcast = NULL;
                }
                retryCount = 0;
                bpabMacState = BPAB_IDLE;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE | Reason:DROP_PKT");
            }
            break;
        }

        case 4: {
            if (bpabMacState != BPAB_WAIT_CTB) break;
            WEBLOG("EVENT:RADIO_STATE | Node:" << self << " | Mode:RX | Goal:LISTEN_CTB");
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
                heardCTB     = false;
                bpabMacState = BPAB_IDLE;
                break;
            }

            WEBLOG("EVENT:SEND | Type:CTB | From:" << self << " | To:BROADCAST");

            BPABPacket *ctb = new BPABPacket("BPAB_CTB", MAC_LAYER_PACKET);
            ctb->setBpabType(BPAB_CTB);
            ctb->setSourceId(self);
            ctb->setByteLength(1);

            toRadioLayer(ctb);
            toRadioLayer(createRadioCommand(SET_STATE, TX));

            bpabMacState = BPAB_WAIT_DATA;
            WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_DATA");
            setTimer(5, slotDuration * 4);
            setTimer(7, 0.0001);
            break;
        }

        case 7: {
            if (bpabMacState != BPAB_WAIT_DATA) break;
            WEBLOG("EVENT:RADIO_STATE | Node:" << self << " | Mode:RX | Goal:WAIT_DATA");
            toRadioLayer(createRadioCommand(SET_STATE, RX));
            break;
        }

        case 8: {
            if (bpabMacState == BPAB_WAIT_ACK) {
                WEBLOG("EVENT:ACK_TIMEOUT | Node:" << self << " | Action:RESTART_CONTENTION");
                bpabMacState = BPAB_IDLE;

                // Không quan tâm số lần retry trước đó, luôn phát lại như 1 gói tin hoàn toàn mới
                if (packetToBroadcast) {
                    WEBLOG("EVENT:RESTART | Node:" << self << " | Reason:NO_ACK_RECEIVED");

                    // Xóa bộ đếm retry cũ để vòng tranh chấp mới có đủ cơ hội (cho WAIT_CTB)
                    retryCount = 0;

                    // Bóc lớp vỏ MAC cũ ra để lấy lại gói tin Network gốc
                    cPacket *netPkt = packetToBroadcast->decapsulate();
                    delete packetToBroadcast;
                    packetToBroadcast = NULL;

                    // Bắt đầu lại toàn bộ quá trình truyền (phát RTB) từ đầu
                    startBpabTransmission(netPkt);
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

    double absDx = fabs(dx);
    double absDy = fabs(dy);
    double halfWidth = widthW / 2.0;
    WEBLOG("EVENT:DEBUG_FILTER | Node:" << self << " | dx:" << dx << " | dy:" << dy
                << " | Dir:" << direction);
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

        default:
            return false;
    }

    return false;
}

void BpabMac::startBpabTransmission(cPacket *netPkt) {
    BPABPacket *pkt = new BPABPacket("BPAB_DATA", MAC_LAYER_PACKET);
    pkt->setByteLength(netPkt->getByteLength());
    pkt->setPayload("BPAB_PAYLOAD");
    pkt->encapsulate(netPkt);
    packetToBroadcast = pkt;

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
    WEBLOG("EVENT:SEND | Type:RTB | From:" << self << " | To:BROADCAST");
    WEBLOG("EVENT:STATE | Node:" << self << " | State:WAIT_CTB");

    setTimer(4, 0.0001);
    setTimer(3, slotDuration * (2 * (maxIterations + 5)));
}

void BpabMac::sendBlackBurst() {
    BPABPacket *bb = new BPABPacket("BLACK_BURST", MAC_LAYER_PACKET);
    bb->setBpabType(BPAB_BLACK_BURST);
    bb->setByteLength(1);
    toRadioLayer(bb);
    toRadioLayer(createRadioCommand(SET_STATE, TX));
}

void BpabMac::endContention(bool won) {
    cancelTimer(1);
    cancelTimer(2);
    toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_OFF));

    if (won) {
        toRadioLayer(createRadioCommand(SET_STATE, RX));
        toRadioLayer(createRadioCommand(SET_CS_INTERRUPT_ON));

        double backoff = uniform(0, 3 * slotDuration);
        WEBLOG("EVENT:WINNER | Node:" << self << " | Backoff:" << backoff);

        bpabMacState = BPAB_PRE_CTB;
        WEBLOG("EVENT:STATE | Node:" << self << " | State:PRE_CTB");
        setTimer(6, backoff);
    } else{
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

int BpabMac::calculateTransmissionDirection() {
    // Lấy tọa độ mới nhất
    myX = mobilityModule->getLocation().x;
    myY = mobilityModule->getLocation().y;

    double dx = myX - lastX;
    double dy = myY - lastY;

    // Chỉ cập nhật hướng nếu xe có di chuyển thực sự (tránh nhiễu khi xe đứng yên)
    if (fabs(dx) > 0.01 || fabs(dy) > 0.01) {
        // So sánh xem xe di chuyển theo trục X nhiều hơn hay trục Y nhiều hơn
        if (fabs(dx) > fabs(dy)) {
            // ĐÃ SỬA: Đang đi về hường Đông (dx > 0) thì truyền về Tây, và ngược lại
            transmissionDirection = (dx > 0) ? WEST : EAST;
        } else {
            // ĐÃ SỬA: Đang đi về hướng Bắc (dy > 0) thì truyền về Nam, và ngược lại
            transmissionDirection = (dy > 0) ? SOUTH : NORTH;
        }
    }

    // Lưu lại tọa độ hiện tại thành tọa độ cũ cho lần tính toán tiếp theo
    lastX = myX;
    lastY = myY;

    return transmissionDirection;
}
