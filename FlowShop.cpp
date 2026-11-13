/* =====================================================================
   ĐỒ ÁN: HỆ THỐNG DÂY CHUYỀN BẢO DƯỠNG XE
   Môn  : Phân tích thiết kế và giải thuật
   Ngôn ngữ: C++17  —  Biên dịch: g++ -std=c++17 -O2 -o xuong xuong.cpp
   ---------------------------------------------------------------------
   NGHIỆP VỤ (thứ người dùng thấy):
     Tiếp nhận xe → Hàng chờ → Lập lịch & chạy dây chuyền → Trạng thái
     trạm → Xe hoàn thành + phiếu → Báo cáo ca → Xuất file
   LÕI ẨN (không hiện tên thuật toán ra màn hình):
     Bộ xếp lịch Flow shop hoán vị (Fm|prmu|Cmax) — chọn thứ tự xe vào
     dây chuyền sao cho cả lô xong sớm nhất, so sánh với thứ tự đến (FIFO).
   ===================================================================== */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <climits>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

/* =====================================================================
   1. HẰNG SỐ NGHIỆP VỤ
   ===================================================================== */
static const int M = 5;                 // 5 trạm
static const int SHIFT_START = 8 * 60;  // ca bắt đầu 08:00 (phút)

static const vector<string> ST_NAME = {
    "Tiếp nhận", "Rửa & vệ sinh", "Chẩn đoán", "Bảo dưỡng", "Kiểm tra & bàn giao"
};
static const vector<string> ST_CODE = { "T1", "T2", "T3", "T4", "T5" };

static const vector<string> VTYPE = { "Sedan/Hatchback", "SUV/Bán tải", "Xe 16 chỗ/tải nhẹ" };
static const vector<string> PKG = { "Nhanh", "Định kỳ", "Chuyên sâu", "Khắc phục lỗi" };

// Thời gian chuẩn (phút) của từng gói tại từng trạm. G1 bỏ qua chẩn đoán → T3 = 0
static const int BASE[4][M] = {
    { 8, 12,  0, 25,  6},   // G1 Nhanh
    {10, 15, 15, 40, 10},   // G2 Định kỳ
    {12, 18, 25, 70, 15},   // G3 Chuyên sâu
    {12, 15, 30, 90, 20}    // G4 Khắc phục lỗi
};
static const double KTYPE[3] = { 1.00, 1.20, 1.40 };

static double kmFactor(int odo) {
    if (odo < 20000)  return 1.00;
    if (odo < 50000)  return 1.05;
    if (odo <= 100000) return 1.15;
    return 1.25;
}

/* =====================================================================
   2. TIỆN ÍCH: giờ giấc, căn lề tiếng Việt (UTF-8), nhập liệu
   ===================================================================== */
static size_t uwidth(const string& s) {           // đếm ký tự hiển thị UTF-8
    size_t w = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++w;
    return w;
}
static string padR(const string& s, size_t w) {
    size_t l = uwidth(s); return s + string(w > l ? w - l : 0, ' ');
}
static string padL(const string& s, size_t w) {
    size_t l = uwidth(s); return string(w > l ? w - l : 0, ' ') + s;
}
static string hhmm(int minute) {                  // phút tương đối → giờ thực
    int t = SHIFT_START + minute;
    ostringstream os;
    os << setw(2) << setfill('0') << (t / 60) % 24 << ":" << setw(2) << setfill('0') << t % 60;
    return os.str();
}
static string trim(string s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
static string ask(const string& prompt) {
    cout << prompt;
    string s;
    if (!getline(cin, s)) { cout << "\n(Kết thúc nhập liệu)\n"; exit(0); }
    return trim(s);
}
static int askInt(const string& prompt, int lo, int hi, int def = INT_MIN) {
    while (true) {
        string s = ask(prompt);
        if (s.empty() && def != INT_MIN && def >= lo && def <= hi) return def;
        try {
            size_t pos = 0; int v = stoi(s, &pos);
            if (pos == s.size() && v >= lo && v <= hi) return v;
        }
        catch (...) {}
        cout << "  ! Nhập một số trong [" << lo << ", " << hi << "]"
            << (def != INT_MIN ? " (Enter = " + to_string(def) + ")" : "") << "\n";
    }
}
static string bar(double pct, int width = 10) {
    int f = (int)llround(pct / 100.0 * width);
    f = max(0, min(width, f));
    string s; for (int i = 0; i < f; ++i) s += "█";
    for (int i = f; i < width; ++i) s += "░";
    return s;
}

/* =====================================================================
   3. CẤU TRÚC DỮ LIỆU
   ===================================================================== */
struct Vehicle {
    int id = 0;
    string plate, owner;
    int type = 0, pkg = 0, odo = 0;
    vector<int> p;      // p[0..4]: thời gian xử lý tại 5 trạm (tự sinh)
    int arrival = 0;    // phút vào xưởng (kể từ 08:00)
    int total() const { int s = 0; for (int x : p) s += x; return s; }
};

struct Task { int veh, st, s, f; };   // veh = chỉ số xe trong lô; s,f = phút tuyệt đối

struct Schedule {
    vector<int> order;      // thứ tự phục vụ (hoán vị – giữ nguyên ở mọi trạm)
    vector<Task> tasks;     // tasks[i*M + k]: xe ở vị trí i, trạm k
    int start = 0, make = 0, sumFlow = 0, sumWait = 0;
};

struct Done {                       // xe đã hoàn thành (để in phiếu / báo cáo)
    Vehicle v;
    vector<pair<int, int>> st;       // 5 cặp (vào trạm, ra trạm)
    int outTime = 0;
};

struct Workshop {
    vector<Vehicle> queue;          // hàng chờ lô hiện tại
    vector<Done> done;              // xe đã xong
    vector<int> busy;               // tổng phút bận mỗi trạm
    int now = 0;                    // đồng hồ xưởng (phút từ 08:00)
    int nextId = 1;
    unsigned seed = 2026;
    bool jitter = true;             // dao động ±10%
    mt19937 rng;
    // lô vừa chạy (giữ lại để xem trạng thái / Gantt / xuất CSV)
    bool hasBatch = false;
    vector<Vehicle> batch;
    Schedule sch;
    long long sumFifo = 0, sumChosen = 0;
    Workshop() : busy(M, 0), rng(2026) {}
};

/* =====================================================================
   4. TIẾP NHẬN XE: sinh thời gian xử lý tại 5 trạm
   ===================================================================== */
static vector<int> genTimes(Workshop& W, int pkg, int type, int odo) {
    vector<int> p(M);
    uniform_real_distribution<double> U(-0.10, 0.10);
    for (int k = 0; k < M; ++k) {
        if (BASE[pkg][k] == 0) { p[k] = 0; continue; }   // trạm bỏ qua
        double v = BASE[pkg][k] * KTYPE[type] * kmFactor(odo);
        if (W.jitter) v *= (1.0 + U(W.rng));
        p[k] = max(1, (int)llround(v));
    }
    return p;
}

static void addVehicle(Workshop& W, const string& plate, const string& owner,
    int type, int pkg, int odo, bool verbose = true) {
    Vehicle v;
    v.id = W.nextId++; v.plate = plate; v.owner = owner;
    v.type = type; v.pkg = pkg; v.odo = odo;
    v.arrival = W.now;
    v.p = genTimes(W, pkg, type, odo);
    W.queue.push_back(v);
    if (verbose) {
        cout << "\n  ✔ Đã tiếp nhận " << v.plate << " (" << VTYPE[type] << ", gói " << PKG[pkg] << ")\n";
        cout << "    Thời gian dự kiến: ";
        for (int k = 0; k < M; ++k) cout << ST_CODE[k] << "=" << v.p[k] << "p  ";
        cout << "\n    Tổng thời gian xử lý: " << v.total() << " phút\n";
        cout << "    Vị trí trong hàng chờ: " << W.queue.size() << "\n";
    }
}

static void receiveVehicle(Workshop& W) {
    cout << "\n────────── TIẾP NHẬN XE VÀO XƯỞNG ──────────\n";
    string plate = ask("Biển số xe: ");
    if (plate.empty()) { cout << "  ! Biển số trống, hủy tiếp nhận.\n"; return; }
    for (const auto& v : W.queue)
        if (v.plate == plate) { cout << "  ! Xe này đã có trong hàng chờ.\n"; return; }
    string owner = ask("Tên chủ xe: ");
    if (owner.empty()) owner = "(không rõ)";
    cout << "Loại xe:  [1] " << VTYPE[0] << "  [2] " << VTYPE[1] << "  [3] " << VTYPE[2] << "\n";
    int type = askInt("Chọn loại xe (1-3): ", 1, 3) - 1;
    cout << "Gói bảo dưỡng:\n";
    for (int g = 0; g < 4; ++g) {
        int s = 0; for (int k = 0; k < M; ++k) s += BASE[g][k];
        cout << "  [" << g + 1 << "] " << padR(PKG[g], 16) << " (chuẩn ~" << s << " phút)\n";
    }
    int pkg = askInt("Chọn gói (1-4): ", 1, 4) - 1;
    int odo = askInt("Số km đã chạy: ", 0, 2000000);
    addVehicle(W, plate, owner, type, pkg, odo);
}

/* =====================================================================
   5. BỘ XẾP LỊCH NỘI BỘ  (Flow shop hoán vị – ẩn với người dùng)
      C[i][k] = max(C[i-1][k], C[i][k-1]) + p[xe_i][k]
   ===================================================================== */
static Schedule buildSchedule(const vector<Vehicle>& v, const vector<int>& ord, int t0) {
    Schedule sc; sc.order = ord; sc.start = t0;
    int n = (int)ord.size();
    sc.tasks.resize(n * M);
    vector<vector<int>> C(n, vector<int>(M, 0));
    for (int i = 0; i < n; ++i) {
        const Vehicle& x = v[ord[i]];
        for (int k = 0; k < M; ++k) {
            int a = (i > 0 ? C[i - 1][k] : t0);   // trạm k còn bận xe trước
            int b = (k > 0 ? C[i][k - 1] : t0);   // xe chưa xong trạm trước
            int s = max(a, b);
            C[i][k] = s + x.p[k];
            sc.tasks[i * M + k] = Task{ ord[i], k, s, C[i][k] };
        }
        sc.sumFlow += C[i][M - 1] - x.arrival;
        sc.sumWait += (C[i][M - 1] - x.arrival) - x.total();
    }
    sc.make = n ? C[n - 1][M - 1] : t0;
    return sc;
}
static int makespanOf(const vector<Vehicle>& v, const vector<int>& ord) {
    int t[M] = { 0 };
    for (int j : ord) {
        int prev = 0;
        for (int k = 0; k < M; ++k) { int c = max(t[k], prev) + v[j].p[k]; t[k] = c; prev = c; }
    }
    return t[M - 1];
}
// Xếp lịch thông minh: xe nặng vào trước, rồi chèn từng xe vào vị trí tốt nhất
static vector<int> smartOrder(const vector<Vehicle>& v) {
    int n = (int)v.size();
    vector<int> ord(n); iota(ord.begin(), ord.end(), 0);
    sort(ord.begin(), ord.end(), [&](int a, int b) {
        if (v[a].total() != v[b].total()) return v[a].total() > v[b].total();
        return v[a].plate < v[b].plate;
        });
    vector<int> seq; seq.reserve(n);
    for (int j : ord) {
        int k = (int)seq.size(), best = INT_MAX, bp = 0;
        for (int pos = 0; pos <= k; ++pos) {
            seq.insert(seq.begin() + pos, j);
            int val = makespanOf(v, seq);
            seq.erase(seq.begin() + pos);
            if (val < best) { best = val; bp = pos; }
        }
        seq.insert(seq.begin() + bp, j);
    }
    return seq;
}

/* =====================================================================
   6. MÔ PHỎNG DÂY CHUYỀN: in log theo sự kiện
   ===================================================================== */
struct Event { int t, prio, idx; };   // prio: 0 = ra trạm (in trước), 1 = vào trạm

static void runSimulation(Workshop& W, const Schedule& sc, bool stepMode) {
    vector<Event> ev;
    for (int i = 0; i < (int)sc.tasks.size(); ++i) {
        ev.push_back({ sc.tasks[i].s, 1, i });
        ev.push_back({ sc.tasks[i].f, 0, i });
    }
    sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) {
        if (a.t != b.t) return a.t < b.t;
        return a.prio < b.prio;
        });

    cout << "\n────────── DÂY CHUYỀN BẮT ĐẦU CHẠY ──────────\n";
    bool silent = false;
    for (const auto& e : ev) {
        const Task& t = sc.tasks[e.idx];
        const Vehicle& x = W.batch[t.veh];
        if (!silent) {
            if (e.prio == 1) {
                if (x.p[t.st] == 0) continue;   // trạm bỏ qua, không in
                cout << "[" << hhmm(e.t) << "] " << padR(x.plate, 12)
                    << " → " << ST_CODE[t.st] << " " << ST_NAME[t.st]
                    << " (" << x.p[t.st] << "p)\n";
            }
            else {
                if (t.st == M - 1) {
                    int flow = e.t - x.arrival, wait = flow - x.total();
                    cout << "[" << hhmm(e.t) << "] " << padR(x.plate, 12)
                        << " ✔ HOÀN THÀNH  (lưu xưởng " << flow << "p, chờ " << wait << "p)\n";
                }
                else if (t.st + 1 < M && x.p[t.st] > 0) {
                    int gap = sc.tasks[e.idx + 1].s - e.t;
                    if (gap > 0 && x.p[t.st + 1] > 0)
                        cout << "[" << hhmm(e.t) << "] " << padR(x.plate, 12)
                        << " … chờ vào " << ST_CODE[t.st + 1] << " (" << gap << "p trạm bận)\n";
                }
            }
        }
        if (stepMode && !silent) {
            string c = ask("   (Enter = tiếp | a = chạy hết | q = ẩn log còn lại): ");
            if (c == "a") stepMode = false;
            else if (c == "q") { silent = true; stepMode = false; }
        }
    }

    // Kết thúc lô: chốt sổ
    for (int i = 0; i < (int)sc.order.size(); ++i) {
        Done d; d.v = W.batch[sc.order[i]];
        for (int k = 0; k < M; ++k) {
            const Task& t = sc.tasks[i * M + k];
            d.st.push_back({ t.s, t.f });
            W.busy[k] += d.v.p[k];
        }
        d.outTime = sc.tasks[i * M + M - 1].f;
        W.done.push_back(d);
    }
    W.now = sc.make;
    cout << "\n[" << hhmm(W.now) << "] === DÂY CHUYỀN TRỐNG — LÔ XONG ===\n";
}

/* =====================================================================
   7. SƠ ĐỒ GANTT + TRẠNG THÁI TRẠM
   ===================================================================== */
static void printGantt(ostream& os, const Workshop& W, int width = 84) {
    if (!W.hasBatch || W.sch.tasks.empty()) { os << "  (chưa có lô nào chạy)\n"; return; }
    const Schedule& sc = W.sch;
    int span = sc.make - sc.start;
    if (span <= 0) return;
    const string sym = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    double scale = (double)width / span;
    os << "\n── SƠ ĐỒ DÂY CHUYỀN (mỗi ký tự ≈ " << fixed << setprecision(1)
        << (double)span / width << " phút, '.' = trạm nghỉ) ──\n";
    for (int k = 0; k < M; ++k) {
        string line(width, '.');
        for (int i = 0; i < (int)sc.order.size(); ++i) {
            const Task& t = sc.tasks[i * M + k];
            if (t.f <= t.s) continue;
            int a = (int)floor((t.s - sc.start) * scale);
            int b = (int)floor((t.f - sc.start) * scale);
            a = max(0, min(a, width - 1)); b = max(a, min(b, width));
            char c = sym[i % sym.size()];
            if (b <= a) line[a] = c; else for (int q = a; q < b; ++q) line[q] = c;
        }
        os << "  " << padR(ST_CODE[k] + " " + ST_NAME[k], 24) << "|" << line << "|\n";
    }
    os << "  " << padR("", 24) << " " << hhmm(sc.start)
        << string(width - 12, ' ') << hhmm(sc.make) << "\n  Chú thích: ";
    for (int i = 0; i < (int)sc.order.size(); ++i)
        os << sym[i % sym.size()] << "=" << W.batch[sc.order[i]].plate << "  ";
    os << "\n";
}

static void showStatus(Workshop& W) {
    if (!W.hasBatch) { cout << "  Chưa có lô nào chạy. Tiếp nhận xe rồi chọn mục 3.\n"; return; }
    cout << "\n── TRẠNG THÁI DÂY CHUYỀN ──\n";
    cout << "Lô chạy từ " << hhmm(W.sch.start) << " đến " << hhmm(W.sch.make) << "\n";
    int t = askInt("Xem tại thời điểm (phút kể từ 08:00, " + to_string(W.sch.start)
        + ".." + to_string(W.sch.make) + ", Enter = đầu lô): ",
        W.sch.start, W.sch.make, W.sch.start);
    cout << "\nThời điểm: " << hhmm(t) << "\n";
    cout << "  " << padR("Trạm", 26) << padR("Trạng thái", 12) << padR("Xe đang làm", 14) << "Còn lại\n";
    cout << "  " << string(60, '-') << "\n";
    for (int k = 0; k < M; ++k) {
        int found = -1;
        for (int i = 0; i < (int)W.sch.order.size(); ++i) {
            const Task& tk = W.sch.tasks[i * M + k];
            if (tk.s <= t && t < tk.f) { found = i; break; }
        }
        cout << "  " << padR(ST_CODE[k] + " " + ST_NAME[k], 26);
        if (found < 0) cout << padR("RẢNH", 12) << padR("-", 14) << "-\n";
        else {
            const Task& tk = W.sch.tasks[found * M + k];
            cout << padR("BẬN", 12) << padR(W.batch[tk.veh].plate, 14) << (tk.f - t) << "p\n";
        }
    }
    cout << "\n  Tình trạng từng xe:\n";
    for (int i = 0; i < (int)W.sch.order.size(); ++i) {
        const Vehicle& x = W.batch[W.sch.order[i]];
        cout << "    " << padR(x.plate, 14);
        if (t < W.sch.tasks[i * M].s) { cout << "chờ vào dây chuyền\n"; continue; }
        if (t >= W.sch.tasks[i * M + M - 1].f) { cout << "✔ đã hoàn thành\n"; continue; }
        bool ok = false;
        for (int k = 0; k < M; ++k) {
            const Task& tk = W.sch.tasks[i * M + k];
            if (tk.s <= t && t < tk.f) { cout << "đang ở " << ST_CODE[k] << " " << ST_NAME[k] << "\n"; ok = true; break; }
            if (k + 1 < M && tk.f <= t && t < W.sch.tasks[i * M + k + 1].s && x.p[k + 1] > 0) {
                cout << "chờ vào " << ST_CODE[k + 1] << " (trạm bận)\n"; ok = true; break;
            }
        }
        if (!ok) cout << "đang di chuyển\n";
    }
}

/* =====================================================================
   8. PHIẾU BẢO DƯỠNG + BÁO CÁO CA
   ===================================================================== */
static void printTicket(ostream& os, const Done& d) {
    const Vehicle& v = d.v;
    os << "\n╔════════════ PHIẾU BẢO DƯỠNG XE ════════════╗\n";
    os << "  Biển số   : " << v.plate << "\n";
    os << "  Chủ xe    : " << v.owner << "\n";
    os << "  Loại xe   : " << VTYPE[v.type] << "\n";
    os << "  Gói       : " << PKG[v.pkg] << "\n";
    os << "  Số km     : " << v.odo << "\n";
    os << "  Giờ vào   : " << hhmm(v.arrival) << "     Giờ ra: " << hhmm(d.outTime) << "\n";
    os << "  ─────────────────────────────────────────────\n";
    os << "  " << padR("Trạm", 26) << padL("Vào", 8) << padL("Ra", 8) << padL("Phút", 8) << "\n";
    int proc = 0;
    for (int k = 0; k < M; ++k) {
        os << "  " << padR(ST_CODE[k] + " " + ST_NAME[k], 26);
        if (v.p[k] == 0) { os << padL("(bỏ qua)", 24) << "\n"; continue; }
        os << padL(hhmm(d.st[k].first), 8) << padL(hhmm(d.st[k].second), 8)
            << padL(to_string(v.p[k]), 8) << "\n";
        proc += v.p[k];
    }
    int flow = d.outTime - v.arrival;
    os << "  ─────────────────────────────────────────────\n";
    os << "  Tổng thời gian xử lý : " << proc << " phút\n";
    os << "  Tổng thời gian chờ   : " << (flow - proc) << " phút\n";
    os << "  Tổng lưu xưởng       : " << flow << " phút\n";
    os << "  Kỹ thuật viên ký: ______________  Khách hàng ký: ______________\n";
    os << "╚═════════════════════════════════════════════╝\n";
}

static void printReport(ostream& os, const Workshop& W) {
    os << "\n════════════ BÁO CÁO CA LÀM VIỆC ════════════\n";
    if (W.done.empty()) { os << "  Chưa có xe nào hoàn thành.\n"; return; }
    long long sumFlow = 0, sumWait = 0;
    for (const auto& d : W.done) {
        int proc = d.v.total();
        sumFlow += d.outTime - d.v.arrival;
        sumWait += (d.outTime - d.v.arrival) - proc;
    }
    int n = (int)W.done.size();
    os << "  Số xe hoàn thành        : " << n << "\n";
    os << "  Giờ bắt đầu ca          : " << hhmm(0) << "\n";
    os << "  Giờ kết thúc            : " << hhmm(W.now) << "  (" << W.now << " phút)\n";
    if (W.sumFifo > 0) {
        long long saved = W.sumFifo - W.sumChosen;
        os << "  Thời gian theo lịch đã chạy : " << W.sumChosen << " phút\n";
        os << "  Nếu làm theo thứ tự đến     : " << W.sumFifo << " phút\n";
        os << "  → Tiết kiệm             : " << saved << " phút";
        if (W.sumFifo > 0) os << fixed << setprecision(1) << "  (-" << 100.0 * saved / W.sumFifo << "%)";
        os << "\n";
    }
    os << "  Thời gian lưu xưởng TB  : " << (double)sumFlow / n << " phút/xe\n";
    os << "  Thời gian chờ TB        : " << (double)sumWait / n << " phút/xe\n";
    os << "\n  Hiệu suất sử dụng trạm:\n";
    int bottleneck = 0;
    for (int k = 1; k < M; ++k) if (W.busy[k] > W.busy[bottleneck]) bottleneck = k;
    for (int k = 0; k < M; ++k) {
        double u = W.now > 0 ? 100.0 * W.busy[k] / W.now : 0.0;
        os << "    " << padR(ST_CODE[k] + " " + ST_NAME[k], 26) << bar(u) << "  "
            << padL(to_string((int)llround(u)) + "%", 5)
            << (k == bottleneck ? "   ← NÚT CỔ CHAI" : "") << "\n";
    }
    os << "\n  Nhận xét: trạm " << ST_NAME[bottleneck] << " bận nhất ("
        << W.busy[bottleneck] << "/" << W.now << " phút).\n";
    os << "  Muốn tăng năng lực xưởng nên bổ sung nhân lực/thiết bị cho trạm này.\n";
    printGantt(os, W);
}

/* =====================================================================
   9. KỊCH BẢN DEMO CÓ SẴN
   ===================================================================== */
static void loadDemo(Workshop& W, int k) {
    if (k == 1) {
        addVehicle(W, "51A-123.45", "Nguyen Van A", 0, 1, 45000, false);
        cout << "  KB1: 1 xe — dây chuyền trống, xe đi một mạch 5 trạm.\n";
    }
    else if (k == 2) {
        addVehicle(W, "51A-123.45", "Nguyen Van A", 0, 1, 45000, false);
        addVehicle(W, "59B-222.22", "Tran Thi B", 1, 2, 78000, false);
        addVehicle(W, "30C-333.33", "Le Van C", 0, 0, 15000, false);
        addVehicle(W, "60D-444.44", "Pham Van D", 1, 1, 120000, false);
        cout << "  KB2: 4 xe cùng vào — so sánh thứ tự đến vs thứ tự đề xuất.\n";
    }
    else if (k == 3) {
        const char* plates[10] = { "51A-001.11","51A-002.22","51A-003.33","51A-004.44","51A-005.55",
                                  "51A-006.66","51A-007.77","51A-008.88","51A-009.99","51A-010.10" };
        for (int i = 0; i < 10; ++i) {
            ostringstream os; os << "Khach " << setw(2) << setfill('0') << i + 1;
            addVehicle(W, plates[i], os.str(), i % 3, (i * 7) % 4, 8000 + i * 13000, false);
        }
        cout << "  KB3: cao điểm 10 xe — xem trạm nút cổ chai & thời gian chờ.\n";
    }
    else if (k == 4) {
        addVehicle(W, "51F-555.55", "Hoang Van E", 0, 3, 135000, false);
        addVehicle(W, "51G-666.66", "Do Thi F", 0, 0, 12000, false);
        addVehicle(W, "29A-777.77", "Bui Van G", 1, 1, 60000, false);
        addVehicle(W, "92B-888.88", "Dang Van H", 2, 2, 95000, false);
        addVehicle(W, "60C-999.99", "Ngo Thi I", 0, 1, 30000, false);
        addVehicle(W, "15A-101.01", "Vu Van K", 1, 0, 22000, false);
        cout << "  KB4: 6 xe, có 1 xe khắc phục lỗi rất nặng — vì sao thứ tự quan trọng.\n";
    }
    cout << "  Hàng chờ hiện có " << W.queue.size() << " xe.\n";
}

/* =====================================================================
   10. CÁC CHỨC NĂNG MENU
   ===================================================================== */
static void showQueue(Workshop& W) {
    cout << "\n── HÀNG CHỜ ──\n";
    if (W.queue.empty()) { cout << "  (trống)\n"; return; }
    cout << "  " << padR("#", 4) << padR("Biển số", 14) << padR("Chủ xe", 18)
        << padR("Loại xe", 20) << padR("Gói", 16) << padL("Km", 9) << padL("Tổng p", 9) << "\n";
    cout << "  " << string(90, '-') << "\n";
    for (size_t i = 0; i < W.queue.size(); ++i) {
        const Vehicle& v = W.queue[i];
        cout << "  " << padR(to_string(i + 1), 4) << padR(v.plate, 14) << padR(v.owner, 18)
            << padR(VTYPE[v.type], 20) << padR(PKG[v.pkg], 16)
            << padL(to_string(v.odo), 9) << padL(to_string(v.total()), 9) << "\n";
    }
    int d = askInt("Hủy xe nào? (số thứ tự, 0 = quay lại): ", 0, (int)W.queue.size(), 0);
    if (d > 0) {
        cout << "  Đã hủy xe " << W.queue[d - 1].plate << " khỏi hàng chờ.\n";
        W.queue.erase(W.queue.begin() + d - 1);
    }
}

static void planAndRun(Workshop& W) {
    if (W.queue.empty()) { cout << "  Hàng chờ trống. Tiếp nhận xe trước (mục 1).\n"; return; }
    int t0 = W.now;
    int n = (int)W.queue.size();

    vector<int> fifo(n); iota(fifo.begin(), fifo.end(), 0);   // thứ tự đến
    Schedule scFifo = buildSchedule(W.queue, fifo, t0);
    vector<int> smart = smartOrder(W.queue);
    Schedule scSmart = buildSchedule(W.queue, smart, t0);

    bool useSmart = scSmart.make <= scFifo.make;              // đề xuất = lịch tốt hơn
    const Schedule& prop = useSmart ? scSmart : scFifo;

    cout << "\n── LẬP LỊCH DÂY CHUYỀN (lô " << n << " xe, bắt đầu " << hhmm(t0) << ") ──\n";
    cout << "  Thứ tự phục vụ đề xuất:\n";
    for (int i = 0; i < n; ++i) {
        const Vehicle& v = W.queue[prop.order[i]];
        cout << "    " << padR(to_string(i + 1) + ".", 4) << padR(v.plate, 14)
            << padR(PKG[v.pkg], 16) << padR(VTYPE[v.type], 20)
            << "(" << v.total() << " phút xử lý)\n";
    }
    cout << "\n  Hoàn thành dự kiến : " << hhmm(prop.make)
        << "  (" << (prop.make - t0) << " phút nữa)\n";
    cout << "  Nếu theo thứ tự đến: " << hhmm(scFifo.make)
        << "  (" << (scFifo.make - t0) << " phút nữa)\n";
    int saved = scFifo.make - prop.make;
    if (saved > 0)
        cout << "  → Tiết kiệm " << saved << " phút (" << fixed << setprecision(1)
        << 100.0 * saved / (scFifo.make - t0) << "%)\n";
    else cout << "  → Thứ tự đến đã là tốt nhất cho lô này.\n";

    int ch = askInt("\nChạy theo: [1] Đề xuất (Enter)  [2] Thứ tự đến  [0] Quay lại: ", 0, 2, 1);
    if (ch == 0) return;
    const Schedule& chosen = (ch == 2) ? scFifo : prop;
    int logMode = askInt("In log: [1] Toàn bộ (Enter)  [2] Từng sự kiện: ", 1, 2, 1);

    W.batch = W.queue;
    W.queue.clear();
    W.sch = chosen;
    W.hasBatch = true;
    W.sumFifo += scFifo.make - t0;
    W.sumChosen += chosen.make - t0;

    runSimulation(W, W.sch, logMode == 2);
    cout << "  Hoàn thành " << W.batch.size() << " xe lúc " << hhmm(W.now)
        << " | Chờ trung bình " << fixed << setprecision(1)
        << (double)W.sch.sumWait / W.batch.size() << " phút/xe\n";
    printGantt(cout, W);
}

static void showDone(Workshop& W) {
    cout << "\n── XE ĐÃ HOÀN THÀNH ──\n";
    if (W.done.empty()) { cout << "  (chưa có)\n"; return; }
    cout << "  " << padR("#", 4) << padR("Biển số", 14) << padR("Gói", 16)
        << padL("Vào", 8) << padL("Ra", 8) << padL("Chờ", 8) << "\n";
    cout << "  " << string(58, '-') << "\n";
    for (size_t i = 0; i < W.done.size(); ++i) {
        const Done& d = W.done[i];
        cout << "  " << padR(to_string(i + 1), 4) << padR(d.v.plate, 14) << padR(PKG[d.v.pkg], 16)
            << padL(hhmm(d.v.arrival), 8) << padL(hhmm(d.outTime), 8)
            << padL(to_string(d.outTime - d.v.arrival - d.v.total()), 8) << "\n";
    }
    int c = askInt("In phiếu xe nào? (số thứ tự, 0 = quay lại): ", 0, (int)W.done.size(), 0);
    if (c > 0) printTicket(cout, W.done[c - 1]);
}

static void exportFiles(Workshop& W) {
    cout << "\n── XUẤT FILE ──\n  1. Báo cáo ca (.txt)\n  2. Lịch lô vừa chạy (.csv)\n"
        "  3. Phiếu tất cả xe (.txt)\n  0. Quay lại\n";
    int c = askInt("Chọn: ", 0, 3, 0);
    if (c == 1) {
        string p = ask("Tên file (Enter = baocao_ca.txt): "); if (p.empty()) p = "baocao_ca.txt";
        ofstream f(p);
        if (!f) { cout << "  ! Không ghi được file.\n"; return; }
        printReport(f, W); cout << "  ✔ Đã ghi " << p << "\n";
    }
    else if (c == 2) {
        if (!W.hasBatch) { cout << "  Chưa có lô nào chạy.\n"; return; }
        string p = ask("Tên file (Enter = lich.csv): "); if (p.empty()) p = "lich.csv";
        ofstream f(p);
        if (!f) { cout << "  ! Không ghi được file.\n"; return; }
        f << "Order,Plate,Package,Station,StartMin,FinishMin,StartTime,EndTime,Proc\n";
        for (int i = 0; i < (int)W.sch.order.size(); ++i)
            for (int k = 0; k < M; ++k) {
                const Task& t = W.sch.tasks[i * M + k];
                const Vehicle& v = W.batch[t.veh];
                f << i + 1 << "," << v.plate << "," << PKG[v.pkg] << "," << ST_NAME[k] << ","
                    << t.s << "," << t.f << "," << hhmm(t.s) << "," << hhmm(t.f) << "," << v.p[k] << "\n";
            }
        cout << "  ✔ Đã ghi " << p << " (mở Excel để vẽ Gantt màu)\n";
    }
    else if (c == 3) {
        if (W.done.empty()) { cout << "  Chưa có xe hoàn thành.\n"; return; }
        string p = ask("Tên file (Enter = phieu_bao_duong.txt): "); if (p.empty()) p = "phieu_bao_duong.txt";
        ofstream f(p);
        if (!f) { cout << "  ! Không ghi được file.\n"; return; }
        for (const auto& d : W.done) printTicket(f, d);
        cout << "  ✔ Đã ghi " << p << "\n";
    }
}

static void settings(Workshop& W) {
    cout << "\n── CÀI ĐẶT ──\n";
    cout << "  Dao động thời gian ±10%: " << (W.jitter ? "BẬT" : "TẮT") << "\n";
    cout << "  Seed ngẫu nhiên        : " << W.seed << "\n";
    cout << "  1. Bật/tắt dao động\n  2. Đổi seed\n  0. Quay lại\n";
    int c = askInt("Chọn: ", 0, 2, 0);
    if (c == 1) { W.jitter = !W.jitter; cout << "  → Dao động: " << (W.jitter ? "BẬT" : "TẮT") << "\n"; }
    else if (c == 2) {
        W.seed = (unsigned)askInt("Seed mới (0..1000000): ", 0, 1000000);
        W.rng.seed(W.seed);
        cout << "  → Đã đổi seed. (Chỉ ảnh hưởng xe tiếp nhận từ giờ trở đi)\n";
    }
}

/* =====================================================================
   11. CHƯƠNG TRÌNH CHÍNH
   ===================================================================== */
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    Workshop W;
    cout << "╔═══════════════════════════════════════════════════════════╗\n";
    cout << "║      HỆ THỐNG DÂY CHUYỀN BẢO DƯỠNG XE  —  CA 08:00        ║\n";
    cout << "║  Tiếp nhận → Rửa → Chẩn đoán → Bảo dưỡng → Kiểm tra       ║\n";
    cout << "╚═══════════════════════════════════════════════════════════╝\n";

    while (true) {
        cout << "\n────────────── MENU XƯỞNG ──────────────\n";
        cout << "Đồng hồ: " << hhmm(W.now) << " | Hàng chờ: " << W.queue.size()
            << " xe | Đã hoàn thành: " << W.done.size() << " xe\n";
        cout << " 1. Tiếp nhận xe vào xưởng\n";
        cout << " 2. Xem hàng chờ / hủy xe\n";
        cout << " 3. Lập lịch & chạy dây chuyền\n";
        cout << " 4. Xem trạng thái các trạm\n";
        cout << " 5. Xe đã hoàn thành + phiếu bảo dưỡng\n";
        cout << " 6. Báo cáo ca làm việc\n";
        cout << " 7. Nạp kịch bản demo có sẵn\n";
        cout << " 8. Cài đặt (dao động thời gian, seed)\n";
        cout << " 9. Xuất file (báo cáo, lịch CSV, phiếu)\n";
        cout << " 0. Thoát\n";
        int c = askInt("Chọn chức năng: ", 0, 9);

        if (c == 0) { cout << "Kết thúc ca. Tạm biệt!\n"; break; }
        switch (c) {
        case 1: receiveVehicle(W); break;
        case 2: showQueue(W); break;
        case 3: planAndRun(W); break;
        case 4: showStatus(W); break;
        case 5: showDone(W); break;
        case 6: printReport(cout, W); break;
        case 7: {
            cout << "\n── KỊCH BẢN DEMO ──\n";
            cout << "  1. KB1 – Xe đầu tiên trong ca (1 xe)\n";
            cout << "  2. KB2 – 4 xe cùng vào (so thứ tự đến vs đề xuất)\n";
            cout << "  3. KB3 – Cao điểm 10 xe (nút cổ chai)\n";
            cout << "  4. KB4 – 6 xe, có xe hỏng nặng\n";
            cout << "  5. Bắt đầu ca mới (xóa toàn bộ)\n";
            cout << "  0. Quay lại\n";
            int k = askInt("Chọn: ", 0, 5, 0);
            if (k >= 1 && k <= 4) loadDemo(W, k);
            else if (k == 5) { W = Workshop(); cout << "  ✔ Đã mở ca mới.\n"; }
            break;
        }
        case 8: settings(W); break;
        case 9: exportFiles(W); break;
        }
    }
    return 0;
}