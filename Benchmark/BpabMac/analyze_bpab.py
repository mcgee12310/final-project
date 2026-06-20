import sys
import re
import math

def analyze_bpab_log(file_path):
    # 1. Khởi tạo các biến lưu trữ trạng thái
    node_positions = {}  # Lưu tọa độ mới nhất của từng node: node_id -> (x, y)
    
    # Biến lưu trữ cho toàn bộ mạng (End-to-End)
    first_send_time = None
    last_receive_time = None
    
    # Biến lưu trữ cho từng Hop
    hops = []
    current_sender = None
    current_hop_start_time = None
    
    # Biến đếm số lượng gói tin & Retries
    stats = {
        'rtb_sent': 0,
        'ctb_sent': 0,
        'data_sent': 0,
        'retries': 0,
        'ack_timeouts': 0
    }

    print(f"Đang phân tích file: {file_path}...\n")
    
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
                
                # CẬP NHẬT TỌA ĐỘ XE TỨC THỜI
                if "EVENT:POS" in event_str:
                    match_x = re.search(r'x:([\d\.-]+)', event_str)
                    match_y = re.search(r'y:([\d\.-]+)', event_str)
                    if match_x and match_y:
                        node_positions[node_id] = (float(match_x.group(1)), float(match_y.group(1)))
                
                # ĐẾM SỐ LƯỢNG GÓI TIN & LỖI
                if "EVENT:SEND_RTB" in event_str:
                    stats['rtb_sent'] += 1
                elif "EVENT:SEND_CTB" in event_str:
                    stats['ctb_sent'] += 1
                elif "EVENT:SEND_DATA" in event_str:
                    stats['data_sent'] += 1
                elif "EVENT:RETRY" in event_str:
                    stats['retries'] += 1
                elif "EVENT:ACK_TIMEOUT" in event_str:
                    stats['ack_timeouts'] += 1

                # THEO DÕI BẮT ĐẦU MỘT HOP MỚI (Từ lúc bắt đầu gửi RTB đầu tiên)
                if ("EVENT:APP_SEND" in event_str) or ("EVENT:SEND_RTB" in event_str and current_hop_start_time is None):
                    if first_send_time is None:
                        first_send_time = time_val # Ghi nhận mốc tg bắt đầu E2E
                        
                    if current_hop_start_time is None:
                        current_hop_start_time = time_val
                        match_from = re.search(r'From:(\d+)', event_str)
                        current_sender = match_from.group(1) if match_from else node_id

                # THEO DÕI KẾT THÚC MỘT HOP (Khi xe tiếp theo nhận thành công DATA và làm Relay)
                if "EVENT:RCV_DATA | Status:SUCCESS_RELAY" in event_str:
                    match_node = re.search(r'Node:(\d+)', event_str)
                    if match_node:
                        receiver = match_node.group(1)
                        
                        # Tính toán thông số cho Hop này
                        if current_sender and current_sender in node_positions and receiver in node_positions:
                            sx, sy = node_positions[current_sender]
                            rx, ry = node_positions[receiver]
                            
                            distance = math.sqrt((sx - rx)**2 + (sy - ry)**2)
                            hop_delay = time_val - current_hop_start_time
                            speed = distance / hop_delay if hop_delay > 0 else 0
                            
                            hops.append({
                                'sender': current_sender,
                                'receiver': receiver,
                                'distance': distance,
                                'delay': hop_delay,
                                'speed': speed
                            })
                            
                        # Chuyển quyền Relay cho trạm tiếp theo
                        current_sender = receiver
                        current_hop_start_time = time_val
                        last_receive_time = time_val

                # Ghi nhận thời điểm nhận E2E cuối cùng tại tầng App
                if "EVENT:APP_RCV" in event_str:
                    last_receive_time = time_val

    except FileNotFoundError:
        print(f"Lỗi: Không tìm thấy file {file_path}")
        return

    # 3. TỔNG HỢP VÀ IN BÁO CÁO
    print("-" * 75)
    print(f"{'CHI TIẾT TỪNG BƯỚC NHẢY (HOP)':^75}")
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
    print(f"{'BÁO CÁO TỔNG QUAN HIỆU NĂNG BPAB':^75}")
    print("=" * 75)
    
    print("\n1. ĐỘ TRỄ VÀ QUÃNG ĐƯỜNG:")
    print(f"   - Tổng số Hops đã đi      : {num_hops} hops")
    print(f"   - End-to-End Delay        : {e2e_delay:.4f} giây")
    print(f"   - Độ trễ trung bình/Hop   : {avg_hop_delay:.4f} giây/hop")
    print(f"   - Message Progress/Hop    : {avg_distance:.2f} mét/hop")
    if avg_hop_delay > 0:
        print(f"   - Tốc độ lan truyền TB    : {(avg_distance / avg_hop_delay):.2f} mét/giây")
    
    print("\n2. THỐNG KÊ GÓI TIN & ĐỘ TIN CẬY:")
    print(f"   - Tổng số RTB đã gửi      : {stats['rtb_sent']}")
    print(f"   - Tổng số CTB đã gửi      : {stats['ctb_sent']}")
    print(f"   - Tổng số DATA đã gửi     : {stats['data_sent']}")
    print(f"   - Số lần phát lại (RETRY) : {stats['retries']}")
    print(f"   - Số lần rớt ACK ngầm     : {stats['ack_timeouts']}")
    print("=" * 75 + "\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Sử dụng: python analyze_bpab.py <ten_file_log.txt>")
    else:
        analyze_bpab_log(sys.argv[1])