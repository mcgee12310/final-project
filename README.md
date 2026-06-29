# BPAB VANET Simulation System

## 1. Giới thiệu

Hệ thống mô phỏng được xây dựng nhằm đánh giá giao thức **Binary-Partition-Assisted Broadcast (BPAB)** trong mạng xe tự tổ chức (VANET).

Hệ thống gồm bốn thành phần chính:

* **OMNeT++**: môi trường mô phỏng mạng.
* **Castalia**: framework mô phỏng mạng không dây.
* **SUMO** và **NetEdit**: tạo bản đồ, sinh lưu lượng giao thông và mô phỏng chuyển động phương tiện.
* **Python TraCI Bridge**: đồng bộ dữ liệu vị trí giữa SUMO và Castalia.

---

# 2. Yêu cầu hệ thống

Khuyến nghị sử dụng:

| Thành phần    | Phiên bản                               |
| ------------- | --------------------------------------- |
| OMNeT++       | 4.6                                     |
| Castalia      | 3.3                                     |
| SUMO          | 1.x                                     |
| NetEdit       | đi kèm SUMO                             |
| Python        | 2.7                                     |

> **Lưu ý**
> * Castalia 3.3 được phát triển cho OMNeT++ 4.x.
> * Không nên sử dụng OMNeT++ 6.x.

---

# 3. Cài đặt OMNeT++

### Bước 1

Tải và giải nén OMNeT++ 4.6 tại: https://omnetpp.org/download/old

Ví dụ:

```text
C:\omnetpp-4.6
```

### Bước 2

Mở Command Prompt của OMNeT++.

### Bước 3

Biên dịch môi trường:

```bash
mingwenv.cmd
configure
make
```

### Bước 4

Khởi động IDE:

```bash
omnetpp
```

Nếu thành công sẽ xuất hiện Eclipse IDE.

---

# 4. Cài đặt Castalia

1. Tải Castalia tại: https://omnetpp.org/download-items/Castalia.html
2. Giải nén Castalia.
3. Trong OMNeT++ chọn:

```text
File
 └── Import
      └── Existing Projects into Workspace
```

4. Chọn thư mục Castalia.

5. Build Project.

# 5. Cài đặt SUMO

Tải SUMO từ: https://sumo.dlr.de
Sau khi cài đặt:

* thêm biến môi trường

```text
SUMO_HOME
```

Ví dụ

```text
C:\Program Files (x86)\Eclipse\Sumo
```

Sau đó thêm

```text
%SUMO_HOME%\bin
```

vào biến `PATH`.

Kiểm tra:

```bash
sumo
```

Nếu hiện thông tin phiên bản nghĩa là cài đặt thành công.

---

# 6. Cài đặt NetEdit

NetEdit được cài đặt cùng SUMO.

Mở bằng:

```bash
netedit
```

hoặc

```text
netedit.exe
```

NetEdit được sử dụng để:

* tạo bản đồ
* chỉnh sửa junction
* tạo route
* sinh file `.net.xml`
* sinh file `.sumocfg`

---

# 7. Cài đặt Python

Cài thư viện TraCI:

```bash
pip install traci
```

hoặc sử dụng thư viện TraCI đi kèm SUMO.

Kiểm tra:

```python
import traci
```

Nếu không báo lỗi là thành công.

---

# 8. Chạy mô phỏng

## Bước 1

Mở project bằng OMNeT++.

Build toàn bộ project.

---

## Bước 2

Mở file

```text
omnetpp.ini
```

Chọn cấu hình cần chạy.

Ví dụ:

```ini
[Config Highway]
```

hoặc

```ini
[Config Intersection]
```

---

## Bước 3

Đảm bảo các file SUMO đã tồn tại:

```text
map.net.xml
routes.rou.xml
simulation.sumocfg
```

---

## Bước 4

Nhấn **Run** trong OMNeT++.

`BpabTraCIManager` sẽ tự động:

* mở TCP Server
* khởi động `bridge.py`
* `bridge.py` tự động chạy SUMO

---

# 9. Visualizer

Sau khi mô phỏng chạy, hệ thống sẽ sinh file:

```text
unified_trace.txt
```

Visualizer đọc file này để hiển thị:

* Vị trí xe
* RTB
* CTB
* DATA
* Black Burst
* RSU
* Vùng tranh chấp
* Trạng thái node

---

# 10. Các file quan trọng

| File                  | Chức năng                        |
| --------------------- | -------------------------------- |
| `BpabMac.cc`          | Cài đặt giao thức BPAB           |
| `BpabTraCIManager.cc` | Đồng bộ dữ liệu SUMO và Castalia |
| `bridge.py`           | Kết nối TraCI và TCP             |
| `omnetpp.ini`         | Cấu hình mô phỏng                |
| `map.net.xml`         | Bản đồ giao thông                |
| `*.sumocfg`           | Cấu hình SUMO                    |
| `unified_trace.txt`   | Log mô phỏng phục vụ Visualizer  |

---

# 11. Kết quả

Sau khi chạy thành công, hệ thống sẽ:

* mô phỏng chuyển động phương tiện bằng SUMO;
* đồng bộ vị trí xe sang Castalia theo thời gian thực;
* thực thi giao thức BPAB trên tầng MAC;
* ghi toàn bộ sự kiện vào `unified_trace.txt`;
* trực quan hóa toàn bộ quá trình truyền thông bằng Visualizer.
