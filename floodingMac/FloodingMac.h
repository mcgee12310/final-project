// FloodingMac.h

#ifndef FLOODINGMAC_H_
#define FLOODINGMAC_H_

#include <set>
#include <utility>
#include <queue>
#include "VirtualMac.h"
#include "FloodPacket_m.h"

class VirtualMobilityManager;

class FloodingMac : public VirtualMac {
protected:
    // =====================================================
    // Timers
    // =====================================================
    enum MacTimers {
        TIMER_REBROADCAST = 1
    };

    // =====================================================
    // Parameters
    // =====================================================
    double maxBroadcastRange;
    double jitterMax;
    int maxHopCount;

    // =====================================================
    // Runtime
    // =====================================================
    int seqCounter;
    VirtualMobilityManager *mobilityModule;

    // Duplicate detection (Phát hiện trùng lặp)
    std::set<std::pair<int, int> > seenPackets;

    // Hàng đợi lưu các gói tin chờ phát lại (Jitter Buffer)
    std::queue<FloodingPacket*> rebroadcastQueue;

protected:
    // Các hàm kế thừa từ VirtualMac của Castalia
    virtual void startup();
    virtual void fromNetworkLayer(cPacket *pkt, int destination);
    virtual void fromRadioLayer(cPacket *pkt, double rssi, double lqi);
    virtual int handleRadioControlMessage(cMessage *msg);
    virtual void timerFiredCallback(int index);

    // Các hàm Logic nội bộ
    bool alreadySeen(int src, int seq);
    double distance(double x1, double y1, double x2, double y2);
    void rebroadcastPacket(FloodingPacket *pkt);

public:
    // Bắt buộc phải có Destructor để dọn rác trong rebroadcastQueue khi kết thúc mô phỏng
    virtual ~FloodingMac();
};

#endif
