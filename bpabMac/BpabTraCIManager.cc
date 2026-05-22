#include "BpabTraCIManager.h"
#include <sstream>
#include <cstdlib>

Define_Module(BpabTraCIManager);

// Khởi tạo biến static
int BpabTraCIManager::tcpClientSocket = -1;
std::ofstream BpabTraCIManager::unifiedLog;
BpabTraCIManager* BpabTraCIManager::instance = nullptr; // Con trỏ static lưu instance duy nhất
std::map<int, bool> BpabTraCIManager::nodeIntersectionStatus;

BpabTraCIManager::~BpabTraCIManager() {
    cancelAndDelete(pollTimer);
    if (unifiedLog.is_open()) unifiedLog.close();

    if (instance == this) instance = nullptr;

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
    instance = this; // Lưu vết instance đang chạy

    if (!unifiedLog.is_open()) {
        unifiedLog.open("unified_trace.txt", std::ios::out);
    }

    tcpServerSocket = -1;
    tcpClientSocket = -1;
    tcpPort = par("tcpPort");
    pollInterval = par("pollInterval").doubleValue();

    setupTcpServer();

    #ifdef _WIN32
        EV << "=== [TraCIManager] Đang tự động khởi chạy SUMO Bridge... ===\n";
        system("start python bridge.py");
    #else
        system("python3 bridge.py &");
    #endif

    // Bắt đầu vòng lặp quét TCP độc lập (không dính dáng đến các Node)
    pollTimer = new cMessage("pollTimer");
    scheduleAt(simTime() + pollInterval, pollTimer);
}

// HÀM STATIC: ĐƯỢC GỌI TỪ TẦNG MAC/APP ĐỂ ÉP VÉT SẠCH BUFFER TCP TỨC THỜI
void BpabTraCIManager::forceInstantTraCISync() {
    if (instance && tcpClientSocket != -1) {
        cContextSwitcher tmp(instance);

        // VÒNG LẶP VÉT SẠCH: Đọc cho đến khi không còn gì trong socket
        // hoặc đã bắt kịp thời gian hiện tại
        bool hasMoreData = true;
        int safetyCounter = 0; // Tránh vòng lặp vô tận nếu dữ liệu đổ về quá nhanh

        while (hasMoreData && safetyCounter < 100) {
            char buffer[4096]; // Tăng buffer lên 4KB
            int bytesRead = recv(tcpClientSocket, buffer, sizeof(buffer) - 1, 0);

            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                std::string data(buffer);
                std::stringstream ss(data);
                std::string cmd;
                while (std::getline(ss, cmd, '\n')) {
                    if (!cmd.empty()) {
                        instance->processCommand(cmd);
                    }
                }
                safetyCounter++;
            } else {
                // Không còn dữ liệu trong đệm OS
                hasMoreData = false;
            }
        }
    }
}

void BpabTraCIManager::writeToUnifiedLog(double time, int nodeId, std::string event, std::string data) {
    if (unifiedLog.is_open()) {
        unifiedLog << time << "\t" << nodeId << "\t" << event << "\t" << data << "\n";
        unifiedLog.flush(); // Đẩy dữ liệu ra ổ cứng ngay lập tức để Node.js đọc kịp
    }
}

void BpabTraCIManager::handleMessage(cMessage *msg) {
    // NẾU LÀ SỰ KIỆN DI CHUYỂN (Dành cho các mốc thời gian của tương lai)
    if (msg->isName("MoveEvent")) {
        // Mở hộp lấy dữ liệu
        int nodeId = msg->par("nodeId").longValue();
        double sumoTime = msg->par("sumoTime").doubleValue();
        double x = msg->par("x").doubleValue();
        double y = msg->par("y").doubleValue();
        int isInter = msg->par("isInter").longValue();

        char path[100];
        sprintf(path, "SN.node[%d].MobilityManager", nodeId);
        cModule *mobModule = simulation.getModuleByPath(path);

        if (mobModule) {
            VirtualMobilityManager *mob = check_and_cast<VirtualMobilityManager*>(mobModule);
            cContextSwitcher tmp(mobModule);
            mob->setLocation(x, y, 0); // Ép di chuyển!
            nodeIntersectionStatus[nodeId] = (isInter == 1);

            if (unifiedLog.is_open()) {
                // ĐÓNG GÓI LẠI GIỐNG HỆT ĐỊNH DẠNG CỦA MAC_EVENT
                std::ostringstream _ss;
                _ss << "EVENT:POS | Node:" << nodeId << " | x:" << x << " | y:" << y;

                // Đổi nhãn từ "POS" thành "MAC_EVENT" để đồng nhất 1 luồng
                writeToUnifiedLog(sumoTime, nodeId, "MAC_EVENT", _ss.str());
            }
        }

        delete msg;
    }
    // NẾU LÀ SỰ KIỆN ĐỌC TCP định kỳ
    else if (msg == pollTimer) {
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
    if (cmd.find("SET_POS") != std::string::npos) {
        size_t timePos = cmd.find("TIME:");
        size_t nodePos = cmd.find("NODE:");
        size_t xPos = cmd.find("X:");
        size_t yPos = cmd.find("Y:");

        int isInter = 0;
        size_t interPos = cmd.find("INTER:");
        if (interPos != std::string::npos) {
            sscanf(cmd.c_str() + interPos, "INTER:%d", &isInter);
        }

        if (timePos != std::string::npos && nodePos != std::string::npos &&
            xPos != std::string::npos && yPos != std::string::npos) {
            try {
                double sumoTime = std::stod(cmd.substr(timePos + 5, nodePos - (timePos + 5) - 1));
                int nodeId = std::stoi(cmd.substr(nodePos + 5, xPos - (nodePos + 5) - 1));
                double x = std::stod(cmd.substr(xPos + 2, yPos - (xPos + 2) - 1));
                double y = std::stod(cmd.substr(yPos + 2));

                if (isInter == 1) {
                    std::ostringstream ss;
                    ss << "EVENT:TRACI_PARSE_INTER | Node:" << nodeId << " | ParsedValue:" << isInter;
                    writeToUnifiedLog(simTime().dbl(), nodeId, "TRACI_DEBUG", ss.str());
                }

                // ---- CẢI TIẾN LẬP LỊCH TỐI ƯU ----
                // Nếu mốc tọa độ này thuộc về hiện tại hoặc quá khứ, ÁP DỤNG TRỰC TIẾP NGAY!
                if (sumoTime <= simTime().dbl()) {
                    char path[100];
                    sprintf(path, "SN.node[%d].MobilityManager", nodeId);
                    cModule *mobModule = simulation.getModuleByPath(path);

                    if (mobModule) {
                        VirtualMobilityManager *mob = check_and_cast<VirtualMobilityManager*>(mobModule);
                        cContextSwitcher tmp(mobModule);
                        mob->setLocation(x, y, 0); // Ghi đè trực tiếp tọa độ thực tế
                        nodeIntersectionStatus[nodeId] = (isInter == 1);

                        if (unifiedLog.is_open()) {
                            std::ostringstream _ss;
                            _ss << "EVENT:POS | Node:" << nodeId << " | x:" << x << " | y:" << y;
                            writeToUnifiedLog(sumoTime, nodeId, "MAC_EVENT", _ss.str());
                        }
                    }
                } else {
                    // Nếu dữ liệu gửi sang dành cho mốc thời gian tương lai, đưa vào Event Heap
                    cMessage *moveMsg = new cMessage("MoveEvent");
                    moveMsg->addPar("nodeId").setLongValue(nodeId);
                    moveMsg->addPar("sumoTime").setDoubleValue(sumoTime);
                    moveMsg->addPar("x").setDoubleValue(x);
                    moveMsg->addPar("y").setDoubleValue(y);
                    moveMsg->addPar("isInter").setLongValue(isInter);

                    scheduleAt(sumoTime, moveMsg);
                }
                // ----------------------------------

            } catch (...) {
                EV << "[TraCIManager] Lỗi Parse lệnh: " << cmd << "\n";
            }
        }
    }
}

bool BpabTraCIManager::isNodeAtIntersection(int nodeId) {
    if (nodeIntersectionStatus.find(nodeId) != nodeIntersectionStatus.end()) {
        return nodeIntersectionStatus[nodeId];
    }
    return false; // Mặc định nếu chưa có dữ liệu là không ở giao lộ
}

void BpabTraCIManager::finish() {
    // Để trống
}
