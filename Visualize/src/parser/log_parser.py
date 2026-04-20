import re
import pandas as pd
import os

class CastaliaParser:
    def __init__(self, file_path):
        self.file_path = file_path
        # Regex chuẩn để tách: Timestamp | Module_Path | Event_Type | Details
        # Ví dụ: 1.152487988701 SN.node[0].Communication.MAC EVENT:SEND | Type:RTB | From:0 | To:BROADCAST
        self.log_pattern = re.compile(r"^(\d+\.\d+)\s+SN\.node\[(\d+)\]\.[^\s]+\s+EVENT:([^|]+)\|(.*)$")

    def parse(self):
        """
        Đọc file log và chuyển đổi thành Pandas DataFrame để dễ dàng xử lý trong Dash/Plotly.
        """
        if not os.path.exists(self.file_path):
            print(f"Lỗi: Không tìm thấy file tại {self.file_path}")
            return pd.DataFrame()

        extracted_data = []

        with open(self.file_path, 'r') as f:
            for line in f:
                line = line.strip()
                if "EVENT:" not in line:
                    continue
                
                match = self.log_pattern.search(line)
                if match:
                    timestamp = float(match.group(1))
                    node_id = int(match.group(2))
                    event_type = match.group(3).strip()
                    details_raw = match.group(4).strip()
                    
                    # Khởi tạo dict chứa dữ liệu cơ bản
                    event_entry = {
                        "time": timestamp,
                        "node": node_id,
                        "event": event_type
                    }
                    
                    # Bóc tách các cặp Key:Value tùy biến sau dấu |
                    # Ví dụ: Type:RTB | From:0 | To:BROADCAST
                    detail_items = details_raw.split("|")
                    for item in detail_items:
                        if ":" in item:
                            key, value = item.split(":", 1)
                            key = key.strip().lower() # Chuyển key về chữ thường để dễ truy vấn
                            value = value.strip()
                            
                            # Thử chuyển giá trị sang số nếu có thể
                            try:
                                if "." in value:
                                    value = float(value)
                                else:
                                    value = int(value)
                            except ValueError:
                                pass # Giữ nguyên là string nếu không phải số
                            
                            event_entry[key] = value
                    
                    extracted_data.append(event_entry)

        # Chuyển danh sách thành DataFrame
        df = pd.DataFrame(extracted_data)
        
        if not df.empty:
            # Đảm bảo dữ liệu được sắp xếp theo thời gian
            df = df.sort_values(by="time").reset_index(drop=True)
            
        return df

    def get_node_positions(self, df):
        """
        Trích xuất tọa độ cố định của các node từ sự kiện EVENT:POS
        """
        pos_df = df[df['event'] == 'POS'].copy()
        if pos_df.empty:
            return {}
        
        # Lấy giá trị POS cuối cùng của mỗi node (phòng trường hợp node di chuyển)
        latest_pos = pos_df.groupby('node').last()
        positions = {}
        for node_id, row in latest_pos.iterrows():
            positions[int(node_id)] = {
                "x": row.get('x', 0),
                "y": row.get('y', 0)
            }
        return positions

# --- Đoạn mã dùng để Test độc lập ---
if __name__ == "__main__":
    # Giả sử file log nằm trong thư mục data/raw/
    path = "data/raw/Castalia-Trace.txt"
    parser = CastaliaParser(path)
    result_df = parser.parse()
    
    print("--- 5 dòng dữ liệu đầu tiên ---")
    print(result_df.head())
    
    print("\n--- Danh sách tọa độ Node ---")
    print(parser.get_node_positions(result_df))
    
    print("\n--- Thống kê các loại sự kiện ---")
    print(result_df['event'].value_counts())
