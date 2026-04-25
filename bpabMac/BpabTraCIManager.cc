#include "BpabTraCIManager.h"
#include <sstream>

Define_Module(BpabTraCIManager);

// Khởi tạo biến static
int BpabTraCIManager::tcpClientSocket = -1;

BpabTraCIManager::~BpabTraCIManager() {
    cancelAndDelete(pollTimer);

    if (tcpClientSocket != -1) {
        #ifdef _WIN32
            closesocket(tcpClientSocket);
        #else
            close(tcpClientSocket);
        #endif
    }
    if (tcpServerSocket != -1) {
        #ifdef _WIN32
            closesocket(tcpServerSocket);
            WSACleanup();
        #else
            close(tcpServerSocket);
        #endif
    }
}

void BpabTraCIManager::initialize() {
    tcpServerSocket = -1;
    tcpClientSocket = -1;
    tcpPort = par("tcpPort");
    pollInterval = par("pollInterval").doubleValue();

    setupTcpServer();

    // Bắt đầu vòng lặp quét TCP độc lập (không dính dáng đến các Node)
    pollTimer = new cMessage("pollTimer");
    scheduleAt(simTime() + pollInterval, pollTimer);
}

void BpabTraCIManager::handleMessage(cMessage *msg) {
    if (msg == pollTimer) {
        pollTcpSocket();
        scheduleAt(simTime() + pollInterval, pollTimer);
    }
}

void BpabTraCIManager::setupTcpServer() {
    #ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif

    tcpServerSocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(tcpPort);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(tcpServerSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(tcpServerSocket, 1);

    // Thiết lập Non-blocking
    #ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(tcpServerSocket, FIONBIO, &mode);
    #else
        int flags = fcntl(tcpServerSocket, F_GETFL, 0);
        fcntl(tcpServerSocket, F_SETFL, flags | O_NONBLOCK);
    #endif

    EV << "=== [TraCIManager] Giao diện SUMO/Web đã mở tại Port " << tcpPort << " ===\n";
}

void BpabTraCIManager::pollTcpSocket() {
    if (tcpClientSocket == -1) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        tcpClientSocket = accept(tcpServerSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (tcpClientSocket != -1) {
            #ifdef _WIN32
                u_long mode = 1;
                ioctlsocket(tcpClientSocket, FIONBIO, &mode);
            #else
                int flags = fcntl(tcpClientSocket, F_GETFL, 0);
                fcntl(tcpClientSocket, F_SETFL, flags | O_NONBLOCK);
            #endif
            EV << "=== [TraCIManager] Bộ điều khiển bên ngoài đã kết nối! ===\n";
        }
    } else {
        char buffer[2048];
        int bytesRead = recv(tcpClientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string data(buffer);

            // Xử lý trường hợp Python gửi nhiều lệnh dính nhau bằng \n
            std::stringstream ss(data);
            std::string cmd;
            while (std::getline(ss, cmd, '\n')) {
                if (!cmd.empty()) {
                    processCommand(cmd);
                }
            }
        } else if (bytesRead == 0) {
            tcpClientSocket = -1;
            EV << "=== [TraCIManager] Bộ điều khiển đã ngắt kết nối. ===\n";
        }
    }
}

void BpabTraCIManager::processCommand(const std::string& cmd) {
    // Phân tích: SET_POS|TIME:1.20|NODE:5|X:100.5|Y:200.2
    if (cmd.find("SET_POS") != std::string::npos) {
        size_t timePos = cmd.find("TIME:");
        size_t nodePos = cmd.find("NODE:");
        size_t xPos = cmd.find("X:");
        size_t yPos = cmd.find("Y:");

        if (timePos != std::string::npos && nodePos != std::string::npos &&
            xPos != std::string::npos && yPos != std::string::npos) {
            try {
                // Bóc tách dữ liệu
                double sumoTime = std::stod(cmd.substr(timePos + 5, nodePos - (timePos + 5) - 1));
                int nodeId = std::stoi(cmd.substr(nodePos + 5, xPos - (nodePos + 5) - 1));
                double x = std::stod(cmd.substr(xPos + 2, yPos - (xPos + 2) - 1));
                double y = std::stod(cmd.substr(yPos + 2));

                // 1. DỊCH CHUYỂN XE TRONG CASTALIA
                char path[100];
                sprintf(path, "SN.node[%d].MobilityManager", nodeId);
                cModule *mobModule = simulation.getModuleByPath(path);

                if (mobModule) {
                    VirtualMobilityManager *mob = check_and_cast<VirtualMobilityManager*>(mobModule);
                    cContextSwitcher tmp(mobModule);
                    mob->setLocation(x, y, 0);
                    EV << "UPDATE_POS | Node:" << nodeId << " | x:" << x << " | y:" << y << " | SumoTime:" << sumoTime << "\n";

                    // 2. GỬI LÊN WEB BẰNG ĐỒNG HỒ CỦA SUMO (sumoTime)
                    std::ostringstream _netSs;
                    _netSs << sumoTime << " MAC EVENT:POS | Node:" << nodeId << " | x:" << x << " | y:" << y << "\n";
                    std::string _msg = _netSs.str();
                    ::send(tcpClientSocket, _msg.c_str(), _msg.length(), 0);
                }
            } catch (...) {
                EV << "[TraCIManager] Lỗi Parse lệnh: " << cmd << "\n";
            }
        }
    }
}

void BpabTraCIManager::finish() {
    // Để trống
}
