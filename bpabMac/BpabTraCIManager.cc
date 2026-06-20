#include "BpabTraCIManager.h"
#include <sstream>
#include <cstdlib>
#include <cmath>

Define_Module(BpabTraCIManager);

// Khởi tạo biến static
int BpabTraCIManager::tcpClientSocket = -1;
std::ofstream BpabTraCIManager::unifiedLog;
BpabTraCIManager* BpabTraCIManager::instance = nullptr;
std::map<int, bool> BpabTraCIManager::nodeIntersectionStatus;

BpabTraCIManager::~BpabTraCIManager() {
    cancelAndDelete(pollTimer);
    if (unifiedLog.is_open()) unifiedLog.close();
    if (instance == this) instance = nullptr;
    // Đóng socket (giữ nguyên code cũ của bạn)
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

void BpabTraCIManager::initialize() {
    instance = this;
    nodeIntersectionStatus.clear(); // Xóa sạch dữ liệu cũ khi khởi động lại

    if (!unifiedLog.is_open()) {
        unifiedLog.open("unified_trace.txt", std::ios::out);
    }

    tcpServerSocket = -1;
    tcpClientSocket = -1;
    tcpPort = par("tcpPort");
    pollInterval = par("pollInterval").doubleValue();

    setupTcpServer();

    // Khởi động bridge python
#ifdef _WIN32
    // /k = giu cua so lai sau khi lenh chay xong
    system("start cmd /k python bridge.py");
#else
    // Giu terminal lai bang cach them; exec bash
    system("xterm -hold -e 'python bridge.py' &");
#endif

    pollTimer = new cMessage("pollTimer");
    scheduleAt(simTime() + pollInterval, pollTimer);
}

// Cập nhật trạng thái giao lộ vào Map
void BpabTraCIManager::updateIntersectionStatus(int nodeId, int isInter) {
    nodeIntersectionStatus[nodeId] = (isInter == 1);
}

void BpabTraCIManager::handleMessage(cMessage *msg) {
    if (msg->isName("MoveEvent")) {
        int nodeId = msg->par("nodeId").longValue();
        double sumoTime = msg->par("sumoTime").doubleValue();
        double x = msg->par("x").doubleValue();
        double y = msg->par("y").doubleValue();
        int isInter = msg->par("isInter").longValue();

        // Cập nhật trạng thái
        updateIntersectionStatus(nodeId, isInter);

        char path[100];
        sprintf(path, "SN.node[%d].MobilityManager", nodeId);
        cModule *mobModule = simulation.getModuleByPath(path);

        if (mobModule) {
            VirtualMobilityManager *mob = check_and_cast<VirtualMobilityManager*>(mobModule);
            cContextSwitcher tmp(mobModule);
            mob->setLocation(x, y, 0);

            if (unifiedLog.is_open()) {
                std::ostringstream _ss;
                _ss << "EVENT:POS | Node:" << nodeId << " | x:" << x << " | y:" << y
                    << " | Inter:" << (isInter ? "TRUE" : "FALSE");
                writeToUnifiedLog(sumoTime, nodeId, "MAC_EVENT", _ss.str());
            }
        }
        delete msg;
    } else if (msg == pollTimer) {
        pollTcpSocket();
        scheduleAt(simTime() + pollInterval, pollTimer);
    }
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
//    return;
    // Parser an toàn hơn
    try {
        if (cmd.find("SET_POS") != std::string::npos) {
            std::stringstream ss(cmd);
            std::string segment;

            double sumoTime = 0, x = 0, y = 0;
            int nodeId = 0, isInter = 0;

            // Định dạng lệnh: SET_POS|TIME:10.0|NODE:5|X:100.0|Y:200.0|INTER:1
            // Dùng getline với delimiter '|' để tách lệnh
            while(std::getline(ss, segment, '|')) {
                if (segment.find("TIME:") == 0) sumoTime = std::stod(segment.substr(5));
                else if (segment.find("NODE:") == 0) nodeId = std::stoi(segment.substr(5));
                else if (segment.find("X:") == 0) x = std::stod(segment.substr(2));
                else if (segment.find("Y:") == 0) y = std::stod(segment.substr(2));
                else if (segment.find("INTER:") == 0) isInter = std::stoi(segment.substr(6));
            }

            // Logic cập nhật
            if (sumoTime <= simTime().dbl()) {
                // Áp dụng tức thời
                updateIntersectionStatus(nodeId, isInter);

                char path[100];
                sprintf(path, "SN.node[%d].MobilityManager", nodeId);
                cModule *mobModule = simulation.getModuleByPath(path);
                if (mobModule) {
                    VirtualMobilityManager *mob = check_and_cast<VirtualMobilityManager*>(mobModule);
                    cContextSwitcher tmp(mobModule);
                    mob->setLocation(x, y, 0);

                    if (unifiedLog.is_open()) {
                        std::ostringstream _ss;
                        _ss << "EVENT:POS | Node:" << nodeId << " | x:" << x << " | y:" << y;
                        writeToUnifiedLog(sumoTime, nodeId, "MAC_EVENT", _ss.str());
                    }
                }
            } else {
                // Đưa vào Event Heap (Dữ liệu tương lai)
                cMessage *moveMsg = new cMessage("MoveEvent");
                moveMsg->addPar("nodeId").setLongValue(nodeId);
                moveMsg->addPar("sumoTime").setDoubleValue(sumoTime);
                moveMsg->addPar("x").setDoubleValue(x);
                moveMsg->addPar("y").setDoubleValue(y);
                moveMsg->addPar("isInter").setLongValue(isInter);
                scheduleAt(sumoTime, moveMsg);
            }
        }
    } catch (const std::exception& e) {
        EV << "[TraCIManager] Lỗi Parse: " << e.what() << " - CMD: " << cmd << "\n";
    }
}

// Cung cấp hàm getter chuẩn để các module khác (Mac/App) sử dụng
bool BpabTraCIManager::isNodeAtIntersection(int nodeId) {
    if (nodeIntersectionStatus.count(nodeId)) {
        return nodeIntersectionStatus[nodeId];
    }
    return false;
}

void BpabTraCIManager::writeToUnifiedLog(double time, int nodeId, std::string event, std::string data) {
    if (unifiedLog.is_open()) {
        unifiedLog << time << "\t" << nodeId << "\t" << event << "\t" << data << "\n";
        unifiedLog.flush(); // Đẩy dữ liệu ra ổ cứng ngay lập tức để Node.js đọc kịp
    }
}

void BpabTraCIManager::finish() {
    // Hàm này phải có, dù là rỗng
}
