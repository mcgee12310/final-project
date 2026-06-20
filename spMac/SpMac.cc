#include "SpMac.h"
#include <sstream>
#include <cmath>
#include "BpabTraCIManager.h"

Define_Module(SpMac);

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

void SpMac::startup() {
    VirtualMac::startup();

    maxBroadcastRange = par("maxBroadcastRange");
    maxHopCount = par("maxHopCount");
    numSlots = par("numSlots");
    slotDuration = par("slotDuration");
    probabilityP = par("probabilityP");
    seqCounter = 0;

    cModule *node = getParentModule()->getParentModule();
    cModule *mobility = node->getSubmodule("MobilityManager");
    mobilityModule = check_and_cast<VirtualMobilityManager*>(mobility);

    double myX = mobilityModule->getLocation().x;
    double myY = mobilityModule->getLocation().y;

    WEBLOG("SP_STARTUP node=" << self);
    WEBLOG("EVENT:POS | Node:" << self << " | x:" << myX << " | y:" << myY << " | Type:Vehicle");

    toRadioLayer(createRadioCommand(SET_STATE, RX));
}

bool SpMac::alreadySeen(int src, int seq) {
    std::pair<int,int> key(src, seq);
    if (seenPackets.count(key)) return true;
    seenPackets.insert(key);
    return false;
}

double SpMac::distance(double x1, double y1, double x2, double y2) {
    double dx = x1 - x2;
    double dy = y1 - y2;
    return sqrt(dx * dx + dy * dy);
}

void SpMac::fromNetworkLayer(cPacket *pkt, int destination) {
    double myX = mobilityModule->getLocation().x;
    double myY = mobilityModule->getLocation().y;

    SpPacket *spPkt = new SpPacket("SpData", MAC_LAYER_PACKET);
    spPkt->setOriginalSourceId(self);
    spPkt->setSenderId(self);
    spPkt->setSpSeqNumber(seqCounter++);
    spPkt->setCreationTime(simTime().dbl());
    spPkt->setSourceX(myX);
    spPkt->setSourceY(myY);
    spPkt->setSenderX(myX);
    spPkt->setSenderY(myY);
    spPkt->setHopCount(0);
    spPkt->setTtl(maxHopCount);
    spPkt->setPayload("SP_FLOOD");

    spPkt->encapsulate(pkt);
    seenPackets.insert(std::make_pair(spPkt->getOriginalSourceId(), spPkt->getSpSeqNumber()));

    WEBLOG("SEND src=" << self << " seq=" << spPkt->getSpSeqNumber() << " bytes=" << spPkt->getByteLength());

    toRadioLayer(spPkt);
    toRadioLayer(createRadioCommand(SET_STATE, TX));
}

void SpMac::fromRadioLayer(cPacket *pkt, double rssi, double lqi) {
    SpPacket *spPkt = dynamic_cast<SpPacket*>(pkt);
    if (!spPkt) return;

    int src = spPkt->getOriginalSourceId();
    int seq = spPkt->getSpSeqNumber();

    // ======================================================
    // BƯỚC 1: KIỂM TRA KHOẢNG CÁCH ĐẦU TIÊN (LỌC BIÊN GIỚI)
    // ======================================================
    double myX = mobilityModule->getLocation().x;
    double myY = mobilityModule->getLocation().y;
    double d = distance(myX, myY, spPkt->getSenderX(), spPkt->getSenderY());

    // Nếu khoảng cách vượt quá bán kính cấu hình -> Vứt bỏ ngay lập tức
    if (d > maxBroadcastRange) {
        // WEBLOG("DROP_RANGE node=" << self << " src=" << src << " dist=" << d);
        return;
    }

    // ======================================================
    // BƯỚC 2: LỌC TTL (TIME-TO-LIVE)
    // ======================================================
    if (spPkt->getTtl() <= 0) {
        WEBLOG("DROP_TTL node=" << self << " src=" << src << " seq=" << seq);
        return;
    }

    // ======================================================
    // BƯỚC 3: KIỂM TRA LẶP & HỦY PHÁT KHI NGHE LỎM (OVERHEARING)
    // ======================================================
    if (alreadySeen(src, seq)) {
        // Nếu gói tin này đã từng nhận rồi, kiểm tra xem mình có đang chờ phát nó không
        if (!rebroadcastQueue.empty()) {
            SpPacket *frontPkt = rebroadcastQueue.front();
            if (frontPkt->getOriginalSourceId() == src && frontPkt->getSpSeqNumber() == seq) {
                // Hủy hẹn giờ và dọn hàng đợi
                cancelTimer(TIMER_SP_REBROADCAST);
                while (!rebroadcastQueue.empty()) {
                    delete rebroadcastQueue.front();
                    rebroadcastQueue.pop();
                }
                WEBLOG("SP_CANCEL | Node:" << self << " | Reason:OVERHEARD_FORWARDER");
            }
        }
//        WEBLOG("DROP_DUPLICATE node=" << self << " src=" << src << " seq=" << seq);
        return;
    }

    // ======================================================
    // BƯỚC 4: GIAO BẢN TIN AN TOÀN CHO TẦNG ỨNG DỤNG
    // ======================================================
    WEBLOG("EVENT:SP_RCV | Node:" << self
               << " | Src:" << src
               << " | Seq:" << seq
               << " | Hop:" << spPkt->getHopCount()
               << " | Dist:" << d);
    cPacket *innerPkt = spPkt->getEncapsulatedPacket();
    if (innerPkt) {
        toNetworkLayer(innerPkt->dup());
    }

    // ======================================================
    // BƯỚC 5: TÍNH TOÁN SLOT THEO KHOẢNG CÁCH VÀ HẸN GIỜ PHÁT
    // ======================================================
    double ratio = d / maxBroadcastRange;
    if (ratio > 1.0) ratio = 1.0;

    // Nút ở xa (ratio -> 1) có slot nhỏ. Nút ở gần có slot lớn.
    int assignedSlot = floor(numSlots * (1.0 - ratio));
    if (assignedSlot < 0) assignedSlot = 0;
    if (assignedSlot >= numSlots) assignedSlot = numSlots - 1;

    // MicroJitter (1ms -> 10ms) để tránh đụng độ trong cùng 1 slot
    double microJitter = uniform(0.001, 0.010);
    double totalDelay = (assignedSlot * slotDuration) + microJitter;

    WEBLOG("SP_ASSIGN | Node:" << self << " | Dist:" << d << " | Slot:" << assignedSlot << " | Delay:" << totalDelay);

    scheduleRebroadcast(spPkt, totalDelay);
}

void SpMac::scheduleRebroadcast(SpPacket *pkt, double delay) {
    if (pkt->getTtl() <= 1) return;

    SpPacket *copy = pkt->dup();
    copy->removeControlInfo(); // Fix lỗi Radio nuốt gói

    copy->setSenderId(self);
    copy->setSenderX(mobilityModule->getLocation().x);
    copy->setSenderY(mobilityModule->getLocation().y);
    copy->setHopCount(pkt->getHopCount() + 1);
    copy->setTtl(pkt->getTtl() - 1);

    rebroadcastQueue.push(copy);

    if (rebroadcastQueue.size() == 1) {
        setTimer(TIMER_SP_REBROADCAST, delay);
    }
}

void SpMac::timerFiredCallback(int index) {
    if (index == TIMER_SP_REBROADCAST) {
        if (!rebroadcastQueue.empty()) {
            SpPacket *pktToTransmit = rebroadcastQueue.front();
            rebroadcastQueue.pop();

            // QUYẾT ĐỊNH XÁC SUẤT p KHI ĐẾN LƯỢT (SLOT)
            double randVal = uniform(0.0, 1.0);

            if (randVal <= probabilityP) {
                WEBLOG("SP_DECISION | Node:" << self << " | p:" << probabilityP << " | Rand:" << randVal << " | Action:FORWARD");
                toRadioLayer(pktToTransmit);
                toRadioLayer(createRadioCommand(SET_STATE, TX));
            } else {
                WEBLOG("SP_DECISION | Node:" << self << " | p:" << probabilityP << " | Rand:" << randVal << " | Action:DROP_PROBABILITY");
                delete pktToTransmit;
            }

            // (Giản lược xử lý multi-packet cho mô phỏng cơ bản)
            if (!rebroadcastQueue.empty()) {
                // Nếu còn gói tin khác, set 1 delay nhỏ để xử lý tiếp
                setTimer(TIMER_SP_REBROADCAST, uniform(0.001, 0.005));
            }
        }
    }
}

int SpMac::handleRadioControlMessage(cMessage *msg) { return 0; }

SpMac::~SpMac() {
    while (!rebroadcastQueue.empty()) {
        SpPacket *pkt = rebroadcastQueue.front();
        rebroadcastQueue.pop();
        delete pkt;
    }
}
