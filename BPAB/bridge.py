# -*- coding: utf-8 -*-

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
STEP_LENGTH = 1  # Bước nhảy thời gian của SUMO (1 giây)
PLAYBACK_SPEED = 2.0 # Tốc độ thời gian thực

def main():
    # 1. KẾT NỐI VỚI CASTALIA (Cổng 9999)
    try:
        castalia_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        castalia_sock.connect((CASTALIA_IP, CASTALIA_PORT))
    except ConnectionRefusedError:
        sys.exit(1)

    # 2. KHỞI ĐỘNG SUMO
    # Dùng "sumo" thay vì "sumo-gui" để chạy ngầm hoàn toàn
    sumoCmd = ["sumo", "-c", SUMO_CONFIG_FILE, "--step-length", str(STEP_LENGTH)]
    
    try:
        traci.start(sumoCmd)
    except Exception as e:
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
                # [ĐÃ SỬA] Dùng đúng biến vid 
                road_id = traci.vehicle.getRoadID(vid)
                x_sumo, y_sumo = traci.vehicle.getPosition(vid)
                node_id = int(''.join(filter(str.isdigit, vid)))
                
                # Cờ nhận diện xe đang ở trong giao lộ (SUMO internal lane)
                is_inter = 1 if road_id.startswith(":") else 0
                
                # KHÔNG DÙNG OFFSET: Lấy tọa độ gốc tuyệt đối của SUMO
                cmd = "SET_POS|TIME:{:.2f}|NODE:{}|X:{:.2f}|Y:{:.2f}|INTER:{}\n".format(
                    current_sumo_time, 
                    node_id, 
                    x_sumo,   # (hoặc x_castalia nếu bạn có dùng biến này)
                    y_sumo,   # (hoặc y_castalia nếu bạn có dùng biến này)
                    is_inter
                )

                if is_inter == 1:
                    print("[{:.2f}] PYTHON DEBUG: Node {} đang ở giao lộ (INTER:1)".format(current_sumo_time, node_id))

                castalia_sock.sendall(cmd.encode('utf-8'))
            
            time.sleep(STEP_LENGTH / PLAYBACK_SPEED)

    except KeyboardInterrupt:
        pass
    except Exception as e:
        pass
    finally:
        # 4. ĐÓNG KẾT NỐI AN TOÀN
        traci.close()
        castalia_sock.close()

if __name__ == "__main__":
    main()