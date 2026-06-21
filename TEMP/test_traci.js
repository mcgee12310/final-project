const traci = require('traci');
const { spawn } = require('child_process');

async function runTest() {
    console.log("⏳ Đang khởi động SUMO và ép mở cổng TraCI 8813...");

    // 1. Tự khởi động SUMO chạy ngầm và yêu cầu mở cổng 8813
    const sumoProcess = spawn('sumo', ['-c', 'map.sumocfg', '--remote-port', '8813']);

    // Bắt lỗi nếu SUMO không chạy được
    sumoProcess.stderr.on('data', (data) => {
        console.error(`[SUMO LOG]: ${data}`);
    });

    // ĐỢI 2 GIÂY để SUMO kịp tải bản đồ và mở mạng xong
    await new Promise(resolve => setTimeout(resolve, 2000));

    try {
        console.log("🔌 Đang kết nối Node.js vào SUMO...");
        
        // 2. Kết nối TraCI vào cổng 8813 (Cách viết phổ biến của các thư viện JS)
        const client = new traci.Connection({ port: 8813 });
        await client.connect();
        
        console.log("🟢 Kết nối thành công! Đang đọc 50 bước...\n");

        for (let i = 0; i < 50; i++) {
            // Bước tới 1 khoảng thời gian
            await client.simulation.step(); 

            // Lấy danh sách xe
            const vehicles = await client.vehicle.getIDList();
            
            if (vehicles.length > 0) {
                const vid = vehicles[0]; 
                
                const pos = await client.vehicle.getPosition(vid);
                const speed = await client.vehicle.getSpeed(vid);
                
                console.log(`[Bước ${i}] Xe ${vid} | Tọa độ: (X: ${pos[0].toFixed(2)}, Y: ${pos[1].toFixed(2)}) | Vận tốc: ${speed.toFixed(2)} m/s`);
                
                // Ép xe dừng lại ở bước 20
                if (i === 20) {
                    console.log(`\n🛑 LỆNH ĐIỀU KHIỂN: Ép xe [${vid}] phanh gấp về 0 m/s!\n`);
                    await client.vehicle.setSpeed(vid, 0);
                }
            } else {
                console.log(`[Bước ${i}] Chưa có xe...`);
            }
        }

        console.log("\n✅ Test xong. Đang đóng kết nối...");
        await client.close();
        sumoProcess.kill(); // Tắt tiến trình SUMO

    } catch (error) {
        console.error("🔴 Lỗi kịch bản:", error);
        sumoProcess.kill();
    }
}

runTest();
