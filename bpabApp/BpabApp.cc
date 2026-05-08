#include "BpabApp.h"
#include <cstdio>   // Để dùng sprintf
#include <sstream>  // Để dùng std::ostringstream
#include "BpabTraCIManager.h" // Nhúng thư viện ghi log chung

Define_Module(BpabApp);

// =====================================================================
// MACRO GHI LOG CHO TẦNG APP (Tương tự WEBLOG của MAC)
// =====================================================================
#define APP_LOG(msgArgs) \
    do { \
        std::ostringstream _ss; \
        _ss << msgArgs; \
        trace() << _ss.str(); \
        /* --- GHI VÀO FILE LOG CHUNG --- */ \
        BpabTraCIManager::writeToUnifiedLog(simTime().dbl(), self, "APP_EVENT", _ss.str()); \
        /* --- GỬI QUA SOCKET CHO VISUALIZER --- */ \
        if (BpabTraCIManager::tcpClientSocket != -1) { \
            std::ostringstream _netSs; \
            _netSs << simTime().dbl() << " APP " << _ss.str() << "\n"; \
            std::string _msg = _netSs.str(); \
            ::send(BpabTraCIManager::tcpClientSocket, _msg.c_str(), _msg.length(), 0); \
        } \
    } while(0)
// =====================================================================


void BpabApp::startup() {
    sendInterval = par("sendInterval");
    isNode0Sender = par("isNode0Sender");
    packetSequenceNumber = 0;

    if (isNode0Sender && self == 0) {
        setTimer(1, sendInterval);
        APP_LOG("EVENT:APP_START | Node:0 | Status:SENDER_READY");
    } else {
        // APP_LOG("EVENT:APP_START | Node:" << self << " | Status:LISTENER");
    }
}

void BpabApp::timerFiredCallback(int timerIndex) {
    switch (timerIndex) {
        case 1: {
            double sensorData = 36.5 + packetSequenceNumber * 0.1;
            ApplicationPacket *newPacket = createGenericDataPacket(sensorData, packetSequenceNumber, 64);

            char payload[100];
            sprintf(payload, "Hello VANET! Nhiet do: %.1f", sensorData);
            newPacket->setName(payload);

            toNetworkLayer(newPacket, BROADCAST_NETWORK_ADDRESS);

            // THAY THẾ TRACE CŨ BẰNG APP_LOG
            APP_LOG("EVENT:APP_SEND | Node:" << self
                    << " | Seq:" << packetSequenceNumber
                    << " | Payload:[" << payload << "]");

            packetSequenceNumber++;
            setTimer(1, sendInterval);
            break;
        }
        default:
            break;
    }
}

void BpabApp::fromNetworkLayer(ApplicationPacket *rcvPacket, const char *source, double rssi, double lqi) {
    // THAY THẾ TRACE CŨ BẰNG APP_LOG
    APP_LOG("EVENT:APP_RCV | Node:" << self
            << " | From:" << source
            << " | Seq:" << rcvPacket->getSequenceNumber()
            << " | Payload:[" << rcvPacket->getName() << "]"
            << " | RSSI:" << rssi << "dBm");
}
