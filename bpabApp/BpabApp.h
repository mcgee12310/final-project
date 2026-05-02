#ifndef _BPABAPP_H_
#define _BPABAPP_H_

#include "VirtualApplication.h"

class BpabApp : public VirtualApplication {
  private:
    double sendInterval;
    bool isNode0Sender;
    int packetSequenceNumber;

  protected:
    // Các hàm bắt buộc phải ghi đè (override) từ VirtualApplication
    virtual void startup() override;
    virtual void fromNetworkLayer(ApplicationPacket *rcvPacket, const char *source, double rssi, double lqi) override;
    virtual void timerFiredCallback(int timerIndex) override;
};

#endif
