#ifndef _BPABTRACIMANAGER_H_
#define _BPABTRACIMANAGER_H_

#include <omnetpp.h>
#include "VirtualMobilityManager.h" // ThÆ° viá»‡n di chuyá»ƒn cá»§a Castalia

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
    // Ä�áº·t public static Ä‘á»ƒ BpabMac cÃ³ thá»ƒ truy cáº­p vÃ  gá»­i Log (WEBLOG)
    static int tcpClientSocket;
    static void writeToUnifiedLog(double time, int nodeId, std::string event, std::string data);
    static BpabTraCIManager* instance;
    static void forceInstantTraCISync();

  protected:
    static std::ofstream unifiedLog;

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
