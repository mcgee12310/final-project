#ifndef _BPABTRACIMANAGER_H_
#define _BPABTRACIMANAGER_H_

#include <omnetpp.h>
#include <map>
#include <fstream>
#include <string>
#include <vector>
#include "VirtualMobilityManager.h"

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
    // Các biến static để module khác truy cập toàn cục
    static int tcpClientSocket;
    static std::map<int, bool> nodeIntersectionStatus; // Lưu trạng thái: Key=nodeId, Val=isNearIntersection
    static BpabTraCIManager* instance;                 // Dùng để gọi các hàm instance từ context static

    // Các hàm static helper
    static void writeToUnifiedLog(double time, int nodeId, std::string event, std::string data);
    static bool isNodeAtIntersection(int nodeId);      // Kiểm tra trạng thái
    static void updateIntersectionStatus(int nodeId, int isInter); // CẬP NHẬT TRẠNG THÁI (MỚI)

  protected:
    static std::ofstream unifiedLog;

    int tcpServerSocket;
    int tcpPort;
    cMessage *pollTimer;
    double pollInterval;

    // Các hàm override của OMNeT++
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual ~BpabTraCIManager();

    // Các hàm xử lý TCP và TraCI
    void setupTcpServer();
    void pollTcpSocket();
    void processCommand(const std::string& cmd);
};

#endif
