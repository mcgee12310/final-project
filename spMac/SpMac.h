#ifndef _SPMAC_H_
#define _SPMAC_H_

#include <queue>
#include <set>
#include "VirtualMac.h"
#include "VirtualMobilityManager.h"
#include "SpPacket_m.h"

using namespace std;

enum SpMacTimers {
    TIMER_SP_REBROADCAST = 1
};

class SpMac : public VirtualMac {
  protected:
    double maxBroadcastRange;
    int maxHopCount;
    int numSlots;
    double slotDuration;
    double probabilityP;
    int seqCounter;

    VirtualMobilityManager *mobilityModule;
    set<pair<int, int>> seenPackets;
    queue<SpPacket*> rebroadcastQueue;

    virtual void startup() override;
    virtual void fromNetworkLayer(cPacket *pkt, int destination) override;
    virtual void fromRadioLayer(cPacket *pkt, double rssi, double lqi) override;
    virtual void timerFiredCallback(int index) override;
    virtual int handleRadioControlMessage(cMessage *msg) override;

    virtual bool alreadySeen(int src, int seq);
    virtual double distance(double x1, double y1, double x2, double y2);
    virtual void scheduleRebroadcast(SpPacket *pkt, double delay);

  public:
    virtual ~SpMac();
};

#endif
