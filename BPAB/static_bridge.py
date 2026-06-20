# -*- coding: utf-8 -*-

import socket
import time
import sys

# ==========================================
# CAU HINH HE THONG
# ==========================================
CASTALIA_IP = "127.0.0.1"
CASTALIA_PORT = 9999
NUM_NODES = 500

X_START = 100.0
X_END = 10100.0

def main():
    # 1. KET NOI VOI CASTALIA
    try:
        castalia_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        castalia_sock.connect((CASTALIA_IP, CASTALIA_PORT))
        print("=== Da ket noi voi Castalia tai %s:%d ===" % (CASTALIA_IP, CASTALIA_PORT))
    except socket.error:
        print("Khong ket noi duoc Castalia! Vui long dam bao OMNeT++ da chay truoc.")
        sys.exit(1)

    # 2. GUI TOA DO TINH LUU TRU TRONG MANG
    try:
        print("Bat dau gui toa do khoi tao cho %d node..." % NUM_NODES)
        
        if NUM_NODES > 1:
            x_step = (X_END - X_START) / (NUM_NODES - 1)
        else:
            x_step = 0

        for index in range(NUM_NODES):
            # X: Node sẽ được rải đều chính xác từ 100.0 đến 10100.0
            x_pos = X_START + (index * x_step)
            
            # Y: Xếp luân phiên vào 4 làn đường (100, 110, 120, 130)
            y_pos = 100.0 + (index % 6) * 10.0
            
            is_inter = 0 # Duong cao toc thang, khong co giao lo
            sim_time = 3.0 # Thoi gian de OMNeT++ set toa do
            
            # Dinh dang lenh gui sang BpabTraCIManager
            cmd = "SET_POS|TIME:%.2f|NODE:%d|X:%.2f|Y:%.2f|INTER:%d\n" % (
                sim_time, index, x_pos, y_pos, is_inter
            )
            
            castalia_sock.sendall(cmd.encode('utf-8'))
            
        print("Da gui hoan tat %d toa do! Dang giu ket noi TCP..." % NUM_NODES)
        
        # 3. NGU DONG (IDLE) KEEPALIVE
        # Giu script chay vo han de socket khong bi huy, 
        # giup BpabTraCIManager khong in ra loi "Bo dieu khien da ngat ket noi".
        while True:
            time.sleep(60)
            
    except KeyboardInterrupt:
        print("\nNhan duoc lenh ngung script tu nguoi dung.")
        
    finally:
        try:
            castalia_sock.close()
            print("Da dong ket noi socket an toan.")
        except Exception:
            pass

if __name__ == "__main__":
    main()