# BÁO CÁO BÀI TẬP LỚN: ĐỀ 8 - FLOW SHOP HOÁN VỊ (PFSP)
## LẬP LỊCH DÂY CHUYỀN BẢO DƯỠNG TOA XE

---

### 1. Thông tin nhóm và phân công
- **Đề bài**: Đề 8 - Lập lịch dây chuyền bảo dưỡng: Flow Shop hoán vị (PFSP).
- **Ứng dụng**: Dây chuyền bảo dưỡng định kỳ toa xe đường sắt, sản xuất hàng loạt.
- **Phân công nhiệm vụ**:
  + **Thành viên 1**: Xây dựng mô hình toán học, phân tích độ phức tạp NP-khó, bộ đọc dữ liệu đa định dạng (`core/data_reader.cpp`), hàm đánh giá (`core/evaluator.cpp`), quy tắc Johnson cho $m = 2$ (`exact/johnson.cpp`), thuật toán Nhánh Cận (`exact/branch_bound.cpp`).
  + **Thành viên 2**: Cài đặt Heuristic NEH chuẩn và kỹ thuật tăng tốc Taillard $O(nm)$ (`baseline/neh.cpp`), các cấu trúc lân cận Insertion, Swap, VND (`baseline/local_search.cpp`), Metaheuristic Simulated Annealing (`meta/sa.cpp`).
  + **Thành viên 3**: Cài đặt Metaheuristic Iterated Greedy (`meta/ig.cpp`), Tabu Search (`meta/ts.cpp`), Genetic Algorithm (`meta/ga.cpp`), xây dựng kịch bản thực nghiệm, kiểm thử bằng `checker.exe` và viết báo cáo.

---

### 2. Môi trường phát triển và thực thi
- **Hệ điều hành**: Windows 10 / 11 (hỗ trợ cả Linux/macOS qua g++).
- **Trình biên dịch C++**: MSVC v143 (Visual Studio 2022 Community) hoặc GCC / G++ (hỗ trợ C++17 trở lên).
- **Trình thông dịch**: Python 3.8+ (chỉ sử dụng thư viện chuẩn `os`, `subprocess`, `time`, `math`, không yêu cầu cài thêm gói ngoài).

---

### 3. Hướng dẫn biên dịch và Chạy thuật toán

#### 3.1. Biên dịch mã nguồn
Biên dịch bằng MSVC x64:
```cmd
cl.exe /EHsc /O2 /W3 src\main.cpp src\core\evaluator.cpp src\core\data_reader.cpp src\exact\johnson.cpp src\exact\branch_bound.cpp src\baseline\neh.cpp src\baseline\local_search.cpp src\meta\sa.cpp src\meta\ig.cpp src\meta\ts.cpp src\meta\ga.cpp /Fe:pfsp.exe
```
Hoặc mở file giải pháp `pfsp.sln` trong Visual Studio 2022 và nhấn **Build Solution (Ctrl + Shift + B)**.

#### 3.2. Cú pháp chạy chương trình
```cmd
pfsp.exe <tep_du_lieu> [options]
```

**Các tham số dòng lệnh**:
- `--algo <name>`: Chọn thuật toán (`all`, `johnson`, `bb`, `neh`, `neh_t`, `ls`, `sa`, `ig`, `ts`, `ga`).
- `--time <ms>`: Ngân sách thời gian chạy (mili giây). Mặc định tính theo công thức: $n \cdot m / 2 \times 30$ ms.
- `--seed <int>`: Khởi tạo hạt giống số ngẫu nhiên (mặc định: 42).
- `--inst <idx>`: Chỉ số thể hiện cần giải trong file nhiều thể hiện (mặc định: 1).
- `--save_sol <path>`: Xuất tệp lời giải chuẩn `.sol` (Dòng 1: Cmax, Dòng 2: hoán vị 1-indexed).
- `--save_conv <path>`: Xuất file log hội tụ `.csv` (`time_s,iter,evals,best`).
- `--contract`: In kết quả ra màn hình đúng chuẩn đề bài (Dòng 1: Cmax, Dòng 2: hoán vị).

#### 3.3. Ví dụ cụ thể chạy được ngay
1. **Chạy so sánh toàn bộ thuật toán trên ví dụ nhỏ**:
   ```cmd
   pfsp.exe data\vidu\vidu.txt --algo all
   ```
2. **Chạy quy tắc Johnson cho bài toán 2 máy**:
   ```cmd
   pfsp.exe data\tusinh\rand_6x2.txt --algo johnson
   ```
3. **Chạy Iterated Greedy trên thể hiện Taillard ta001**:
   ```cmd
   pfsp.exe data\chuan\ta001.txt --algo ig --time 1000 --seed 42 --save_sol results\solutions\ta001_IG_s42.sol
   ```

---

### 4. Lệnh chạy chương trình kiểm tra nghiệm (Checker)
Cú pháp công cụ thẩm định nghiệm:
```cmd
checker.exe <tep_du_lieu> <tep_ket_qua>
```
Ví dụ kiểm tra file lời giải tổng hợp của 10 instance Taillard:
```cmd
checker.exe data\chuan\tai20_5.txt results_20_5.txt
```
Hoặc kiểm tra kết quả ví dụ nhỏ:
```cmd
pfsp.exe data\vidu\vidu.txt --algo bb --append results_vidu.txt
checker.exe data\vidu\vidu.txt results_vidu.txt
```

---

### 5. Lệnh chạy toàn bộ thực nghiệm và sinh kết quả
Chỉ cần chạy tập lệnh batch duy nhất:
```cmd
run_all.bat
```
Hoặc chạy script python xuất toàn bộ file hồ sơ (raw CSV, conv log, solutions, biểu đồ):
```cmd
python scripts\export_all_results.py
```
- **Danh sách hạt giống (seed)**: `42`, `123`, `999`.
- **Thời gian chạy ước tính**: ~20-30 giây cho toàn bộ bộ dữ liệu Taillard $20 \times 5$.

---

### 6. Bảng tham số mặc định của các Metaheuristic

| Thuật toán | Tham số chính | Giá trị mặc định | Giải thích ý nghĩa |
| :--- | :--- | :--- | :--- |
| **Simulated Annealing (SA)** | $T_0$ | 0.5 | Hệ số nhiệt độ ban đầu ($T = T_0 \times C_{max}$) |
| | $\alpha$ | 0.995 | Hệ số giảm nhiệt độ hình học |
| | Neighborhood | 80% Insert, 20% Swap | Cấu trúc sinh lân cận hỗn hợp |
| **Iterated Greedy (IG)** | $d$ | 4 | Số lượng công việc bị rút ra ở pha Destruction |
| | $T_{factor}$ | 0.5 | Hệ số nhiệt độ chấp nhận nghiệm xấu theo Ruiz & Stützle (2007) |
| | Local Search | Insertion (Taillard Accel) | Tìm kiếm cục bộ sau pha tái tạo Construction |
| **Tabu Search (TS)** | $tenure$ | $7 + \text{rand}(0, n/5)$ | Thời gian cấm công việc quay lại vị trí cũ |
| | Aspiration | True | Cho phép vượt cấm nếu nghiệm mới tốt hơn kỷ lục toàn cục |
| **Genetic Algorithm (GA)**| $pop\_size$ | 50 | Kích thước quần thể cá thể |
| | Crossover | Order Crossover (OX) | Toán tử lai ghép bảo toàn hoán vị hợp lệ |
| | $p_m$ | 0.25 | Xác suất đột biến chèn (Insertion Mutation) |
| | Elitism | 2 | Giữ lại 2 cá thể ưu tú nhất qua từng thế hệ |
