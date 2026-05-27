#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

#ifndef _BPABMAC_H_
#define _BPABMAC_H_

#include "VirtualMac.h"
#include "BPABPacket_m.h"
#include "node/mobilityManager/VirtualMobilityManager.h"

using namespace std;

#define BROADCAST_INTER (-2)

enum BpabMacState {
    BPAB_IDLE             = 1,
    BPAB_CONTENDING       = 2,
    BPAB_TRANSMITTING     = 3,
    BPAB_WAITING_BB       = 4,
    BPAB_WAIT_CTB         = 5,
    BPAB_WAIT_DATA        = 6,
    BPAB_PRE_CTB          = 7,
    BPAB_WAIT_ACK         = 8,
    BPAB_INTER_CONTENDING = 9
};

class BpabMac: public VirtualMac {
 private:
    // --- Tham so NED ---
    int    maxIterations;
    double rangeR;
    double widthW;
    double minProgress;
    double slotDuration;
    int    maxRetries;

    // --- Trang thai chung ---
    double myX, myY;
    double lastX, lastY;
    double myDistanceToSrc;
    int    srcDirection;
    double slotStartTime;
    int    retryCount;
    int    transmissionDirection;
    int    lastRTBDirection;      // Hướng RTB vừa gửi (INTER hoặc thường)
    int    lastDataDestId;        // ID relay vừa gửi DATA để nhận ACK ngầm

    BpabMacState bpabMacState;
    int    currentIteration;
    double limitL, limitU;
    bool   heardBB;
    bool   heardCTB;
    bool   isTransmitting;

    // --- Thông tin nguồn ---
    VirtualMobilityManager* mobilityModule;
    double srcX, srcY;
    int    srcId;

    // --- Buffer gói ---
    BPABPacket *packetToBroadcast;

    // --- Giao lộ: node nguồn ---
    bool   heardAnyCTB;
    int    incomingBranchDir;     // Hướng nguồn đến giao lộ (nhánh bị loại trừ)

    // --- Giao lộ: node relay ---
    int    interSrcId;
    double interSrcX, interSrcY;
    int    myInterBranch;         // Nhánh relay thuộc về
    int    myInterZone;           // Zone của relay (0=Outer, 1=Mid, 2=Inner)
    int    interZonePhase;        // Pha đang xử lý (0, 1, 2)
    double interSlotStart;        // Thời điểm bắt đầu slot nhánh này
    bool   heardBBInZone;

    // --- Hàm hỗ trợ chung ---
    bool   isValidForwardNode(double myX, double myY,
                              double srcX, double srcY,
                              int direction, double rangeR);
    void   preparePacket(cPacket *netPkt);
    void   sendRTB();
    void   sendCTB();
    void   sendData(int winnerId);
    void   sendBlackBurst();
    void   endContention(bool won);
    int    calculateTransmissionDirection();
    int    getIncomingBranch(double sX, double sY, double mX, double mY);

    // --- Hàm hỗ trợ giao lộ (Đã cập nhật theo Radial Zoning) ---
    void   sendInterRTB(int excludeDir);
    void   sendInterBroadcastData();
    int    getInterBranch(double mX, double mY, double sX, double sY);
    int    getInterZone(double dist);
    int    getBranchSlotOffset(int branch, int excludeDir);
    double interTotalTimeout(int excludeDir);

 protected:
    void startup();
    void fromNetworkLayer(cPacket *, int);
    void fromRadioLayer(cPacket *, double, double);
    int  handleRadioControlMessage(cMessage *msg);
    void timerFiredCallback(int);

 public:
    BpabMac() : VirtualMac() {}
    virtual ~BpabMac();
};

#endif
