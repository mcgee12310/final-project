#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    // Định nghĩa kiểu socklen_t cho Windows
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

// Dinh nghia cac trang thai cua giao thuc BPAB tai tang MAC
enum BpabMacState {
    BPAB_IDLE = 1,          // Dang ranh, doi goi tin tu App hoac Radio
    BPAB_CONTENDING = 2,    // Dang trong qua trinh tranh chap nhi phan (N vong lap)
    BPAB_TRANSMITTING = 3,  // Dang phat du lieu thuc te sau khi thang tranh chap
    BPAB_WAITING_BB = 4,    // Dang nghe Xung den (Black Burst) tu cac xe khac
    BPAB_WAIT_CTB = 5,
    BPAB_WAIT_DATA = 6,
    BPAB_PRE_CTB = 7
};

class BpabMac: public VirtualMac {
 private:
    // --- Cac tham so doc tu file .ned ---
    int maxIterations;        // N: So vong lap toi da
    double rangeR;            // R: Pham vi truyen dan
    double widthW;
    double minProgress;
    double slotDuration;      // Thoi gian cua mot khe (slot)
    double bbTxPower;         // Cong suat phat Xung den
    double rssiThreshold;     // Nguong nang luong de nhan dien Xung den

    // --- Cac bien trang thai noi bo ---
    int myId;
    double myX;
    double myY;
    double myDistanceToSrc;
    BpabMacState bpabMacState; // Trang thai hien tai cua MAC
    int currentIteration;      // Vong lap hien tai (i)
    double limitL;             // Ranh gioi duoi (Lower bound)
    double limitU;             // Ranh gioi tren (Upper bound)
    bool heardBB;
    bool heardCTB;
    bool isTransmitting;

    int retryCount;
    int maxRetries;

    // Luu tru thong tin xe nguon (Initiator)
    VirtualMobilityManager* mobilityModule;
    double srcX;
    double srcY;
    int srcId;

    // Goi tin dang cho de gui (Buffer)
    BPABPacket *packetToBroadcast;

    // --- Cac ham ho tro thuat toan ---
    bool isValidForwardNode(double myX, double myY,
                                     double srcX, double srcY,
                                     int direction,
                                     double rangeR);
    void startBpabTransmission(cPacket *netPkt);
    void sendBlackBurst();      // Ham phat xung den vat ly
    void endContention(bool won); // Ket thuc tranh chap (Thang/Thua)

 protected:
    // --- Cac ham bat buoc cua Castalia VirtualMac ---
    void startup();
    void fromNetworkLayer(cPacket *, int);
    void fromRadioLayer(cPacket *, double, double);
    int handleRadioControlMessage(cMessage *msg);
    void timerFiredCallback(int);

 public:
    // Constructor va Destructor
    BpabMac() : VirtualMac() {}
    virtual ~BpabMac();
};

#endif
