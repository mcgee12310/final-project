const net = require('net');
const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 8080 });
console.log("🟢 WebSocket server đang chờ trình duyệt ở port 8080...");

let castaliaClient = null;

// Buffer lưu log khi chưa có browser nào kết nối
const logBuffer = [];
const MAX_BUFFER = 500;

// Broadcast tới tất cả WebSocket clients
function broadcast(line) {
    let sent = false;
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(line);
            sent = true;
        }
    });
    // Nếu chưa có browser nào → buffer lại
    if (!sent) {
        logBuffer.push(line);
        if (logBuffer.length > MAX_BUFFER) logBuffer.shift();
    }
}

// Khi browser kết nối → flush buffer cũ ra trước
wss.on('connection', (ws) => {
    console.log("🌐 Browser đã kết nối WebSocket!");

    // Gửi lại toàn bộ log đã buffer
    if (logBuffer.length > 0) {
        console.log(`📦 Flush ${logBuffer.length} dòng log đã buffer...`);
        logBuffer.forEach(line => ws.send(line));
        logBuffer.length = 0;
    }

    // Nhận lệnh điều khiển từ Web UI → gửi về Castalia
    ws.on('message', (message) => {
        console.log("📩 Nhận lệnh điều khiển từ Web:", message.toString());
        if (castaliaClient && !castaliaClient.destroyed) {
            castaliaClient.write(message.toString() + "\n");
        }
    });
});

function connectToCastalia() {
    console.log("⏳ Đang thử kết nối tới Castalia (Port 9999)...");
    castaliaClient = new net.Socket();

    // FIX: TCP stream có thể gộp nhiều dòng → dùng buffer riêng để split \n
    let tcpBuffer = "";

    castaliaClient.connect(9999, '127.0.0.1', () => {
        console.log("🔵 Đã kết nối thành công tới Castalia qua TCP Socket!");
    });

    castaliaClient.on('data', (data) => {
        tcpBuffer += data.toString();

        // Tách từng dòng hoàn chỉnh (kết thúc bằng \n)
        const lines = tcpBuffer.split('\n');

        // Dòng cuối có thể chưa hoàn chỉnh → giữ lại trong buffer
        tcpBuffer = lines.pop();

        lines.forEach(line => {
            if (line.trim()) {
                // console.log("📨 Castalia →", line); // Debug ở terminal
                broadcast(line);
            }
        });
    });

    castaliaClient.on('close', () => {
        console.log("🔴 Castalia đã ngắt kết nối. Thử lại sau 3 giây...");
        castaliaClient.destroy();
        setTimeout(connectToCastalia, 3000);
    });

    castaliaClient.on('error', () => {
        // Lỗi sẽ trigger 'close' → không cần xử lý thêm
    });
}

connectToCastalia();
