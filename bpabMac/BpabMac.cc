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
        /* GỌI SANG BIẾN STATIC CỦA TRACIMANAGER */ \
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
        widthW = 50.0; // Mặc định hình chữ nhật rộng 50m
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
}

BpabMac::~BpabMac() {
    if (packetToBroadcast) cancelAndDelete(packetToBroadcast);
}

// --- 1. XU LY GOI TIN TU TANG MANG (Network Layer) ---
void BpabMac::fromNetworkLayer(cPacket *msg, int destination) {
    if (bpabMacState != BPAB_IDLE) {
        WEBLOG("EVENT:REJECT | Node:" << self << " | Reason:MAC_BUSY"); // Thay cho "MAC dang ban"
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

            myX = mobilityModule->getLocation().x;
            myY = mobilityModule->getLocation().y;
            double srcX = pkt->getSourceX();
            double srcY = pkt->getSourceY();

            if (!isValidForwardNode(myX, myY, srcX, srcY,
                                    pkt->getDirection(), rangeR)) {
                WEBLOG("EVENT:FILTER | Node:" << self << " | Status:WRONG_DIRECTION"); // Thay cho "Sai huong"
                return;
            }

            if (bpabMacState == BPAB_CONTENDING) {
                WEBLOG("EVENT:FILTER | Node:" << self << " | Status:ALREADY_CONTENDING"); // Thay cho "Dang contending"
                return;
            }

            // Chuyển đổi biến int direction thành chuỗi chữ để UI dễ đọc
            const char* dirStr = (pkt->getDirection() == EAST) ? "EAST" :
                                 (pkt->getDirection() == WEST) ? "WEST" :
                                 (pkt->getDirection() == NORTH) ? "NORTH" : "SOUTH";

            // Nối thêm Dir, MaxR và Width vào đuôi log
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
                WEBLOG("EVENT:BACKDOWN | Node:" << self << " | Reason:HEARD_OTHER_CTB"); // Thay cho "nghe thay CTB -> rut lui"
                heardCTB = true;
                break;
            }

            else if (bpabMacState == BPAB_WAIT_CTB && packetToBroadcast) {
                retryCount = 0;
                cancelTimer(3);
                cancelTimer(4); // hủy timer set RX nếu chưa fired

                WEBLOG("EVENT:SEND_DATA | From:" << self << " | To:" << winnerId);
                packetToBroadcast->setDestinationId(winnerId);
                packetToBroadcast->setBpabType(BPAB_DATA);
                toRadioLayer(packetToBroadcast);
                toRadioLayer(createRadioCommand(SET_STATE, TX));
                packetToBroadcast = NULL;
                bpabMacState = BPAB_IDLE;
                WEBLOG("EVENT:STATE | Node:" << self << " | State:IDLE | Reason:DATA_SENT_SUCCESS");

            } else {
                WEBLOG("EVENT:RCV_CTB | Status:IGNORED | Node:" << self);
            }
            break;
        }

        case BPAB_DATA: {
            if (pkt->getDestinationId() == self) {
                retryCount = 0;
                cancelTimer(5);
                WEBLOG("EVENT:RCV_DATA | Status:SUCCESS | Node:" << self); // Thay cho "Nhan DATA thanh cong"
                cPacket *netPkt = pkt->decapsulate();
//                toNetworkLayer(netPkt);
                startBpabTransmission(netPkt);
            } else {
                WEBLOG("EVENT:RCV_DATA | Status:IGNORE | Node:" << self);
            }
            break;
        }

        case BPAB_BLACK_BURST: {
            // fallback: nếu packet không bị collision thì detect luôn
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
            WEBLOG("EVENT:CS_DETECTED | Node:" << self << " | Type:BB"); // Thay cho "Carrier sense: Nghe duoc BB"
        }
        return 1;
    }
    return 0;
}

void BpabMac::timerFiredCallback(int timerIndex) {
    switch (timerIndex) {
        case 1: {
            if (bpabMacState != BPAB_CONTENDING) break; // ← thêm guard

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
                // Mình ở nửa GẦN
                if (heardBB) {
                    // Nghe thấy BB -> Có đứa ở xa hơn mình -> Bỏ cuộc
                    WEBLOG("EVENT:DROP_OUT | Node:" << self << " | Reason:HEARD_BB");
                    endContention(false);
                    break;
                } else {
                    // Không nghe thấy BB -> Không có ai ở xa hơn -> Thu hẹp giới hạn TRÊN
                    limitU = mid;
                    WEBLOG("EVENT:ITERATION_KEEP | Node:" << self << " | Round:" << currentIteration << " | Action:SHRINK_UPPER");
                }
            } else {
                // Mình ở nửa XA (đã phát BB) -> Vẫn tiếp tục tranh chấp -> Thu hẹp giới hạn DƯỚI
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
                WEBLOG("EVENT:RETRY | Node:" << self << " | Count:" << retryCount); // Thay cho "thu lai lan x/y"
                cPacket *netPkt = packetToBroadcast->decapsulate();
                delete packetToBroadcast;
                packetToBroadcast = NULL;
                bpabMacState = BPAB_IDLE;
                startBpabTransmission(netPkt);
            } else {
                WEBLOG("EVENT:DROP_PKT | Node:" << self << " | Reason:MAX_RETRY_REACHED"); // Thay cho "huy goi tin"
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
                break; // ← break, không gửi CTB
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
            break; // ← QUAN TRỌNG: phải có break
        }

        case 7: {
            if (bpabMacState != BPAB_WAIT_DATA) break;
            WEBLOG("EVENT:RADIO_STATE | Node:" << self << " | Mode:RX | Goal:WAIT_DATA");
            toRadioLayer(createRadioCommand(SET_STATE, RX));
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
    double halfWidth = widthW / 2.0; // Nửa chiều rộng đường
    WEBLOG("EVENT:DEBUG_FILTER | Node:" << self << " | dx:" << dx << " | dy:" << dy
                << " | Dir:" << direction);
    switch (direction) {
        case EAST:
            // Tiến về +X: dx phải > 0, không vượt quá chiều dài rangeR,
            // và độ lệch trục Y không vượt quá nửa chiều rộng.
            if (dx > minProgress && dx <= rangeR && absDy <= halfWidth) {
                myDistanceToSrc = dx; // Tranh chấp dựa trên tiến độ trục X
                return true;
            }
            break;

        case WEST:
            // Tiến về -X: dx phải < 0
            if (dx < -minProgress && absDx <= rangeR && absDy <= halfWidth) {
                myDistanceToSrc = absDx;
                return true;
            }
            break;

        case NORTH:
            // Tiến về +Y: dy phải > 0, độ lệch trục X không vượt quá nửa chiều rộng
            if (dy > minProgress && dy <= rangeR && absDx <= halfWidth) {
                myDistanceToSrc = dy; // Tranh chấp dựa trên tiến độ trục Y
                return true;
            }
            break;

        case SOUTH:
            // Tiến về -Y: dy phải < 0
            if (dy < -minProgress && absDy <= rangeR && absDx <= halfWidth) {
                myDistanceToSrc = absDy;
                return true;
            }
            break;

        default:
            return false;
    }

    return false; // Nằm ngoài hình chữ nhật
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
    rtbPkt->setDirection(EAST);
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
    bb->setByteLength(1); // ✅ nhỏ nhất có thể = ít TX time nhất
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

        // Random backoff trong khoảng [0, slotDuration]
        double backoff = uniform(0, 3 * slotDuration);
        WEBLOG("EVENT:WINNER | Node:" << self << " | Backoff:" << backoff);

        bpabMacState = BPAB_PRE_CTB; // state mới: chờ trước khi gửi CTB
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
