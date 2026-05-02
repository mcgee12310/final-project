#include "BpabApp.h"

Define_Module(BpabApp);

void BpabApp::startup() {
    // 1. Đọc tham số từ file .ini và file .ned
    sendInterval = par("sendInterval");
    isNode0Sender = par("isNode0Sender");
    packetSequenceNumber = 0;

    // 2. Kịch bản: Nếu là Node 0 và được phép gửi, lên lịch gửi gói tin đầu tiên
    if (isNode0Sender && self == 0) {
        // Sử dụng Timer số 1 của Castalia (thừa kế từ TimerService)
        setTimer(1, sendInterval);
        trace() << "[APP] Node 0 đã sẵn sàng gửi gói tin định kỳ!";
    } else {
//        trace() << "[APP] Node " << self << " khởi động ở chế độ Lắng nghe.";
    }
}

void BpabApp::timerFiredCallback(int timerIndex) {
    switch (timerIndex) {
        case 1: {
            // 1. Tạo một gói tin dữ liệu chung (Data: 0.0, Seq: packetSequenceNumber, Size: 64 bytes)
            ApplicationPacket *newPacket = createGenericDataPacket(0.0, packetSequenceNumber, 64);
            newPacket->setName("BPAB_DATA_PACKET");

            // 2. Đẩy xuống tầng Network (sau đó Network sẽ đẩy xuống MAC của bạn)
            // BROADCAST_NETWORK_ADDRESS là hằng số chuỗi "-1" mặc định của Castalia
            toNetworkLayer(newPacket, BROADCAST_NETWORK_ADDRESS);

            trace() << "[APP] Node " << self << " ĐÃ GỬI gói tin Seq: " << packetSequenceNumber;
            packetSequenceNumber++;

            // 3. Lên lịch vòng lặp gửi gói tin tiếp theo
            setTimer(1, sendInterval);
            break;
        }
        default:
            break;
    }
}

void BpabApp::fromNetworkLayer(ApplicationPacket *rcvPacket, const char *source, double rssi, double lqi) {
    // Hàm này được gọi khi Tầng MAC nhận thành công và bóc tách đẩy ngược lên đây
    trace() << "[APP] Node " << self << " ĐÃ NHẬN gói tin từ Node " << source
            << " | Seq: " << rcvPacket->getSequenceNumber()
            << " | Tín hiệu (RSSI): " << rssi << " dBm";
}
