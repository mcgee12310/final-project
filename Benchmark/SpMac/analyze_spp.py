# -*- coding: utf-8 -*-
import sys
import re
import math

def analyze_spp_log(file_path):
    # 1. Khởi tạo các biến lưu trữ trạng thái
    node_positions = {}  # Lưu tọa độ: node_id -> (x, y)
    
    # Biến lưu trữ cho toàn bộ mạng (End-to-End)
    first_send_time = None
    last_receive_time = None
    source_node = None
    
    # Theo dõi node xa nhất để đo E2E Delay chuẩn xác
    max_x_rcv = -1.0
    furthest_node = None
    
    # Biến lưu trữ cho từng Hop
    hops = []
    current_sender = None
    current_hop_start_time = None
    
    # Biến thống kê hiệu năng giao thức SpP
    stats = {
        'total_forwards': 0,     # Số lần phát sóng thực tế
        'cancels_overheard': 0,  # Số lần hủy phát do nghe lỏm
        'drops_prob': 0,         # Số lần hủy do quay trượt xác suất
        'duplicates': 0          # Số lượng gói tin lặp bị chặn
    }

    print(f"Đang phân tích file log Slotted p-Persistence: {file_path}...\n")
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                parts = line.strip().split('\t')
                if len(parts) < 4:
                    continue
                
                try:
                    time_val = float(parts[0])
                except ValueError:
                    continue
                    
                node_id = parts[1]
                event_str = parts[3]
                
                # A. CẬP NHẬT TỌA ĐỘ XE TỨC THỜI
                if "EVENT:POS" in event_str:
                    match_x = re.search(r'x:([\d\.-]+)', event_str)
                    match_y = re.search(r'y:([\d\.-]+)', event_str)
                    if match_x and match_y:
                        node_positions[node_id] = (float(match_x.group(1)), float(match_y.group(1)))
                
                # B. THEO DÕI NGUỒN PHÁT ĐẦU TIÊN
                if "EVENT:APP_SEND" in event_str and first_send_time is None:
                    first_send_time = time_val
                    source_node = node_id
                    current_sender = node_id
                    current_hop_start_time = time_val
                    stats['total_forwards'] += 1 # Tính cả lượt phát của Nguồn
                    
                    if node_id in node_positions:
                        max_x_rcv = node_positions[node_id][0]
                
                # C. THEO DÕI ĐIỂM KẾT THÚC THEO TỌA ĐỘ (CHÍNH XÁC HƠN)
                if "EVENT:APP_RCV" in event_str:
                    if node_id in node_positions:
                        nx = node_positions[node_id][0]
                        # Chỉ cập nhật E2E Delay nếu node này xa hơn kỷ lục hiện tại
                        if nx > max_x_rcv:
                            max_x_rcv = nx
                            last_receive_time = time_val
                            furthest_node = node_id
                
                # D. ĐẾM THỐNG KÊ GIAO THỨC SpP
                if "SP_CANCEL" in event_str and "OVERHEARD" in event_str:
                    stats['cancels_overheard'] += 1
                elif "DROP_DUPLICATE" in event_str:
                    stats['duplicates'] += 1
                elif "SP_DECISION" in event_str and "Action:DROP_PROBABILITY" in event_str:
                    stats['drops_prob'] += 1
                    
                # E. MÔ PHỎNG CHUỖI HOP HỢP LỆ
                if "SP_DECISION" in event_str and "Action:FORWARD" in event_str:
                    stats['total_forwards'] += 1
                    forwarder_node = node_id
                    
                    if current_sender and current_sender in node_positions and forwarder_node in node_positions:
                        sx, sy = node_positions[current_sender]
                        rx, ry = node_positions[forwarder_node]
                        
                        distance = math.sqrt((sx - rx)**2 + (sy - ry)**2)
                        
                        # [NÂNG CẤP LỌC NHIỄU QUAN TRỌNG]
                        # 1. Khoảng cách phải <= 450m (Bỏ qua các ảo ảnh dội sóng > 1000m)
                        # 2. Phải tiến lên phía trước (rx > sx) (Bỏ qua dội sóng ngược lại phía sau)
                        if distance <= 450.0 and rx > sx:
                            hop_delay = time_val - current_hop_start_time
                            speed = distance / hop_delay if hop_delay > 0 else 0
                            
                            hops.append({
                                'sender': current_sender,
                                'receiver': forwarder_node,
                                'distance': distance,
                                'delay': hop_delay,
                                'speed': speed
                            })
                            
                            # Cập nhật quyền Relay cho xe vừa FORWARD hợp lệ
                            current_sender = forwarder_node
                            current_hop_start_time = time_val

    except FileNotFoundError:
        print(f"Lỗi: Không tìm thấy file {file_path}")
        return

    # 3. TỔNG HỢP VÀ IN BÁO CÁO
    print("-" * 75)
    print(f"{'CHI TIẾT TỪNG BƯỚC NHẢY (HOP) TRONG SpP':^75}")
    print("-" * 75)
    print(f"{'Hop':<10} | {'Từ -> Đến':<12} | {'Khoảng cách (m)':<15} | {'Delay (s)':<12} | {'Tốc độ (m/s)':<15}")
    print("-" * 75)
    
    total_distance = 0
    total_hop_delay = 0
    
    for i, hop in enumerate(hops):
        print(f"Hop {i+1:<5} | Node {hop['sender']:<2}-> {hop['receiver']:<3} | {hop['distance']:<15.2f} | {hop['delay']:<12.4f} | {hop['speed']:<15.2f}")
        total_distance += hop['distance']
        total_hop_delay += hop['delay']

    # Tính toán giá trị trung bình
    num_hops = len(hops)
    avg_hop_delay = (total_hop_delay / num_hops) if num_hops > 0 else 0
    avg_distance = (total_distance / num_hops) if num_hops > 0 else 0
    e2e_delay = (last_receive_time - first_send_time) if (last_receive_time and first_send_time) else 0

    print("\n" + "=" * 75)
    print(f"{'BÁO CÁO TỔNG QUAN HIỆU NĂNG SLOTTED p-PERSISTENCE':^75}")
    print("=" * 75)
    
    print("\n1. ĐỘ TRỄ VÀ QUÃNG ĐƯỜNG (METRICS):")
    print(f"   - Tổng số Hops lan truyền : {num_hops} hops")
    print(f"   - Xe xa nhất nhận được    : Node {furthest_node} (X = {max_x_rcv:.2f}m)")
    print(f"   - End-to-End Delay (Thực) : {e2e_delay:.4f} giây")
    print(f"   - Tổng quãng đường (Thực) : {total_distance:.2f} mét")
    print(f"   - Độ trễ trung bình/Hop   : {avg_hop_delay:.4f} giây/hop")
    print(f"   - Message Progress/Hop    : {avg_distance:.2f} mét/hop")
    
    avg_speed = (avg_distance / avg_hop_delay) if avg_hop_delay > 0 else 0
    if avg_hop_delay > 0:
        print(f"   - Tốc độ lan truyền TB    : {avg_speed:.2f} mét/giây")

    # =================================================================
    # TÍNH NĂNG NHÂN TỈ LỆ (EXTRAPOLATION) CHO TOÀN TUYẾN ĐƯỜNG
    # =================================================================
    TOTAL_HIGHWAY_LENGTH = 10000.0 
    
    print("\n2. DỰ ĐOÁN CHO TOÀN TUYẾN ĐƯỜNG 10KM (EXTRAPOLATION):")
    if total_distance > 0 and avg_speed > 0:
        projected_e2e_delay = TOTAL_HIGHWAY_LENGTH / avg_speed
        projected_hops = int(TOTAL_HIGHWAY_LENGTH / avg_distance)
        
        print(f"   - Chiều dài đường giả định: {TOTAL_HIGHWAY_LENGTH} mét")
        print(f"   - Số Hops dự kiến         : ~ {projected_hops} hops")
        print(f"   - E2E Delay dự kiến       : ~ {projected_e2e_delay:.4f} giây")
    else:
        print("   - Không đủ dữ liệu để dự đoán (Chưa có bước nhảy hợp lệ).")
    
    print("\n3. PHÂN TÍCH TÀI NGUYÊN & OVERHEAD:")
    print(f"   - Tổng số lần phát sóng   : {stats['total_forwards']} lần")
    print(f"   - Số lần Hủy do NGHE LỎM  : {stats['cancels_overheard']} lần")
    print(f"   - Số lần rớt do Xác Suất  : {stats['drops_prob']} lần")
    print(f"   - Gói lặp bị loại bỏ      : {stats['duplicates']} lần")
    
    if total_distance > 0:
        ratio = TOTAL_HIGHWAY_LENGTH / total_distance
        projected_forwards = int(stats['total_forwards'] * ratio)
        print(f"   -> Dự kiến số lần phát sóng cho toàn đường: ~ {projected_forwards} lần")

    print("=" * 75 + "\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Sử dụng: python analyze_spp.py <ten_file_log.txt>")
    else:
        analyze_spp_log(sys.argv[1])