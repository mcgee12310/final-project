// WppMac.h
#ifndef _WPPMAC_H_
#define _WPPMAC_H_

#include <queue>
#include <set>
#include "VirtualMac.h"
#include "VirtualMobilityManager.h"
#include "WppPacket_m.h"

using namespace std;

enum WppMacTimers {
    TIMER_WPP_REBROADCAST = 1
};

class WppMac : public VirtualMac {
  protected:
    // Parameters
    double maxBroadcastRange;
    double jitterMax;
    int maxHopCount;
    int seqCounter;

    // Con trỏ truy xuất tọa độ
    VirtualMobilityManager *mobilityModule;

    // Quản lý chống lặp (Duplicate Detection)
    set<pair<int, int>> seenPackets;

    // Hàng đợi phát lại kèm Jitter
    queue<WppPacket*> rebroadcastQueue;

    // Các hàm nòng cốt của VirtualMac
    virtual void startup() override;
    virtual void fromNetworkLayer(cPacket *pkt, int destination) override;
    virtual void fromRadioLayer(cPacket *pkt, double rssi, double lqi) override;
    virtual void timerFiredCallback(int index) override;
    virtual int handleRadioControlMessage(cMessage *msg) override;

    // Các hàm phụ trợ
    virtual bool alreadySeen(int src, int seq);
    virtual double distance(double x1, double y1, double x2, double y2);
    virtual void rebroadcastPacket(WppPacket *pkt);

  public:
    virtual ~WppMac();
};

#endif
