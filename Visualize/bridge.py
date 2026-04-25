import traci
import socket
import time
import sys

# ==========================================
# CẤU HÌNH HỆ THỐNG
# ==========================================
SUMO_CONFIG_FILE = "map.sumocfg"  # Đổi tên này cho khớp với file cấu hình của bạn
CASTALIA_IP = "127.0.0.1"
CASTALIA_PORT = 9999
STEP_LENGTH = 0.1  # Bước nhảy thời gian của SUMO (0.1 giây)
PLAYBACK_SPEED = 1.0 # Tốc độ thời gian thực (Để 2.0 nếu muốn xe chạy nhanh gấp đôi)
OFFSET = 5000.0

def main():
    # 1. KẾT NỐI VỚI CASTALIA (Cổng 9999)
    try:
        print(f"⏳ Đang tìm kiếm Castalia tại {CASTALIA_IP}:{CASTALIA_PORT}...")
        castalia_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        castalia_sock.connect((CASTALIA_IP, CASTALIA_PORT))
        print("🟢 Đã cắm dây mạng thành công vào Castalia!")
    except ConnectionRefusedError:
        print("🔴 LỖI: Không tìm thấy Castalia. Hãy gõ lệnh chạy Castalia C++ trước khi bật file Python này!")
        sys.exit(1)

    # 2. KHỞI ĐỘNG SUMO
    # Dùng "sumo" thay vì "sumo-gui" để chạy ngầm hoàn toàn (Headless Mode)
    sumoCmd = ["sumo", "-c", SUMO_CONFIG_FILE, "--step-length", str(STEP_LENGTH)]
    
    print("⏳ Đang khởi động lõi mô phỏng SUMO...")
    try:
        traci.start(sumoCmd)
        print("🟢 SUMO đã chạy ngầm! Đang bắt đầu truyền tọa độ...\n")
    except Exception as e:
        print(f"🔴 LỖI khi bật SUMO: {e}")
        castalia_sock.close()
        sys.exit(1)

    # 3. VÒNG LẶP THỜI GIAN THỰC (ĐỒNG BỘ TỌA ĐỘ)
    try:
        # Chạy chừng nào vẫn còn xe trên bản đồ
        while traci.simulation.getMinExpectedNumber() > 0:
            traci.simulationStep()
            
            # Lấy thời gian hiện tại của SUMO (Master Clock)
            current_sumo_time = traci.simulation.getTime()
            
            vehicle_ids = traci.vehicle.getIDList()
            for vid in vehicle_ids:
                x_sumo, y_sumo = traci.vehicle.getPosition(vid)
                node_id = int(''.join(filter(str.isdigit, vid)))
                
                # CỘNG OFFSET ĐỂ KHỬ SỐ ÂM CHO CASTALIA
                x_castalia = x_sumo + OFFSET
                y_castalia = y_sumo + OFFSET
                
                cmd = f"SET_POS|TIME:{current_sumo_time:.2f}|NODE:{node_id}|X:{x_castalia:.2f}|Y:{y_castalia:.2f}\n"
                castalia_sock.sendall(cmd.encode('utf-8'))
            
            time.sleep(STEP_LENGTH / PLAYBACK_SPEED)

    except KeyboardInterrupt:
        print("\n🛑 Đã dừng đồng bộ (Ctrl+C).")
    except Exception as e:
        print(f"\n🔴 LỖI LÕI: {e}")
    finally:
        # 4. ĐÓNG KẾT NỐI AN TOÀN
        print("🧹 Đang dọn dẹp hệ thống...")
        traci.close()
        castalia_sock.close()
        print("✅ Hoàn tất!")

if __name__ == "__main__":
    main()
