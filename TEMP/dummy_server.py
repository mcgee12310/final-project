import socket

def run_dummy_server():
    HOST = '127.0.0.1'
    PORT = 9999

    # Mở cổng 9999 để hứng dữ liệu
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((HOST, PORT))
    server.listen(1)
    
    print(f"🎧 [CASTALIA GIẢ] Đang lắng nghe trên cổng {PORT}...")
    
    conn, addr = server.accept()
    print(f"🟢 Đã có người kết nối từ {addr}!")

    try:
        while True:
            # Nhận dữ liệu và in ra màn hình
            data = conn.recv(1024)
            if not data:
                break
            # Decode và loại bỏ khoảng trắng thừa
            print(f"📥 ĐÃ NHẬN: {data.decode('utf-8').strip()}")
    except KeyboardInterrupt:
        print("\n🛑 Đã tắt Castalia Giả.")
    finally:
        conn.close()
        server.close()

if __name__ == "__main__":
    run_dummy_server()
