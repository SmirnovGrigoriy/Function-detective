// ================================================================
//  Функция-детектив v8.7
//  Сборка БЕЗ дополнительных флагов:
//     g++ -std=c++17 -O2 -g game.cpp -o game.exe
//  (works on Linux/macOS/Windows MinGW/MSVC, -lws2_32 НЕ НУЖЕН)
// ================================================================

#if defined(_MSVC_LANG)
    #if _MSVC_LANG < 201103L
        #error "Требуется C++11 или новее."
    #endif
#elif __cplusplus < 201103L
    #error "Требуется C++11 или новее."
#endif

#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <clocale>
#include <charconv>
#include <stdexcept>
#include <limits>
#include <vector>
#include <random>
#include <numeric>
#include <algorithm>
#include <set>
#include <utility>

// =============================================================
//  Динамическая загрузка ws2_32.dll
// =============================================================
#ifdef _WIN32
    typedef SOCKET sock_t;
    static const sock_t INVALID_SOCK = INVALID_SOCKET;

    namespace ws {
        extern "C" {
            typedef int    (WSAAPI *tWSAStartup)(WORD, LPWSADATA);
            typedef int    (WSAAPI *tWSACleanup)(void);
            typedef SOCKET (WSAAPI *tsocket)(int, int, int);
            typedef int    (WSAAPI *tbind)(SOCKET, const struct sockaddr*, int);
            typedef int    (WSAAPI *tlisten)(SOCKET, int);
            typedef SOCKET (WSAAPI *taccept)(SOCKET, struct sockaddr*, int*);
            typedef int    (WSAAPI *tconnect)(SOCKET, const struct sockaddr*, int);
            typedef int    (WSAAPI *tsend)(SOCKET, const char*, int, int);
            typedef int    (WSAAPI *trecv)(SOCKET, char*, int, int);
            typedef int    (WSAAPI *tclosesocket)(SOCKET);
            typedef int    (WSAAPI *tselect)(int, fd_set*, fd_set*, fd_set*, const struct timeval*);
            typedef int    (WSAAPI *tsetsockopt)(SOCKET, int, int, const char*, int);
            typedef int    (WSAAPI *tgethostname)(char*, int);
            typedef struct hostent* (WSAAPI *tgethostbyname)(const char*);
            typedef char*  (WSAAPI *tinet_ntoa)(struct in_addr);
            typedef int    (WSAAPI *tinet_pton)(int, const char*, void*);
        }
        static tWSAStartup    pWSAStartup    = nullptr;
        static tWSACleanup    pWSACleanup    = nullptr;
        static tsocket        psocket        = nullptr;
        static tbind          pbind          = nullptr;
        static tlisten        plisten        = nullptr;
        static taccept        paccept        = nullptr;
        static tconnect       pconnect       = nullptr;
        static tsend          psend          = nullptr;
        static trecv          precv          = nullptr;
        static tclosesocket   pclosesocket   = nullptr;
        static tselect        pselect        = nullptr;
        static tsetsockopt    psetsockopt    = nullptr;
        static tgethostname   pgethostname   = nullptr;
        static tgethostbyname pgethostbyname = nullptr;
        static tinet_ntoa     pinet_ntoa     = nullptr;
        static tinet_pton     pinet_pton     = nullptr;

        static bool load() {
            if (pWSAStartup) return true;
            HMODULE h = LoadLibraryA("ws2_32.dll");
            if (!h) return false;
            #define WSL(n) do { p##n = (t##n)GetProcAddress(h, #n); if (!p##n) return false; } while(0)
            WSL(WSAStartup);  WSL(WSACleanup);  WSL(socket);    WSL(bind);
            WSL(listen);      WSL(accept);      WSL(connect);   WSL(send);
            WSL(recv);        WSL(closesocket); WSL(select);    WSL(setsockopt);
            WSL(gethostname); WSL(gethostbyname); WSL(inet_ntoa); WSL(inet_pton);
            #undef WSL
            return true;
        }
    }
#else
    typedef int sock_t;
    static const sock_t INVALID_SOCK = -1;
#endif

// =============================================================
//  Обёртки сокетов
// =============================================================
#ifdef _WIN32
    static sock_t sys_socket(int a, int b, int c) { return ws::psocket(a, b, c); }
    static int    sys_bind(sock_t s, const struct sockaddr* a, int l) { return ws::pbind(s, a, l); }
    static int    sys_listen(sock_t s, int b)      { return ws::plisten(s, b); }
    static sock_t sys_accept(sock_t s, struct sockaddr* a, int* l) { return ws::paccept(s, a, l); }
    static int    sys_connect(sock_t s, const struct sockaddr* a, int l) { return ws::pconnect(s, a, l); }
    static int    sys_send(sock_t s, const char* b, int l, int f) { return ws::psend(s, b, l, f); }
    static int    sys_recv(sock_t s, char* b, int l, int f) { return ws::precv(s, b, l, f); }
    static int    sys_closesocket(sock_t s) { return ws::pclosesocket ? ws::pclosesocket(s) : -1; }
    static int    sys_select(int n, fd_set* r, fd_set* w, fd_set* e, const struct timeval* t) { return ws::pselect(n, r, w, e, t); }
    static int    sys_setsockopt(sock_t s, int a, int b, const char* c, int l) { return ws::psetsockopt(s, a, b, c, l); }
    static int    sys_gethostname(char* b, int l)  { return ws::pgethostname(b, l); }
    static struct hostent* sys_gethostbyname(const char* n) { return ws::pgethostbyname(n); }
    static char*  sys_inet_ntoa(struct in_addr a)  { return ws::pinet_ntoa(a); }
    static int    sys_inet_pton(int a, const char* b, void* c) { return ws::pinet_pton(a, b, c); }
    static unsigned short sys_htons(unsigned short x) { return (unsigned short)(((x & 0xFFu) << 8) | ((x >> 8) & 0xFFu)); }
#else
    static sock_t sys_socket(int a, int b, int c) { return ::socket(a, b, c); }
    static int    sys_bind(sock_t s, const struct sockaddr* a, int l) { return ::bind(s, a, (socklen_t)l); }
    static int    sys_listen(sock_t s, int b)      { return ::listen(s, b); }
    static sock_t sys_accept(sock_t s, struct sockaddr* a, int* l) {
        socklen_t sl = l ? (socklen_t)*l : 0;
        sock_t r = ::accept(s, a, l ? &sl : nullptr);
        if (l) *l = (int)sl;
        return r;
    }
    static int    sys_connect(sock_t s, const struct sockaddr* a, int l) { return ::connect(s, a, (socklen_t)l); }
    static int    sys_send(sock_t s, const char* b, int l, int f) { return (int)::send(s, b, (size_t)l, f); }
    static int    sys_recv(sock_t s, char* b, int l, int f) { return (int)::recv(s, b, (size_t)l, f); }
    static int    sys_closesocket(sock_t s)        { return ::close(s); }
    static int    sys_select(int n, fd_set* r, fd_set* w, fd_set* e, const struct timeval* t) { return ::select(n, r, w, e, const_cast<struct timeval*>(t)); }
    static int    sys_setsockopt(sock_t s, int a, int b, const char* c, int l) { return ::setsockopt(s, a, b, c, (socklen_t)l); }
    static int    sys_gethostname(char* b, int l)  { return ::gethostname(b, (size_t)l); }
    static struct hostent* sys_gethostbyname(const char* n) { return ::gethostbyname(n); }
    static char*  sys_inet_ntoa(struct in_addr a)  { return ::inet_ntoa(a); }
    static int    sys_inet_pton(int a, const char* b, void* c) { return ::inet_pton(a, b, c); }
    static unsigned short sys_htons(unsigned short x) { return htons(x); }
#endif
static inline void closeSock(sock_t s) { sys_closesocket(s); }

// =============================================================
//  ВЫВОД ЧЕРЕЗ WriteConsoleW
// =============================================================
#ifdef _WIN32
class Utf8CoutBuf : public std::streambuf {
public:
    explicit Utf8CoutBuf(std::ostream& out, DWORD stdHandle = STD_OUTPUT_HANDLE)
        : out_(out), orig_(out.rdbuf()), handle_(GetStdHandle(stdHandle)) {
        out_.rdbuf(this);
    }
    ~Utf8CoutBuf() {
        sync();
        out_.rdbuf(orig_);
    }
protected:
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        buffer_.append(s, (size_t)n);
        return n;
    }
    int overflow(int c) override {
        if (c != EOF) buffer_ += (char)c;
        return c;
    }
    int sync() override {
        if (!buffer_.empty()) { flush(buffer_); buffer_.clear(); }
        return 0;
    }
private:
    void flush(const std::string& s) {
        DWORD mode;
        if (!GetConsoleMode(handle_, &mode)) {
            DWORD written;
            WriteFile(handle_, s.data(), (DWORD)s.size(), &written, nullptr);
            return;
        }
        auto tryWrite = [&](UINT cp) -> bool {
            int wlen = MultiByteToWideChar(cp, cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
                                           s.data(), (int)s.size(), nullptr, 0);
            if (wlen <= 0) return false;
            std::wstring wbuf((size_t)wlen, L'\0');
            MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), &wbuf[0], wlen);
            DWORD written;
            WriteConsoleW(handle_, wbuf.data(), (DWORD)wlen, &written, nullptr);
            return true;
        };
        if (!tryWrite(CP_UTF8)) tryWrite(1251);
    }
    std::ostream& out_;
    std::streambuf* orig_;
    HANDLE handle_;
    std::string buffer_;
};
#endif

// =============================================================
//  Цвета
// =============================================================
namespace Col {
    static std::string RST, RED, GRN, YEL, CYA, MAG, BOLD, DIM;
    static void init(bool useColor) {
        RST  = useColor ? "\033[0m"  : "";
        RED  = useColor ? "\033[31m" : "";
        GRN  = useColor ? "\033[32m" : "";
        YEL  = useColor ? "\033[33m" : "";
        CYA  = useColor ? "\033[36m" : "";
        MAG  = useColor ? "\033[35m" : "";
        BOLD = useColor ? "\033[1m"  : "";
        DIM  = useColor ? "\033[2m"  : "";
    }
}

static bool enableAnsi() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0;
    if (!GetConsoleMode(h, &m)) return false;
    if (!SetConsoleMode(h, m | 0x0004)) return false;
    return true;
#else
    return true;
#endif
}

// =============================================================
//  Точные дроби + Лагранж
// =============================================================
static long long gcdll(long long a, long long b) {
    a = std::llabs(a); b = std::llabs(b);
    while (b) { long long t = a % b; a = b; b = t; }
    return a;
}
struct Frac {
    long long n, d;
    Frac(long long n_ = 0, long long d_ = 1) : n(n_), d(d_) { norm(); }
    void norm() {
        if (d < 0) { n = -n; d = -d; }
        long long g = gcdll(n, d);
        if (g > 1) { n /= g; d /= g; }
    }
    Frac operator+(const Frac& o) const { return Frac(n*o.d + o.n*d, d*o.d); }
    Frac operator*(const Frac& o) const { return Frac(n*o.n, d*o.d); }
    Frac operator/(const Frac& o) const { return Frac(n*o.d, d*o.n); }
    Frac operator-() const { return Frac(-n, d); }
    bool isZero() const { return n == 0; }
};
struct Point { double x, y; };

static std::vector<Frac> polyMul(const std::vector<Frac>& a, const std::vector<Frac>& b) {
    std::vector<Frac> r(a.size() + b.size() - 1, Frac(0));
    for (size_t i = 0; i < a.size(); i++)
        for (size_t j = 0; j < b.size(); j++)
            r[i + j] = r[i + j] + a[i] * b[j];
    return r;
}
static std::vector<Frac> lagrangeCoefs(const std::vector<Point>& pts) {
    int n = (int)pts.size();
    std::vector<Frac> res(n, Frac(0));
    for (int i = 0; i < n; ++i) {
        std::vector<Frac> num = {Frac(1)};
        Frac den(1);
        for (int j = 0; j < n; ++j) {
            if (j == i) continue;
            long long xj = (long long)std::llround(pts[j].x);
            num = polyMul(num, {Frac(-xj), Frac(1)});
            den = den * Frac((long long)std::llround(pts[i].x - pts[j].x));
        }
        Frac scale = Frac((long long)std::llround(pts[i].y)) / den;
        for (size_t k = 0; k < num.size(); ++k) res[k] = res[k] + num[k] * scale;
    }
    return res;
}
static std::string formatPoly(const std::vector<Frac>& c) {
    std::string s; bool first = true;
    for (int k = (int)c.size() - 1; k >= 0; --k) {
        if (c[k].isZero()) continue;
        long long n = c[k].n, d = c[k].d;
        bool neg = n < 0; if (neg) n = -n;
        if (!first) s += neg ? " - " : " + ";
        else if (neg) s += "-";
        if (k == 0) s += (d == 1) ? std::to_string(n)
                                  : "(" + std::to_string(n) + "/" + std::to_string(d) + ")";
        else {
            bool one = (n == 1 && d == 1);
            if (!one) s += (d == 1) ? std::to_string(n) + "*"
                                    : "(" + std::to_string(n) + "/" + std::to_string(d) + ")*";
            s += (k == 1) ? "x" : ("x**" + std::to_string(k));
        }
        first = false;
    }
    return s.empty() ? "0" : s;
}
static std::string makeLagrangeExpr(const std::vector<Point>& pts) {
    try {
        auto coefs = lagrangeCoefs(pts);
        for (auto& c : coefs)
            if (std::llabs(c.n) > 1000000000000LL || c.d > 1000000000000LL)
                throw std::runtime_error("overflow");
        return formatPoly(coefs);
    } catch (...) { return ""; }
}

// =============================================================
//  Матфункции
// =============================================================
static double fact_d(double v) {
    if (v >= 0 && v == std::floor(v) && v <= 170) {
        double r = 1.0;
        for (int i = 2; i <= (int)v; ++i) r *= i;
        return r;
    }
    return std::tgamma(v + 1.0);
}
static double combinations(double n_, double k_) {
    long long n = (long long)std::llround(n_), k = (long long)std::llround(k_);
    if (k < 0 || n < 0 || k > n) return 0.0;
    if (k > n - k) k = n - k;
    double r = 1.0;
    for (long long i = 0; i < k; ++i) r = r * (double)(n - i) / (double)(i + 1);
    return std::round(r);
}
static double permutations(double n_, double k_) {
    long long n = (long long)std::llround(n_), k = (long long)std::llround(k_);
    if (k < 0 || n < 0 || k > n) return 0.0;
    double r = 1.0;
    for (long long i = 0; i < k; ++i) r *= (double)(n - i);
    return r;
}
static double fib_d(double n_) {
    long long n = (long long)n_; bool neg = n < 0;
    if (neg) n = -n;
    double a = 0, b = 1;
    for (long long i = 0; i < n; ++i) { double c = a + b; a = b; b = c; }
    return (neg && (n % 2 == 0)) ? -a : a;
}
static bool is_prime_ll(long long n) {
    if (n < 2) return false;
    if (n < 4) return true;
    if (n % 2 == 0) return false;
    for (long long i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}
static double next_prime_d(double v) {
    long long n = (long long)std::floor(v) + 1;
    if (n < 2) n = 2;
    while (!is_prime_ll(n)) ++n;
    return (double)n;
}
static double digit_sum_d(double v) {
    long long n = (long long)std::fabs(v), s = 0;
    while (n > 0) { s += n % 10; n /= 10; }
    return (double)s;
}
static double reverse_digits_d(double v) {
    bool neg = v < 0;
    long long n = (long long)std::fabs(v), r = 0;
    while (n > 0) { r = r * 10 + n % 10; n /= 10; }
    return neg ? -(double)r : (double)r;
}
static double count_digits_d(double v) {
    long long n = (long long)std::fabs(v);
    if (n == 0) return 1.0;
    int c = 0;
    while (n > 0) { ++c; n /= 10; }
    return (double)c;
}

// =============================================================
//  Парсер
// =============================================================
class ExprParser {
public:
    ExprParser(const std::string& s, double x) : src(s), pos(0), xval(x) {}
    double parse() {
        skip();
        double r = parseBitOr();
        skip();
        if (pos < src.size())
            throw std::runtime_error(std::string("Лишний символ '") + src[pos] +
                                     "' в позиции " + std::to_string(pos));
        return r;
    }
private:
    std::string src; size_t pos; double xval;
    bool stopAtBar = false;

    void skip() { while (pos < src.size() && std::isspace((unsigned char)src[pos])) ++pos; }
    bool consume(const char* t) {
        size_t n = std::strlen(t);
        if (src.compare(pos, n, t) == 0) { pos += n; return true; }
        return false;
    }
    char pk() const { return pos < src.size() ? src[pos] : '\0'; }
    char pk(int o) const { return pos + o < src.size() ? src[pos + o] : '\0'; }
    double sh(long long v, long long s, bool l) const {
        int b = (int)(sizeof(long long) * 8);
        if (s < 0 || s >= b) return 0.0;
        return (double)(l ? (v << s) : (v >> s));
    }

    double parseBitOr() {
        double l = parseBitXor();
        if (stopAtBar) return l;
        while (true) { skip();
            if (pk() == '|') { ++pos; double r = parseBitXor(); l = (double)((long long)l | (long long)r); }
            else break; }
        return l;
    }
    double parseBitXor() {
        double l = parseBitAnd();
        while (true) { skip();
            if (pk() == '^') { ++pos; double r = parseBitAnd(); l = (double)((long long)l ^ (long long)r); }
            else break; }
        return l;
    }
    double parseBitAnd() {
        double l = parseShift();
        while (true) { skip();
            if (pk() == '&') { ++pos; double r = parseShift(); l = (double)((long long)l & (long long)r); }
            else break; }
        return l;
    }
    double parseShift() {
        double l = parseAdd();
        while (true) { skip();
            if (consume("<<")) l = sh((long long)l, (long long)parseAdd(), true);
            else if (consume(">>")) l = sh((long long)l, (long long)parseAdd(), false);
            else break; }
        return l;
    }
    double parseAdd() {
        double l = parseMul();
        while (true) { skip();
            if (pk() == '+') { ++pos; l += parseMul(); }
            else if (pk() == '-') { ++pos; l -= parseMul(); }
            else break; }
        return l;
    }
    double parseMul() {
        double l = parseUnary();
        while (true) { skip();
            char c = pk();
            if (c == '*') { if (pk(1) == '*') break; ++pos; l *= parseUnary(); }
            else if (c == '/') {
                ++pos;
                if (pk() == '/') { ++pos; l = std::floor(l / parseUnary()); }
                else l /= parseUnary();
            } else if (c == '%') { ++pos; l = std::fmod(l, parseUnary()); }
            else break; }
        return l;
    }
    double parseUnary() {
        skip();
        char c = pk();
        if (c == '-') { ++pos; return -parseUnary(); }
        if (c == '+') { ++pos; return  parseUnary(); }
        if (c == '~') { ++pos; return (double)(~(long long)parseUnary()); }
        return parsePower();
    }
    double parsePower() {
        double b = parsePostfix();
        skip();
        if (pk() == '*' && pk(1) == '*') { pos += 2; return std::pow(b, parseUnary()); }
        return b;
    }
    double parsePostfix() {
        double v = parsePrimary();
        while (true) { skip();
            if (pk() == '!') { ++pos; v = fact_d(v); }
            else break; }
        return v;
    }
    double parsePrimary() {
        skip();
        if (pos >= src.size()) throw std::runtime_error("Неожиданный конец");
        if (pk() == '|') {
            ++pos;
            bool saved = stopAtBar;
            stopAtBar = true;
            double v = parseBitOr();
            stopAtBar = saved;
            skip();
            if (pk() != '|') throw std::runtime_error("Ожидалось закрывающее '|'");
            ++pos;
            return std::fabs(v);
        }
        if (pk() == '(') {
            ++pos;
            bool saved = stopAtBar;
            stopAtBar = false;
            double r = parseBitOr();
            stopAtBar = saved;
            skip();
            if (pk() != ')') throw std::runtime_error("Ожидалось ')' в позиции " + std::to_string(pos));
            ++pos;
            return r;
        }
        if (std::isdigit((unsigned char)pk()) || pk() == '.') return parseNum();
        if (std::isalpha((unsigned char)pk()) || pk() == '_') return parseIdent();
        throw std::runtime_error(std::string("Неожиданный символ '") + pk() + "'");
    }
    double parseNum() {
        size_t st = pos; bool dot = false;
        while (pos < src.size() && (std::isdigit((unsigned char)src[pos]) || src[pos] == '.')) {
            if (src[pos] == '.') { if (dot) break; dot = true; }
            ++pos;
        }
        if (pos < src.size() && (src[pos]=='e' || src[pos]=='E')) {
            size_t sv = pos; ++pos;
            if (pos < src.size() && (src[pos]=='+'||src[pos]=='-')) ++pos;
            if (pos < src.size() && std::isdigit((unsigned char)src[pos]))
                while (pos < src.size() && std::isdigit((unsigned char)src[pos])) ++pos;
            else pos = sv;
        }
        std::string s = src.substr(st, pos - st);
        // from_chars не зависит от локали: '.' — всегда десятичный разделитель.
        double v = 0.0;
        auto rc = std::from_chars(s.data(), s.data() + s.size(), v);
        if (rc.ec != std::errc() || rc.ptr != s.data() + s.size())
            throw std::runtime_error("Плохое число: " + s);
        return v;
    }
    double parseIdent() {
        size_t st = pos;
        while (pos < src.size() && (std::isalnum((unsigned char)src[pos]) || src[pos] == '_')) ++pos;
        std::string id = src.substr(st, pos - st);
        for (auto& c : id) c = (char)std::tolower((unsigned char)c);

        if (id == "x")   return xval;
        if (id == "pi")  return 3.14159265358979323846;
        if (id == "e")   return 2.71828182845904523536;
        if (id == "tau") return 6.28318530717958647692;
        if (id == "phi") return (1.0 + std::sqrt(5.0)) / 2.0;
        if (id == "eps") return std::numeric_limits<double>::epsilon();
        if (id == "inf") return std::numeric_limits<double>::infinity();

        skip();
        if (pos >= src.size() || src[pos] != '(')
            throw std::runtime_error("Ожидалась '(' после '" + id + "'");
        ++pos;
        std::vector<double> a; skip();
        if (pk() != ')') {
            a.push_back(parseBitOr()); skip();
            while (pk() == ',') { ++pos; a.push_back(parseBitOr()); skip(); }
        }
        if (pk() != ')') throw std::runtime_error("Ожидалась ')' после '" + id + "'");
        ++pos;
        return dispatch(id, a);
    }
    double dispatch(const std::string& id, std::vector<double>& a) {
        size_t n = a.size();
        auto need = [&](size_t k) {
            if (n != k) throw std::runtime_error("'" + id + "' требует " + std::to_string(k) + " арг.");
        };
        if (id=="pow")   { need(2); return std::pow(a[0], a[1]); }
        if (id=="hypot") { need(2); return std::hypot(a[0], a[1]); }
        if (id=="fmod")  { need(2); return std::fmod(a[0], a[1]); }
        if (id=="atan2") { need(2); return std::atan2(a[0], a[1]); }
        if (id=="c"||id=="comb"||id=="binom"||id=="ncr"||id=="choose") { need(2); return combinations(a[0], a[1]); }
        if (id=="p"||id=="perm"||id=="npr") { need(2); return permutations(a[0], a[1]); }
        if (id=="gcd") { need(2);
            long long x=(long long)std::fabs(a[0]), y=(long long)std::fabs(a[1]);
            while (y) { long long t=x%y; x=y; y=t; }
            return (double)x;
        }
        if (id=="lcm") { need(2);
            long long x=(long long)std::fabs(a[0]), y=(long long)std::fabs(a[1]);
            if (!x||!y) return 0.0;
            long long g=x, h=y;
            while (h) { long long t=g%h; g=h; h=t; }
            return (double)(x/g*y);
        }
        if (id=="lerp") { need(3); return a[0]+(a[1]-a[0])*a[2]; }

        need(1);
        double v = a[0];
        if (id=="sin")  return std::sin(v);
        if (id=="cos")  return std::cos(v);
        if (id=="tan")  return std::tan(v);
        if (id=="sec")  return 1.0/std::cos(v);
        if (id=="csc")  return 1.0/std::sin(v);
        if (id=="cot")  return 1.0/std::tan(v);
        if (id=="asin") return std::asin(v);
        if (id=="acos") return std::acos(v);
        if (id=="atan") return std::atan(v);
        if (id=="sinh") return std::sinh(v);
        if (id=="cosh") return std::cosh(v);
        if (id=="tanh") return std::tanh(v);
        if (id=="sech") return 1.0/std::cosh(v);
        if (id=="csch") return 1.0/std::sinh(v);
        if (id=="coth") return 1.0/std::tanh(v);
        if (id=="asinh") return std::asinh(v);
        if (id=="acosh") return std::acosh(v);
        if (id=="atanh") return std::atanh(v);
        if (id=="deg") return v * 180.0 / 3.14159265358979323846;
        if (id=="rad") return v * 3.14159265358979323846 / 180.0;
        if (id=="erf")  return std::erf(v);
        if (id=="erfc") return std::erfc(v);
        if (id=="fact"||id=="factorial") return fact_d(v);
        if (id=="fib"||id=="fibonacci")  return fib_d(v);
        if (id=="isprime")   return is_prime_ll((long long)v) ? 1.0 : 0.0;
        if (id=="nextprime") return next_prime_d(v);
        if (id=="is_even"||id=="even") return ((long long)v%2==0)?1.0:0.0;
        if (id=="is_odd" ||id=="odd")  return ((long long)v%2!=0)?1.0:0.0;
        if (id=="digit_sum"||id=="digitsum") return digit_sum_d(v);
        if (id=="reverse"||id=="rev") return reverse_digits_d(v);
        if (id=="count_digits"||id=="ndigits") return count_digits_d(v);
        if (id=="tri"||id=="triangular") return v*(v+1.0)/2.0;
        if (id=="sqrt") return std::sqrt(v);
        if (id=="cbrt") return std::cbrt(v);
        if (id=="abs")  return std::fabs(v);
        if (id=="exp")  return std::exp(v);
        if (id=="expm1") return std::expm1(v);
        if (id=="ln"||id=="log") return std::log(v);
        if (id=="log2") return std::log2(v);
        if (id=="log10") return std::log10(v);
        if (id=="log1p") return std::log1p(v);
        if (id=="floor") return std::floor(v);
        if (id=="ceil")  return std::ceil(v);
        if (id=="round") return std::round(v);
        if (id=="trunc") return std::trunc(v);
        if (id=="fract") return v - std::floor(v);
        if (id=="sign"||id=="sgn") return (double)((v>0)-(v<0));
        if (id=="sqr") return v*v;
        if (id=="gamma") return std::tgamma(v);
        if (id=="lgamma") return std::lgamma(v);
        if (id=="popcount") {
            unsigned long long u = (unsigned long long)(long long)v;
            int c = 0; while (u) { c += (int)(u&1); u >>= 1; }
            return (double)c;
        }
        if (id=="clz") {
            unsigned long long u = (unsigned long long)(long long)v;
            if (!u) return 64.0;
            int c = 0; while (!(u & (1ULL<<63))) { u <<= 1; ++c; }
            return (double)c;
        }
        if (id=="ctz") {
            unsigned long long u = (unsigned long long)(long long)v;
            if (!u) return 64.0;
            int c = 0; while (!(u & 1ULL)) { u >>= 1; ++c; }
            return (double)c;
        }
        throw std::runtime_error("Неизвестная функция: " + id);
    }
};

// =============================================================
//  Утилиты
// =============================================================
static std::string trimLower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}
static std::string stripSpaces(const std::string& s) {
    std::string r;
    for (char c : s) if (!std::isspace((unsigned char)c)) r += c;
    return r;
}
static std::string preprocess(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '=') continue;
        return s.substr(i + 1);
    }
    return s;
}
static bool hasBitwise(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '~' || c == '^' || c == '&') return true;
        if (c == '<' && i+1 < s.size() && s[i+1] == '<') return true;
        if (c == '>' && i+1 < s.size() && s[i+1] == '>') return true;
    }
    return false;
}
static std::string complexityRating(size_t len) {
    if (len <= 10) return "S";
    if (len <= 20) return "A";
    if (len <= 30) return "B";
    if (len <= 50) return "C";
    return "D";
}
static std::string rateColor(const std::string& r) {
    if (r == "S") return Col::MAG;
    if (r == "A") return Col::GRN;
    if (r == "B") return Col::CYA;
    if (r == "C") return Col::YEL;
    return Col::RED;
}
static std::vector<Point> generatePoints(int n, std::mt19937& rng) {
    std::uniform_int_distribution<int> d(-5, 5);
    std::vector<Point> pts; std::set<int> used;
    while ((int)pts.size() < n) {
        int xi = d(rng);
        if (used.count(xi)) continue;
        used.insert(xi);
        pts.push_back({(double)xi, (double)d(rng)});
    }
    return pts;
}
static int evaluateHits(const std::string& expr, const std::vector<Point>& pts) {
    int hc = 0;
    for (auto& p : pts) {
        try {
            ExprParser par(expr, p.x);
            double y = par.parse();
            double tol = 1e-6 * (1.0 + std::fabs(p.y));
            if (std::isfinite(y) && std::fabs(y - p.y) < tol) hc++;
        } catch (...) {}
    }
    return hc;
}

// =============================================================
//  СЕТЬ
// =============================================================
#ifndef DISABLE_NETWORK
static bool netInit() {
#ifdef _WIN32
    if (!ws::load()) return false;
    WSADATA wsa;
    return ws::pWSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}
static void netCleanup() {
#ifdef _WIN32
    if (ws::pWSACleanup) ws::pWSACleanup();
#endif
}

class NetConn {
public:
    NetConn() : sock(INVALID_SOCK) {}
    explicit NetConn(sock_t s) : sock(s) {}
    ~NetConn() { disconnect(); }
    NetConn(const NetConn&) = delete;
    NetConn& operator=(const NetConn&) = delete;
    bool valid() const { return sock != INVALID_SOCK; }
    void disconnect() { if (sock != INVALID_SOCK) { closeSock(sock); sock = INVALID_SOCK; } }
    bool sendLine(const std::string& line) {
        if (!valid()) return false;
        std::string data = line + "\n";
        size_t total = 0;
        while (total < data.size()) {
            int sent = sys_send(sock, data.data() + total, (int)(data.size() - total), 0);
            if (sent <= 0) return false;
            total += (size_t)sent;
        }
        return true;
    }
    bool recvLine(std::string& out) {
        out.clear();
        if (!valid()) return false;
        char c;
        while (true) {
            int r = sys_recv(sock, &c, 1, 0);
            if (r <= 0) return false;
            if (c == '\n') return true;
            if (c != '\r') out += c;
        }
    }
private:
    sock_t sock;
};

static std::string getLocalIp() {
    char hostname[256];
    if (sys_gethostname(hostname, (int)sizeof(hostname)) != 0) return "";
    struct hostent* he = sys_gethostbyname(hostname);
    if (!he || !he->h_addr_list[0]) return "";
    struct in_addr a;
    std::memcpy(&a, he->h_addr_list[0], sizeof(a));
    return std::string(sys_inet_ntoa(a));
}

static bool sendPoints(NetConn& c, const std::vector<Point>& pts) {
    if (!c.sendLine("POINTS " + std::to_string(pts.size()))) return false;
    for (auto& p : pts) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6f %.6f", p.x, p.y);
        if (!c.sendLine(buf)) return false;
    }
    return true;
}
static bool recvPoints(NetConn& c, std::vector<Point>& pts) {
    std::string line;
    if (!c.recvLine(line)) return false;
    if (line.rfind("POINTS ", 0) != 0) return false;
    int n = 0;
    try { n = std::stoi(line.substr(7)); } catch (...) { return false; }
    pts.clear();
    for (int i = 0; i < n; i++) {
        if (!c.recvLine(line)) return false;
        double x, y;
        if (std::sscanf(line.c_str(), "%lf %lf", &x, &y) != 2) return false;
        pts.push_back({x, y});
    }
    return true;
}
#endif

// =============================================================
//  Справка / подсказки
// =============================================================
static void printHelp() {
    std::cout << "-------------------------------------------------------------\n";
    std::cout << "Переменная: x   Константы: pi, e, tau, phi, eps, inf\n";
    std::cout << "Арифметика: + - * / % //   Степень: **   Факториал: n! / fact(n)\n";
    std::cout << "Модуль: |x| или abs(x)\n";
    std::cout << "Битовые: & | ^ << >> ~\n";
    std::cout << "Сравнения и логические операторы отключены.\n";
    std::cout << "Дробные числа: 0.5, 2.75, 1e-3 (разделитель — точка)\n";
    std::cout << "\n--- Тригонометрия ---\n";
    std::cout << "  sin cos tan sec csc cot\n";
    std::cout << "  asin acos atan atan2(y,x)\n";
    std::cout << "  sinh cosh tanh sech csch coth asinh acosh atanh\n";
    std::cout << "  deg rad   erf erfc\n";
    std::cout << "\n--- Популярные ---\n";
    std::cout << "  fact C P fib isprime nextprime is_even is_odd\n";
    std::cout << "  digit_sum reverse count_digits triangular gcd lcm lerp\n";
    std::cout << "  sqrt cbrt abs exp expm1 ln log log2 log10 log1p\n";
    std::cout << "  floor ceil round trunc fract sign gamma lgamma\n";
    std::cout << "\n--- Команды ---\n";
    std::cout << "  auto / poly K   (тренировка)\n";
    std::cout << "  history · undo · hint · help · back\n";
    std::cout << "  end — итоги и выход; exit — выход\n";
    std::cout << "-------------------------------------------------------------\n";
}
static void printHint(int n, int attempts) {
    std::cout << "Подсказка: ";
    if      (n == 2) std::cout << "через 2 точки проходит прямая.\n";
    else if (n == 3) std::cout << "через 3 точки — парабола.\n";
    else if (n == 4) std::cout << "через 4 точки — кубическая.\n";
    else             std::cout << "многочлен степени " << (n-1) << " через все точки.\n";
    if (attempts >= 3)
        std::cout << "(Попробуй 'auto' или 'poly K'.)\n";
}

// =============================================================
//  Рекорды / сессия
// =============================================================
struct Records { int score[4]; Records() { score[0]=score[1]=score[2]=score[3]=0; } };
static void loadRecords(Records& r) {
    std::ifstream f("records.txt"); if (!f) return;
    for (int i = 0; i < 4; i++) if (!(f >> r.score[i])) r.score[i] = 0;
}
static void saveRecords(const Records& r) {
    std::ofstream f("records.txt"); if (!f) return;
    for (int i = 0; i < 4; i++) f << r.score[i] << "\n";
}
struct Session {
    int totalAttempts = 0;
    bool anyAttempt = false, anyPerfect = false, anySpeedrun = false;
    bool usedAutoAnywhere = false, minimalist = false, bitwisePerfect = false;
    bool explorer = false, pveChampion = false;
    int pvpGames = 0;
    bool pvpChampion = false;
};
struct Attempt { std::string expr; int hits; int len; };

static void showStats(const Session& s, const Records& r) {
    std::cout << "\n========== ИТОГИ ==========\n\n";
    const char* names[4] = {"Лёгкий", "Средний", "Сложный", "Безумный"};
    const int maxes[4] = {2, 3, 4, 6};
    std::cout << "Уровень     Рекорд  Максимум\n";
    for (int i = 0; i < 4; i++) {
        std::cout << names[i];
        for (size_t k = std::strlen(names[i]); k < 12; k++) std::cout << ' ';
        std::cout << "  ";
        if (r.score[i] > 0) std::cout << r.score[i];
        else std::cout << "-";
        std::cout << "      " << maxes[i] << "\n";
    }
    int sum = 0, maxsum = 0;
    for (int i = 0; i < 4; i++) { sum += r.score[i]; maxsum += maxes[i]; }
    std::cout << "\nВсего рекордов: " << sum << "/" << maxsum << "\n";
    std::cout << "Попыток за сессию: " << s.totalAttempts << "\n";
    if (s.pvpGames > 0) std::cout << "Сетевых игр: " << s.pvpGames << "\n";

    std::cout << "\n=== Достижения ===\n";
    auto ach = [](bool ok, const std::string& label) {
        std::cout << "  [" << (ok ? "X" : " ") << "] " << label << "\n";
    };
    ach(s.anyAttempt,       "First Blood — первая попытка");
    ach(s.anyPerfect,       "Perfectionist — идеальное попадание");
    ach(s.anySpeedrun,      "Speedrun — идеал с первой попытки");
    ach(s.usedAutoAnywhere, "Autoassisted — использован auto");
    ach(s.anyPerfect && !s.usedAutoAnywhere, "Pure Master — идеал без auto");
    ach(s.minimalist,       "Minimalist — идеал ≤ 12 символов");
    ach(s.bitwisePerfect,   "Bitwise Wizard — идеал с битовыми операциями");
    ach(s.explorer,         "Explorer — 5+ разных выражений");
    ach(s.pveChampion,      "PvE Champion — победа в PvE турнире");
    ach(s.pvpGames > 0,     "Network Duelist — первая сетевая игра");
    ach(s.pvpChampion,      "PvP Champion — победа в сетевой игре");
}

// =============================================================
//  Тренировка
// =============================================================
static int playLevel(int idx, const std::string& name, int n,
                     std::mt19937& rng, Records& rec, Session& sess) {
    std::cout << "\n==== Уровень: " << name << " (" << n << " точек) ====\n\n";
    auto pts = generatePoints(n, rng);
    std::cout << "Целевые точки:\n";
    for (size_t i = 0; i < pts.size(); ++i)
        std::cout << "  P" << (i+1) << " = (" << pts[i].x << ", " << pts[i].y << ")\n";
    std::cout << "\nВведи f(x), 'help' — справка, 'back' — в меню, 'end' — итоги.\n\n";

    std::vector<Attempt> history;
    int bestHits = -1, bestLen = 0;
    std::string bestExpr;
    int attempts = 0;
    bool usedAutoLevel = false;
    std::set<std::string> distinctExprs;

    auto commitRecord = [&]() {
        if (bestHits > rec.score[idx]) {
            rec.score[idx] = bestHits; saveRecords(rec);
            std::cout << "Новый рекорд сохранён (" << bestHits << "/" << n << ")!\n";
        }
    };

    while (true) {
        std::cout << "f(x) = "; std::cout.flush();
        std::string line;
        if (!std::getline(std::cin, line)) { commitRecord(); return 1; }
        std::string cmd = trimLower(line);

        if (cmd == "exit" || cmd == "quit") { commitRecord(); return 1; }
        if (cmd == "end")                   { commitRecord(); return 2; }
        if (cmd == "back" || cmd == "menu") { commitRecord(); return 0; }
        if (cmd == "help") { printHelp(); continue; }
        if (cmd == "hint") { printHint(n, attempts); continue; }

        if (cmd == "history") {
            if (history.empty()) { std::cout << "(попыток не было)\n"; continue; }
            for (size_t i = 0; i < history.size(); ++i) {
                auto& a = history[i];
                std::cout << "  " << (i+1) << ". f(x) = " << a.expr << " -> "
                          << a.hits << "/" << n << " (" << complexityRating(a.len) << ")\n";
            }
            continue;
        }
        if (cmd == "undo") {
            if (history.empty()) { std::cout << "(нечего отменять)\n"; continue; }
            history.pop_back();
            bestHits = -1; bestExpr = ""; bestLen = 0;
            for (auto& a : history)
                if (a.hits > bestHits || (a.hits == bestHits && a.len < bestLen)) {
                    bestHits = a.hits; bestExpr = a.expr; bestLen = a.len;
                }
            std::cout << "Отменено.\n";
            continue;
        }

        bool fromAuto = false;
        if (cmd == "auto") {
            if (usedAutoLevel) { std::cout << "auto уже использован.\n"; continue; }
            usedAutoLevel = sess.usedAutoAnywhere = true;
            std::string poly = makeLagrangeExpr(pts);
            if (poly.empty()) { std::cout << "Полином слишком сложный.\n"; continue; }
            std::cout << "auto -> f(x) = " << poly << "\n";
            line = poly; fromAuto = true;
        } else if (cmd.rfind("poly ", 0) == 0) {
            std::istringstream iss(cmd.substr(5));
            int K = 0;
            if (!(iss >> K) || K < 2 || K > n) {
                std::cout << "Использование: poly K, 2 <= K <= " << n << "\n";
                continue;
            }
            std::vector<Point> sub(pts.begin(), pts.begin() + K);
            std::string poly = makeLagrangeExpr(sub);
            if (poly.empty()) { std::cout << "Полином слишком сложный.\n"; continue; }
            std::cout << "poly " << K << " -> f(x) = " << poly << "\n";
            line = poly;
        } else if (cmd.empty()) continue;

        std::string expr = preprocess(line);
        try { ExprParser t(expr, 0.0); t.parse(); }
        catch (const std::exception& e) { std::cout << "Ошибка: " << e.what() << "\n"; continue; }

        attempts++; sess.totalAttempts++; sess.anyAttempt = true;
        std::string stripped = stripSpaces(expr);
        distinctExprs.insert(stripped);
        if (distinctExprs.size() >= 5) sess.explorer = true;

        int hc = 0;
        std::cout << "\n";
        for (size_t i = 0; i < pts.size(); ++i) {
            double y;
            try { ExprParser p(expr, pts[i].x); y = p.parse(); }
            catch (const std::exception& e) { std::cout << "  [ERR]  " << e.what() << "\n"; continue; }
            double tol = 1e-6 * (1.0 + std::fabs(pts[i].y));
            if (std::isfinite(y) && std::fabs(y - pts[i].y) < tol) {
                hc++;
                std::cout << "  [OK]   f(" << pts[i].x << ") = " << y << " -> " << pts[i].y << "\n";
            } else {
                std::cout << "  [MISS] f(" << pts[i].x << ") = " << y << " (нужно " << pts[i].y << ")\n";
            }
        }
        int exprLen = (int)stripped.size();
        std::string rating = complexityRating(exprLen);
        std::cout << "\nПопаданий: " << hc << "/" << n
                  << "   длина: " << exprLen << "   рейтинг: " << rating;
        if (fromAuto) std::cout << "  [auto]";
        std::cout << "\n\n";

        history.push_back({expr, hc, exprLen});
        if (hc > bestHits || (hc == bestHits && exprLen < bestLen)) {
            bestHits = hc; bestExpr = expr; bestLen = exprLen;
            std::cout << "* Новый лучший: " << bestHits << "/" << n << "\n";
        }
        if (hc == n) {
            sess.anyPerfect = true;
            if (attempts == 1) sess.anySpeedrun = true;
            if (exprLen <= 12) sess.minimalist = true;
            if (hasBitwise(expr)) sess.bitwisePerfect = true;
            std::cout << "* ИДЕАЛЬНО! " << n << "/" << n << " (рейтинг " << rating << ")\n";
            commitRecord();
            std::cout << "\nНажми Enter...";
            std::string tmp; std::getline(std::cin, tmp);
            return 0;
        }
        std::cout << "Лучшее: " << bestHits << "/" << n << " (f(x) = " << bestExpr << ")\n";
        std::cout << "Enter — ещё, 'auto', 'undo', 'hint', 'back', 'end'\n\n";
    }
}

// =============================================================
//  PvE
// =============================================================
static std::string computerPickFunction(const std::vector<Point>& pts, std::mt19937& rng) {
    int n = (int)pts.size();
    int fit;
    switch (n) { case 2: fit=1; break; case 3: fit=1; break; case 4: fit=2; break; case 6: fit=3; break; default: fit = std::max(1, n/2); }
    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::shuffle(idx.begin(), idx.end(), rng);
    idx.resize(fit);
    std::sort(idx.begin(), idx.end());
    std::vector<Point> sub;
    for (int i : idx) sub.push_back(pts[i]);
    std::string expr = makeLagrangeExpr(sub);
    return expr.empty() ? "0" : expr;
}

static int playPvELevel(int, const std::string& name, int n, std::mt19937& rng) {
    std::cout << "\n==== PvE: " << name << " (" << n << " точек) ====\n\n";
    auto pts = generatePoints(n, rng);
    std::cout << "Общие точки:\n";
    for (auto& p : pts) std::cout << "  (" << p.x << ", " << p.y << ")\n";

    std::string compExpr = computerPickFunction(pts, rng);
    int compHits = evaluateHits(compExpr, pts);

    std::cout << "\nКомпьютер сделал ход. Результат: " << compHits << "/" << n
              << "  (функция скрыта)\n";
    std::cout << "Цель: >" << compHits << " для победы, =" << compHits << " для ничьей.\n";
    std::cout << "auto/poly в PvE недоступны. 'end' — завершить игру.\n\n";

    int bestHits = 0;
    std::string bestExpr;
    bool userWon = false;
    while (true) {
        std::cout << "f(x) = "; std::cout.flush();
        std::string line;
        if (!std::getline(std::cin, line)) return -2;
        std::string cmd = trimLower(line);
        if (cmd == "quit" || cmd == "exit") return -2;
        if (cmd == "end")                   return -3;
        if (cmd == "giveup" || cmd == "accept" || cmd == "back") break;
        if (cmd == "help") { printHelp(); continue; }
        if (cmd.empty()) continue;
        if (cmd == "auto" || cmd == "poly" || cmd.rfind("poly ", 0) == 0) {
            std::cout << "auto/poly в PvE недоступны.\n";
            continue;
        }
        std::string expr = preprocess(line);
        try { ExprParser t(expr, 0.0); t.parse(); }
        catch (const std::exception& e) { std::cout << "Ошибка: " << e.what() << "\n"; continue; }
        int hits = 0;
        std::cout << "\n";
        for (auto& p : pts) {
            try {
                ExprParser par(expr, p.x);
                double y = par.parse();
                double tol = 1e-6 * (1.0 + std::fabs(p.y));
                if (std::isfinite(y) && std::fabs(y - p.y) < tol) {
                    hits++;
                    std::cout << "  [OK]   f(" << p.x << ") = " << y << " -> " << p.y << "\n";
                } else {
                    std::cout << "  [MISS] f(" << p.x << ") = " << y << " (нужно " << p.y << ")\n";
                }
            } catch (const std::exception& e) { std::cout << "  [ERR]  " << e.what() << "\n"; }
        }
        if (hits > bestHits) { bestHits = hits; bestExpr = expr; }
        std::cout << "\nПопаданий: " << hits << "/" << n << "\n";
        if (hits > compHits) { userWon = true; break; }
        std::cout << "Ещё попытка, 'giveup' или 'end':\n\n";
    }

    std::cout << "\nКомпьютер: f(x) = " << compExpr << " (" << compHits << "/" << n << ")\n";
    if (!bestExpr.empty())
        std::cout << "Твой лучший: f(x) = " << bestExpr << " (" << bestHits << "/" << n << ")\n";

    int result;
    if (userWon || bestHits > compHits) { std::cout << ">>> ПОБЕДА! <<<\n"; result = 1; }
    else if (bestHits == compHits)      { std::cout << ">>> НИЧЬЯ <<<\n"; result = 0; }
    else                                { std::cout << ">>> ПОРАЖЕНИЕ <<<\n"; result = -1; }
    return result;
}

// =============================================================
//  СЕТЕВАЯ ИГРА
// =============================================================
#ifndef DISABLE_NETWORK
static void playNetworkGame(NetConn& conn, bool is_host, const std::vector<Point>& pts) {
    int n = (int)pts.size();
    const char* my_name    = is_host ? "host"   : "client";
    const char* their_name = is_host ? "client" : "host";

    int my_best = 0, their_best = 0;
    bool my_passed = false, their_passed = false;
    bool my_turn = is_host;
    bool game_over = false;

    while (!game_over) {
        if (my_turn) {
            if (my_passed) { if (!conn.sendLine("PASS")) { std::cout << "Связь потеряна.\n"; return; } }
            else {
                bool got = false;
                while (!got) {
                    std::cout << "f(x) (или 'done', 'quit'): "; std::cout.flush();
                    std::string expr;
                    if (!std::getline(std::cin, expr)) { conn.sendLine("QUIT"); return; }
                    std::string cmd = trimLower(expr);
                    if (cmd == "done" || cmd == "pass") { my_passed = true; if (!conn.sendLine("PASS")) return; got = true; }
                    else if (cmd == "quit" || cmd == "exit" || cmd == "end") { conn.sendLine("QUIT"); return; }
                    else if (cmd == "help") { printHelp(); }
                    else if (cmd.empty()) { }
                    else {
                        std::string code = preprocess(expr);
                        try { ExprParser t(code, 0.0); t.parse(); }
                        catch (const std::exception& e) { std::cout << "Ошибка: " << e.what() << "\n"; continue; }
                        int hc = evaluateHits(code, pts);
                        std::cout << "Вы: f(x) = " << code << " -> " << hc << "/" << n << "\n";
                        if (hc > my_best) my_best = hc;
                        if (!conn.sendLine("ATTEMPT " + std::to_string(hc) + " " + code)) return;
                        got = true;
                        if (hc == n) { std::cout << "* ИДЕАЛЬНО! Победа!\n"; return; }
                    }
                }
            }
            my_turn = false;
        } else {
            std::string line;
            if (!conn.recvLine(line)) { std::cout << "Соперник отключился.\n"; return; }
            if (line.rfind("ATTEMPT ", 0) == 0) {
                size_t sp = line.find(' ', 8);
                if (sp == std::string::npos) return;
                int sent_hits = 0;
                try { sent_hits = std::stoi(line.substr(8, sp - 8)); } catch (...) { return; }
                std::string expr = line.substr(sp + 1);
                int local_hits = evaluateHits(expr, pts);
                int hits = local_hits;
                if (sent_hits != local_hits)
                    std::cout << "  (расхождение: пир " << sent_hits << ", локально " << local_hits << ")\n";
                std::cout << "Соперник: f(x) = " << expr << " -> " << hits << "/" << n << "\n";
                if (hits > their_best) their_best = hits;
                if (hits == n) { std::cout << "* Соперник победил!\n"; return; }
            } else if (line == "PASS") {
                if (!their_passed) std::cout << "Соперник сказал 'done'.\n";
                their_passed = true;
            } else if (line.rfind("GAMEOVER ", 0) == 0) {
                std::string who = line.substr(9);
                if (who == my_name) std::cout << "Соперник признал вашу победу!\n";
                else if (who == their_name) std::cout << "Соперник празднует победу.\n";
                else std::cout << "Объявлена ничья.\n";
                return;
            } else if (line == "QUIT") { std::cout << "Соперник покинул игру.\n"; return; }
            my_turn = true;
            if (my_passed && their_passed) game_over = true;
        }
    }

    std::cout << "\n=== Итог сетевой игры ===\n";
    std::cout << "Вы: " << my_best << "/" << n << "   Соперник: " << their_best << "/" << n << "\n";
    if (my_best > their_best) { std::cout << ">>> ПОБЕДА! <<<\n"; conn.sendLine(std::string("GAMEOVER ") + my_name); }
    else if (their_best > my_best) { std::cout << ">>> ПОРАЖЕНИЕ <<<\n"; conn.sendLine(std::string("GAMEOVER ") + their_name); }
    else { std::cout << ">>> НИЧЬЯ <<<\n"; conn.sendLine("GAMEOVER tie"); }
}

static void hostRoom(std::mt19937& rng, Session& sess) {
    if (!netInit()) { std::cout << "Ошибка сети.\n"; return; }
    std::cout << "\n--- Создание комнаты ---\n";
    std::cout << "Порт (Enter = 5555): "; std::cout.flush();
    std::string portStr; std::getline(std::cin, portStr);
    int port = 5555;
    if (!portStr.empty()) {
        try { port = std::stoi(portStr); } catch (...) { std::cout << "Неверный порт.\n"; return; }
    }
    if (port < 1 || port > 65535) { std::cout << "Порт вне диапазона.\n"; return; }

    sock_t ls = sys_socket(AF_INET, SOCK_STREAM, 0);
    if (ls == INVALID_SOCK) { std::cout << "Ошибка сокета.\n"; return; }
    int yes = 1;
    sys_setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, (int)sizeof(yes));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = sys_htons((unsigned short)port);
    if (sys_bind(ls, (struct sockaddr*)&addr, (int)sizeof(addr)) != 0) {
        std::cout << "Не удалось привязаться к порту " << port << ".\n";
        closeSock(ls); return;
    }
    if (sys_listen(ls, 1) != 0) { std::cout << "Ошибка listen.\n"; closeSock(ls); return; }

    std::string ip = getLocalIp();
    std::cout << "\nВаш IP: " << (ip.empty() ? "(неизвестен)" : ip) << "\n";
    std::cout << "Порт: " << port << "\n";
    std::cout << "Ожидание клиента... " << std::flush;

    sock_t cs = INVALID_SOCK;
    while (true) {
        fd_set fds; FD_ZERO(&fds); FD_SET(ls, &fds);
        struct timeval tv; tv.tv_sec = 5; tv.tv_usec = 0;
        int r = sys_select((int)ls + 1, &fds, nullptr, nullptr, &tv);
        if (r > 0) { cs = sys_accept(ls, nullptr, nullptr); break; }
        else if (r == 0) std::cout << "." << std::flush;
        else { std::cout << "\nОшибка select.\n"; closeSock(ls); return; }
    }
    closeSock(ls);
    std::cout << "\nКлиент подключился!\n";

    NetConn conn(cs);
    auto pts = generatePoints(4, rng);
    std::cout << "\nОбщие точки (4):\n";
    for (auto& p : pts) std::cout << "  (" << p.x << ", " << p.y << ")\n";
    if (!sendPoints(conn, pts)) { std::cout << "Ошибка отправки.\n"; return; }

    std::cout << "\nИгра началась. Хост ходит первым.\nВведите f(x) или 'done'. 'quit' — выход.\n\n";
    playNetworkGame(conn, true, pts);
    sess.pvpGames++;
}

static void joinRoom(Session& sess) {
    if (!netInit()) { std::cout << "Ошибка сети.\n"; return; }
    std::cout << "\n--- Подключение ---\n";
    std::cout << "IP хоста: "; std::cout.flush();
    std::string ip; if (!std::getline(std::cin, ip) || ip.empty()) return;
    std::cout << "Порт (Enter = 5555): "; std::cout.flush();
    std::string portStr; std::getline(std::cin, portStr);
    int port = 5555;
    if (!portStr.empty()) {
        try { port = std::stoi(portStr); } catch (...) { std::cout << "Неверный порт.\n"; return; }
    }

    sock_t s = sys_socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCK) { std::cout << "Ошибка сокета.\n"; return; }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = sys_htons((unsigned short)port);
    if (sys_inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        std::cout << "Неверный IP.\n"; closeSock(s); return;
    }
    std::cout << "Подключение к " << ip << ":" << port << "... " << std::flush;
    if (sys_connect(s, (struct sockaddr*)&addr, (int)sizeof(addr)) != 0) {
        std::cout << "не удалось.\n"; closeSock(s); return;
    }
    std::cout << "успешно!\n";

    NetConn conn(s);
    std::vector<Point> pts;
    if (!recvPoints(conn, pts)) { std::cout << "Ошибка получения.\n"; return; }
    std::cout << "\nОбщие точки (" << pts.size() << "):\n";
    for (auto& p : pts) std::cout << "  (" << p.x << ", " << p.y << ")\n";

    std::cout << "\nИгра началась. Хост ходит первым.\nВведите f(x) или 'done'. 'quit' — выход.\n\n";
    playNetworkGame(conn, false, pts);
    sess.pvpGames++;
}

static void networkMenu(std::mt19937& rng, Session& sess) {
    while (true) {
        std::cout << "\n=== СЕТЕВАЯ ИГРА (PvP) ===\n";
        std::cout << "  1. Создать комнату (хост)\n";
        std::cout << "  2. Подключиться (клиент)\n";
        std::cout << "  0. Назад\n";
        std::cout << "> "; std::cout.flush();
        std::string line;
        if (!std::getline(std::cin, line)) return;
        std::string cmd = trimLower(line);
        if (cmd.empty()) continue;
        if (cmd == "0" || cmd == "back" || cmd == "menu") return;
        if (cmd == "1") {
            hostRoom(rng, sess);
            std::cout << "Enter...\n"; std::string tmp; std::getline(std::cin, tmp);
            continue;
        }
        if (cmd == "2") {
            joinRoom(sess);
            std::cout << "Enter...\n"; std::string tmp; std::getline(std::cin, tmp);
            continue;
        }
        std::cout << "Неизвестная команда.\n";
    }
}
#else
static void netCleanup() {}
static void networkMenu(std::mt19937&, Session&) {
    std::cout << "\nСетевая игра отключена при сборке (DISABLE_NETWORK).\n";
}
#endif

// =============================================================
//  Selftest + диагностика кодировки
// =============================================================
static int runSelftest() {
    std::cout << "=== SELFTEST ===\n";
    int fails = 0;
    auto check = [&](const std::string& expr, double x, double expected, double tol = 1e-6) {
        try {
            ExprParser p(expr, x);
            double got = p.parse();
            bool ok = std::fabs(got - expected) < tol;
            std::cout << "  " << (ok ? "[OK]  " : "[FAIL]") << " " << expr
                      << " (x=" << x << ") = " << got;
            if (!ok) { std::cout << " (ожидалось " << expected << ")"; fails++; }
            std::cout << "\n";
        } catch (const std::exception& e) {
            std::cout << "  [FAIL] " << expr << " -> " << e.what() << "\n";
            fails++;
        }
    };
    check("2 + 3", 0, 5);
    check("x ** 2", 4, 16);
    check("sqrt(16)", 0, 4);
    check("|x - 5|", 2, 3);
    check("5!", 0, 120);
    check("sin(pi/2)", 0, 1);
    check("erf(0)", 0, 0);
    check("x & 3", 5, 1);
    check("0.5*x", 4, 2);          // ← НОВЫЙ тест дробного числа
    check("2.75 + 1.25", 0, 4);    // ← ещё один
    check("1e-2", 0, 0.01);        // ← научная нотация
    std::cout << "\n";
    if (fails == 0) std::cout << "ВСЁ РАБОТАЕТ. Среда в порядке.\n";
    else            std::cout << "ОШИБОК: " << fails << "\n";
    return fails == 0 ? 0 : 1;
}

static int runEncodingCheck() {
    std::cout << "=== ENCODING CHECK ===\n";
    const std::string test = "Привет";
    std::cout << "Bytes of \"Привет\":\n  ";
    for (char cc : test) {
        unsigned char c = (unsigned char)cc;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02X ", c);
        std::cout << buf;
    }
    std::cout << "\n\n";
    if (test.size() == 12) {
        std::cout << "=> Файл в UTF-8.\n";
    } else if (test.size() == 6) {
        std::cout << "=> Файл в CP1251.\n";
        std::cout << "   Пересохраните: VS Code -> правый нижний угол -> Save with Encoding -> UTF-8.\n";
    } else {
        std::cout << "=> Неизвестная кодировка (" << test.size() << " байт).\n";
    }
    return 0;
}

// =============================================================
//  main
// =============================================================
int main(int argc, char** argv) {
    std::cout.setf(std::ios::unitbuf);

#ifdef _WIN32
    Utf8CoutBuf utf8cout(std::cout, STD_OUTPUT_HANDLE);
    Utf8CoutBuf utf8cerr(std::cerr, STD_ERROR_HANDLE);
#endif

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--selftest")        return runSelftest();
        if (a == "--check-encoding")  return runEncodingCheck();
        if (a == "--help" || a == "-h") {
            std::cout << "Функция-детектив\n"
                      << "  --selftest         проверить парсер\n"
                      << "  --check-encoding   определить кодировку исходника\n"
                      << "  --no-color         отключить цвета\n";
            return 0;
        }
    }

#ifdef _WIN32
    bool colorOK = enableAnsi();
#else
    bool colorOK = isatty(STDOUT_FILENO) != 0;
#endif
    if (std::getenv("NO_COLOR")) colorOK = false;
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--no-color") colorOK = false;
    Col::init(colorOK);

    // ВАЖНО: локаль для чисел — C (точка), чтобы std::snprintf/sscanf в сети
    // и любые другие места использовали точку как десятичный разделитель,
    // а не запятую из русской локали.
    std::setlocale(LC_ALL, "");
    std::setlocale(LC_NUMERIC, "C");

    std::cout << "===============================================================\n";
    std::cout << "                  ФУНКЦИЯ-ДЕТЕКТИВ  v8.7\n";
    std::cout << "===============================================================\n\n";

    try {
        std::random_device rd;
        std::mt19937 rng(rd());
        Records rec; loadRecords(rec);
        Session sess;

        const std::vector<std::pair<std::string, int>> levels = {
            {"Лёгкий", 2}, {"Средний", 3}, {"Сложный", 4}, {"Безумный", 6}
        };

        while (true) {
            std::cout << "\n";
            std::cout << "Выбери режим:\n\n";
            for (int i = 0; i < 4; i++) {
                std::cout << "  " << (i + 1) << ". Тренировка — " << levels[i].first
                          << " (" << levels[i].second << " точек)";
                if (rec.score[i] > 0)
                    std::cout << "   рекорд: " << rec.score[i] << "/" << levels[i].second;
                std::cout << "\n";
            }
            std::cout << "  5. PvE — против компьютера\n";
            std::cout << "  6. Статистика и достижения\n";
            std::cout << "  7. Сетевая игра — PvP\n";
            std::cout << "  h. Справка\n";
            std::cout << "  end — итоги и выход\n";
            std::cout << "  0. Выход\n\n";
            std::cout << "> "; std::cout.flush();

            std::string line;
            if (!std::getline(std::cin, line)) { netCleanup(); return 0; }
            std::string cmd = trimLower(line);
            if (cmd.empty()) continue;
            if (cmd == "0" || cmd == "exit" || cmd == "quit") { std::cout << "До свидания!\n"; netCleanup(); return 0; }
            if (cmd == "end") { showStats(sess, rec); std::cout << "Спасибо за игру!\n"; netCleanup(); return 0; }
            if (cmd == "h" || cmd == "help") { printHelp(); continue; }
            if (cmd == "6") {
                showStats(sess, rec);
                std::cout << "\nНажми Enter..."; std::string tmp; std::getline(std::cin, tmp);
                continue;
            }
            if (cmd == "7") { networkMenu(rng, sess); continue; }

            if (cmd == "5") {
                std::cout << "\n====== PvE ТУРНИР ======\nНажми Enter для старта...";
                std::string tmp; std::getline(std::cin, tmp);
                int wins = 0, ties = 0, losses = 0;
                bool pveEnd = false;
                for (int i = 0; i < 4; i++) {
                    int r = playPvELevel(i, levels[i].first, levels[i].second, rng);
                    if (r == -3) { pveEnd = true; break; }
                    if (r == -2) { std::cout << "Выход из турнира.\n"; break; }
                    if (r > 0) wins++; else if (r == 0) ties++; else losses++;
                    if (i < 3) { std::cout << "\nEnter..."; std::getline(std::cin, tmp); }
                }
                if (pveEnd) { showStats(sess, rec); std::cout << "Спасибо за игру!\n"; netCleanup(); return 0; }
                if (wins + ties + losses > 0) {
                    std::cout << "\n=== ИТОГ PvE ===\n"
                              << "Побед: " << wins << "   Ничьих: " << ties << "   Поражений: " << losses << "\n";
                    if (wins > losses && wins + ties + losses == 4) { std::cout << "Ты победил!\n"; sess.pveChampion = true; }
                    else if (wins < losses) std::cout << "Компьютер победил.\n";
                    else std::cout << "Ничья.\n";
                    std::cout << "\nEnter..."; std::getline(std::cin, tmp);
                }
                continue;
            }

            if (cmd.size() == 1 && cmd[0] >= '1' && cmd[0] <= '4') {
                int idx = cmd[0] - '1';
                int r = playLevel(idx, levels[idx].first, levels[idx].second, rng, rec, sess);
                if (r == 1) { netCleanup(); return 0; }
                if (r == 2) { showStats(sess, rec); std::cout << "Спасибо за игру!\n"; netCleanup(); return 0; }
                continue;
            }
            std::cout << "Неизвестная команда. Введи 0–7, h или end.\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL] Исключение: " << e.what() << "\n";
        netCleanup();
        return 1;
    } catch (...) {
        std::cerr << "\n[FATAL] Неизвестное исключение.\n";
        netCleanup();
        return 1;
    }
}
