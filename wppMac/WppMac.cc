// WppMac.cc
#include "WppMac.h"
#include <sstream>
#include <cmath>
#include "BpabTraCIManager.h"

Define_Module(WppMac);

// Tận dụng lại hệ thống ghi log của bạn
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

// ======================================================
// STARTUP
// ======================================================
void WppMac::startup() {
    VirtualMac::startup();

    maxBroadcastRange = 400;
    jitterMax = 0.01;
    maxHopCount = par("maxHopCount");
    seqCounter = 0;

    cModule *node = getParentModule()->getParentModule();
    cModule *mobility = node->getSubmodule("MobilityManager");
    mobilityModule = check_and_cast<VirtualMobilityManager*>(mobility);

    double myX = mobilityModule->getLocation().x;
    double myY = mobilityModule->getLocation().y;

    WEBLOG("WPP_STARTUP node=" << self);
    WEBLOG("EVENT:POS | Node:" << self << " | x:" << myX << " | y:" << myY << " | Type:Vehicle");

    // Đảm bảo kênh thu luôn mở
    toRadioLayer(createRadioCommand(SET_STATE, RX));
}

// ======================================================
// DUPLICATE DETECTION & DISTANCE
// ======================================================
bool WppMac::alreadySeen(int src, int seq) {
    std::pair<int,int> key(src, seq);
    if (seenPackets.count(key)) return true;
    seenPackets.insert(key);
    return false;
}

double WppMac::distance(double x1, double y1, double x2, double y2) {
    double dx = x1 - x2;
    double dy = y1 - y2;
    return sqrt(dx * dx + dy * dy);
}

// ======================================================
// FROM NETWORK LAYER (SOURCE NODE)
// ======================================================
void WppMac::fromNetworkLayer(cPacket *pkt, int destination) {
    double myX = mobilityModule->getLocation().x;
    double myY = mobilityModule->getLocation().y;

    WppPacket *wppPkt = new WppPacket("WppData", MAC_LAYER_PACKET);
    wppPkt->setOriginalSourceId(self);
    wppPkt->setSenderId(self);
    wppPkt->setWppSeqNumber(seqCounter++);
    wppPkt->setCreationTime(simTime().dbl());
    wppPkt->setSourceX(myX);
    wppPkt->setSourceY(myY);
    wppPkt->setSenderX(myX);
    wppPkt->setSenderY(myY);
    wppPkt->setHopCount(0);
    wppPkt->setTtl(maxHopCount);
    wppPkt->setPayload("WPP_FLOOD");

    wppPkt->encapsulate(pkt);
    seenPackets.insert(std::make_pair(wppPkt->getOriginalSourceId(), wppPkt->getWppSeqNumber()));

    WEBLOG("SEND src=" << self << " seq=" << wppPkt->getWppSeqNumber() << " bytes=" << wppPkt->getByteLength());

    toRadioLayer(wppPkt);
    toRadioLayer(createRadioCommand(SET_STATE, TX));
}

// ======================================================
// FROM RADIO LAYER (RECEIVER / RELAY NODE)
// ======================================================
void WppMac::fromRadioLayer(cPacket *pkt, double rssi, double lqi) {
    WppPacket *wppPkt = dynamic_cast<WppPacket*>(pkt);
    if (!wppPkt) return;

    int src = wppPkt->getOriginalSourceId();
    int seq = wppPkt->getWppSeqNumber();

    // 2. Tính toán khoảng cách (D)
    double myX = mobilityModule->getLocation().x;
    double myY = mobilityModule->getLocation().y;
    double d = distance(myX, myY, wppPkt->getSenderX(), wppPkt->getSenderY());

    // 3. Lọc tầm phủ sóng và TTL
    if (d > maxBroadcastRange) return;
    if (wppPkt->getTtl() <= 0) {
        WEBLOG("DROP_TTL node=" << self << " src=" << src << " seq=" << seq);
        return;
    }

    // 1. Kiểm tra lặp (Duplicate Check)
    if (alreadySeen(src, seq)) {
        // Kiểm tra xem mình có đang ôm mộng phát lại gói này không
        if (!rebroadcastQueue.empty()) {
            WppPacket *frontPkt = rebroadcastQueue.front();
            if (frontPkt->getOriginalSourceId() == src && frontPkt->getWppSeqNumber() == seq) {
                // Hủy bộ đếm thời gian
                cancelTimer(TIMER_WPP_REBROADCAST);

                // Dọn sạch hàng đợi
                while (!rebroadcastQueue.empty()) {
                    delete rebroadcastQueue.front();
                    rebroadcastQueue.pop();
                }
                WEBLOG("WPP_CANCEL | Node:" << self << " | Reason:OVERHEARD_FORWARDER");
            }
        }

        WEBLOG("DROP_DUPLICATE node=" << self << " src=" << src << " seq=" << seq);
        return;
    }

    // 4. Giao bản tin cho tầng Application (Đảm bảo Node nhận được tin)
    cPacket *innerPkt = wppPkt->getEncapsulatedPacket();
    if (innerPkt) {
        toNetworkLayer(innerPkt->dup());
    }

    // 5. THUẬT TOÁN WEIGHTED p-PERSISTENCE
    double ratio = d / maxBroadcastRange;
    if (ratio > 1.0) ratio = 1.0;

    // Ép đường cong xác suất dốc hơn
    double p = ratio * ratio;

    double randVal = uniform(0.0, 1.0);

    if (randVal <= p) {
        WEBLOG("WPP_DECISION | Node:" << self << " | Dist:" << d << " | p:" << p << " | Rand:" << randVal << " | Action:FORWARD");
        rebroadcastPacket(wppPkt);
    } else {
        WEBLOG("WPP_DECISION | Node:" << self << " | Dist:" << d << " | p:" << p << " | Rand:" << randVal << " | Action:DROP_WPP");
    }
}

// ======================================================
// REBROADCAST QUEUE (JITTER)
// ======================================================
void WppMac::rebroadcastPacket(WppPacket *pkt) {
    if (pkt->getTtl() <= 1) return;

    WppPacket *copy = pkt->dup();

    // Rất quan trọng: Gỡ bỏ nhãn nhận của tầng vật lý để tránh bị nuốt gói
    copy->removeControlInfo();

    copy->setSenderId(self);
    copy->setSenderX(mobilityModule->getLocation().x);
    copy->setSenderY(mobilityModule->getLocation().y);
    copy->setHopCount(pkt->getHopCount() + 1);
    copy->setTtl(pkt->getTtl() - 1);

    rebroadcastQueue.push(copy);

    if (rebroadcastQueue.size() == 1) {
        // Lấy lại tọa độ nguồn từ gói tin copy để tính khoảng cách
        double srcX = copy->getSenderX();
        double srcY = copy->getSenderY();
        double myX = mobilityModule->getLocation().x;
        double myY = mobilityModule->getLocation().y;
        double d = distance(myX, myY, srcX, srcY);

        // Nút ở xa nhất (d gần bằng maxBroadcastRange) sẽ có delay gần bằng 0
        // Nút ở gần hơn sẽ phải chờ lâu hơn
        double ratio = d / maxBroadcastRange;
        if (ratio > 1.0) ratio = 1.0;

        double calculatedDelay = jitterMax * (1.0 - ratio);

        // Cộng thêm một chút nhiễu cực nhỏ (microsecond) để tránh lỗi số học
        calculatedDelay += uniform(0.0001, 0.001);

        setTimer(TIMER_WPP_REBROADCAST, calculatedDelay);
    }
}

// ======================================================
// TIMER CALLBACK
// ======================================================
void WppMac::timerFiredCallback(int index) {
    if (index == TIMER_WPP_REBROADCAST) {
        if (!rebroadcastQueue.empty()) {
            WppPacket *pktToTransmit = rebroadcastQueue.front();
            rebroadcastQueue.pop();

            WEBLOG("REBROADCAST_EXEC node=" << self << " hop=" << pktToTransmit->getHopCount());

            toRadioLayer(pktToTransmit);
            toRadioLayer(createRadioCommand(SET_STATE, TX));

            if (!rebroadcastQueue.empty()) {
                double randomJitter = uniform(0, jitterMax);
                setTimer(TIMER_WPP_REBROADCAST, randomJitter);
            }
        }
    }
}

int WppMac::handleRadioControlMessage(cMessage *msg) { return 0; }

// ======================================================
// DESTRUCTOR (MEMORY SAFETY)
// ======================================================
WppMac::~WppMac() {
    while (!rebroadcastQueue.empty()) {
        WppPacket *pkt = rebroadcastQueue.front();
        rebroadcastQueue.pop();
        delete pkt;
    }
}
