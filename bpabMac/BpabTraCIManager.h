#ifndef _BPABTRACIMANAGER_H_
#define _BPABTRACIMANAGER_H_

#include <omnetpp.h>
#include "VirtualMobilityManager.h" // Thư viện di chuyển của Castalia

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

class BpabTraCIManager : public cSimpleModule {
  public:
    // Đặt public static để BpabMac có thể truy cập và gửi Log (WEBLOG)
    static int tcpClientSocket;

  protected:
    int tcpServerSocket;
    int tcpPort;
    cMessage *pollTimer;
    double pollInterval;

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual ~BpabTraCIManager();

    void setupTcpServer();
    void pollTcpSocket();
    void processCommand(const std::string& cmd);
};

#endif
