const WebSocket = require('ws');
const { Tail } = require('tail');

const wss = new WebSocket.Server({ port: 8080 });
console.log("🚀 WebSocket Server đang chạy ở cổng 8080");

// KHÔNG DÙNG net.Socket NỮA. CHÚNG TA ĐỌC TRỰC TIẾP TỪ FILE LOG CỦA CASTALIA!
const logFilePath = 'D:\\omnetpp-4.6\\samples\\castalia\\Simulations\\BPAB\\unified_trace.txt'; // Sửa lại đúng đường dẫn thư mục C++

try {
    const tail = new Tail(logFilePath);
    
    console.log(`🎧 Đang lắng nghe file log: ${logFilePath}`);

    // Bất cứ khi nào C++ ghi thêm 1 dòng mới vào file
    tail.on("line", function(data) {
        // Broadcast dòng log này cho tất cả trình duyệt Web đang kết nối
        wss.clients.forEach(function each(client) {
            if (client.readyState === WebSocket.OPEN) {
                client.send(data);
            }
        });
    });

    tail.on("error", function(error) {
        console.log('🔴 Lỗi đọc file: ', error);
    });

} catch (error) {
    console.log("⚠️ Chưa tìm thấy file log. Hãy chạy Castalia trước để nó tạo file nhé!");
}
