// g++ -std=c++17 -O2 game.cpp -o game
// Linux/macOS: ./game
// Windows (MinGW): g++ -std=c++17 -O2 game.cpp -o game.exe -lws2_32
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <random>
#include <numeric>
#include <cctype>
#include <stdexcept>
#include <limits>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <set>
#include <utility>

// ============ Сетевые заголовки ============
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET sock_t;
    static const sock_t INVALID_SOCK = INVALID_SOCKET;
    static inline void closeSock(sock_t s) { closesocket(s); }
#else
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    typedef int sock_t;
    static const sock_t INVALID_SOCK = -1;
    static inline void closeSock(sock_t s) { ::close(s); }
#endif

#ifdef _WIN32
#include <windows.h>
static void enableAnsi() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0;
    if (GetConsoleMode(h, &m)) SetConsoleMode(h, m | 0x0004);
}
#else
static void enableAnsi() {}
#endif

namespace Col {
    constexpr const char* RST="\033[0m", *RED="\033[31m", *GRN="\033[32m",
                        *YEL="\033[33m", *CYA="\033[36m", *MAG="\033[35m",
                        *BOLD="\033[1m", *DIM="\033[2m";
}

// ============ Точные дроби + Лагранж ============
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

// ============ Популярные матфункции ============
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
    long long n = (long long)n_; bool neg = n < 0; if (neg) n = -n;
    double a = 0, b = 1;
    for (long long i = 0; i < n; ++i) { double c = a + b; a = b; b = c; }
    return (neg && (n % 2 == 0)) ? -a : a;
}
static bool is_prime_ll(long long n) {
    if (n < 2) return false; if (n < 4) return true;
    if (n % 2 == 0) return false;
    for (long long i = 3; i * i <= n; i += 2) if (n % i == 0) return false;
    return true;
}
static double next_prime_d(double v) {
    long long n = (long long)std::floor(v) + 1; if (n < 2) n = 2;
    while (!is_prime_ll(n)) ++n;
    return (double)n;
}
static double digit_sum_d(double v) {
    long long n = (long long)std::fabs(v), s = 0;
    while (n > 0) { s += n % 10; n /= 10; }
    return (double)s;
}
static double reverse_digits_d(double v) {
    bool neg = v < 0; long long n = (long long)std::fabs(v), r = 0;
    while (n > 0) { r = r * 10 + n % 10; n /= 10; }
    return neg ? -(double)r : (double)r;
}
static double count_digits_d(double v) {
    long long n = (long long)std::fabs(v);
    if (n == 0) return 1.0;
    int c = 0; while (n > 0) { ++c; n /= 10; }
    return (double)c;
}

// ============ Парсер ============
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
            if (pk() == '|') {
                ++pos; double r = parseBitXor();
                l = (double)((long long)l | (long long)r);
            } else break; }
        return l;
    }
    double parseBitXor() {
        double l = parseBitAnd();
        while (true) { skip();
            if (pk() == '^') {
                ++pos; double r = parseBitAnd();
                l = (double)((long long)l ^ (long long)r);
            } else break; }
        return l;
    }
    double parseBitAnd() {
        double l = parseShift();
        while (true) { skip();
            if (pk() == '&') {
                ++pos; double r = parseShift();
                l = (double)((long long)l & (long long)r);
            } else break; }
        return l;
    }
    double parseShift() {
        double l = parseAdd();
        while (true) { skip();
            if (consume("<<"))      l = sh((long long)l, (long long)parseAdd(), true);
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
            if (c == '*') {
                if (pk(1) == '*') break;
                ++pos; l *= parseUnary();
            } else if (c == '/') {
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
        double b = parsePostfix(); skip();
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
            if (pk() != '|')
                throw std::runtime_error("Ожидалось закрывающее '|' (позиция "
                                         + std::to_string(pos) + ")");
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
        throw std::runtime_error(std::string("Неожиданный символ '") + pk() +
                                 "' в позиции " + std::to_string(pos));
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
        try { return std::stod(s); }
        catch (...) { throw std::runtime_error("Плохое число: " + s); }
    }
    double parseIdent() {
        size_t st = pos;
        while (pos < src.size() &&
               (std::isalnum((unsigned char)src[pos]) || src[pos] == '_')) ++pos;
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
            if (n != k) throw std::runtime_error("'" + id + "' требует " +
                std::to_string(k) + " арг., дано " + std::to_string(n));
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

// ============ Утилиты ============
static std::string trimLower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}
static std::string stripSpaces(const std::string& s) {
    std::string r; for (char c : s) if (!std::isspace((unsigned char)c)) r += c;
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

// ============ СЕТЕВОЙ СЛОЙ ============
static bool netInit() {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}
static void netCleanup() {
#ifdef _WIN32
    WSACleanup();
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
    void disconnect() {
        if (sock != INVALID_SOCK) { closeSock(sock); sock = INVALID_SOCK; }
    }
    bool sendLine(const std::string& line) {
        if (!valid()) return false;
        std::string data = line + "\n";
        size_t total = 0;
        while (total < data.size()) {
            int sent = (int)::send(sock, data.data() + total,
                                   (int)(data.size() - total), 0);
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
            int r = (int)::recv(sock, &c, 1, 0);
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
    if (gethostname(hostname, sizeof(hostname)) != 0) return "";
    struct hostent* he = gethostbyname(hostname);
    if (!he || !he->h_addr_list[0]) return "";
    struct in_addr a;
    std::memcpy(&a, he->h_addr_list[0], sizeof(a));
    return std::string(inet_ntoa(a));
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

// ============ Справка ============
static void printHelp() {
    std::cout << "-------------------------------------------------------------\n";
    std::cout << Col::BOLD << "Переменная:" << Col::RST << " x   "
              << Col::BOLD << "Константы:" << Col::RST
              << " pi, e, tau, phi, eps, inf\n";
    std::cout << Col::BOLD << "Арифметика:" << Col::RST << " + - * / % //   "
              << Col::BOLD << "Степень:" << Col::RST << " **\n";
    std::cout << Col::BOLD << "Факториал:" << Col::RST << " n!  или  fact(n)\n";
    std::cout << Col::BOLD << "Модуль:" << Col::RST << " |x|   или   abs(x)\n";
    std::cout << Col::BOLD << "Битовые:" << Col::RST << " & | ^ << >> ~\n";
    std::cout << Col::DIM << "Сравнения и логические операторы ОТКЛЮЧЕНЫ.\n"
              << "Внутри |...| знак '|' закрывает модуль; для битового ИЛИ — |(a | b)|.\n"
              << Col::RST;
    std::cout << Col::BOLD << "\n--- Тригонометрия ---" << Col::RST << "\n";
    std::cout << "  sin cos tan sec csc cot\n";
    std::cout << "  asin acos atan atan2(y,x)\n";
    std::cout << "  sinh cosh tanh sech csch coth asinh acosh atanh\n";
    std::cout << "  deg rad   erf erfc\n";
    std::cout << Col::BOLD << "\n--- Популярные операции ---" << Col::RST << "\n";
    std::cout << "  fact(n) n!   C(n,k)   P(n,k)   fib(n)\n";
    std::cout << "  isprime(n) nextprime(n) is_even is_odd\n";
    std::cout << "  digit_sum reverse count_digits triangular\n";
    std::cout << "  gcd lcm lerp   sqrt cbrt abs exp expm1 ln log log2 log10\n";
    std::cout << "  floor ceil round trunc fract sign gamma lgamma\n";
    std::cout << Col::BOLD << "\n--- Команды в игре ---" << Col::RST << "\n";
    std::cout << "  auto / poly K   (только в тренировке)\n";
    std::cout << "  history · undo · hint · help · back\n";
    std::cout << "  end             — завершить игру с показом итогов\n";
    std::cout << "  exit            — выйти без показа итогов\n";
    std::cout << "-------------------------------------------------------------\n";
}
static void printHint(int n, int attempts) {
    std::cout << Col::CYA << "Подсказка: " << Col::RST;
    if      (n == 2) std::cout << "через 2 точки проходит прямая.\n";
    else if (n == 3) std::cout << "через 3 точки — парабола a*x**2+b*x+c.\n";
    else if (n == 4) std::cout << "через 4 точки — кубическая. Или sin/cos/abs.\n";
    else             std::cout << "многочлен степени " << (n-1) << " проходит через все точки.\n";
    if (attempts >= 3)
        std::cout << Col::DIM << "(Попробуй 'auto' или 'poly K'.)\n" << Col::RST;
}

// ============ Рекорды / сессия ============
struct Records { int score[4] = {0,0,0,0}; };
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
    int pvpWins = 0, pvpLosses = 0, pvpTies = 0, pvpGames = 0;
    bool pvpChampion = false;
};
struct Attempt { std::string expr; int hits; int len; };

// ============ Статистика ============
static void showStats(const Session& s, const Records& r) {
    std::cout << "\n" << Col::BOLD << Col::CYA
              << "========== ИТОГИ ==========\n" << Col::RST << "\n";
    const char* names[4] = {"Лёгкий", "Средний", "Сложный", "Безумный"};
    const int maxes[4] = {2, 3, 4, 6};
    std::cout << "Уровень     Рекорд  Максимум\n";
    for (int i = 0; i < 4; i++) {
        std::cout << names[i];
        for (size_t k = std::strlen(names[i]); k < 12; k++) std::cout << ' ';
        std::cout << "  ";
        if (r.score[i] > 0) std::cout << Col::YEL << r.score[i] << Col::RST;
        else std::cout << "-";
        std::cout << "      " << maxes[i] << "\n";
    }
    int sum = 0, maxsum = 0;
    for (int i = 0; i < 4; i++) { sum += r.score[i]; maxsum += maxes[i]; }
    std::cout << "\nВсего рекордов: " << Col::YEL << sum << "/" << maxsum << Col::RST << "\n";
    std::cout << "Попыток за сессию: " << s.totalAttempts << "\n";
    if (s.pvpGames > 0) {
        std::cout << "Сетевые игры: " << s.pvpGames << "   Победы: " << Col::GRN
                  << s.pvpWins << Col::RST << "   Ничьи: " << Col::YEL
                  << s.pvpTies << Col::RST << "   Поражения: " << Col::RED
                  << s.pvpLosses << Col::RST << "\n";
    }

    std::cout << "\n" << Col::BOLD << "=== Достижения ===\n" << Col::RST;
    auto ach = [](bool ok, const std::string& label, const std::string& desc) {
        std::cout << "  [";
        if (ok) std::cout << Col::GRN << "X" << Col::RST;
        else    std::cout << " ";
        std::cout << "] " << Col::BOLD << label << Col::RST;
        if (!desc.empty()) std::cout << " — " << Col::DIM << desc << Col::RST;
        std::cout << "\n";
    };
    ach(s.anyAttempt,       "First Blood",   "первая попытка");
    ach(s.anyPerfect,       "Perfectionist", "идеальное попадание");
    ach(s.anySpeedrun,      "Speedrun",      "идеал с первой попытки");
    ach(s.usedAutoAnywhere, "Autoassisted",  "использован auto");
    ach(s.anyPerfect && !s.usedAutoAnywhere,
                            "Pure Master",   "идеал без auto");
    ach(s.minimalist,       "Minimalist",    "идеал ≤ 12 символов");
    ach(s.bitwisePerfect,   "Bitwise Wizard","идеал с битовыми операциями");
    ach(s.explorer,         "Explorer",      "5+ разных выражений на уровне");
    ach(s.pveChampion,      "PvE Champion",  "победа в PvE турнире");
    ach(s.pvpGames > 0,     "Network Duelist", "первая сетевая игра");
    ach(s.pvpChampion,      "PvP Champion",  "победа в сетевой игре");
}

// ============ Тренировка ============
// Возврат: 0 — в меню; 1 — выйти; 2 — показать итоги
static int playLevel(int idx, const std::string& name, int n,
                     std::mt19937& rng, Records& rec, Session& sess) {
    std::cout << "\n" << Col::BOLD << Col::CYA
              << "==== Уровень: " << name << " (" << n << " точек) ====\n" << Col::RST << "\n";
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
            std::cout << Col::YEL << "Новый рекорд сохранён ("
                      << bestHits << "/" << n << ")!\n" << Col::RST;
        }
    };

    while (true) {
        std::cout << Col::BOLD << "f(x) = " << Col::RST; std::cout.flush();
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
                std::string r = complexityRating(a.len);
                std::cout << "  " << (i+1) << ". f(x) = " << a.expr << "  ->  "
                          << a.hits << "/" << n << "  ("
                          << rateColor(r) << r << Col::RST << ")\n";
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
            std::cout << Col::YEL << "Отменено.\n" << Col::RST;
            continue;
        }

        bool fromAuto = false;
        if (cmd == "auto") {
            if (usedAutoLevel) { std::cout << Col::RED << "auto уже использован.\n" << Col::RST; continue; }
            usedAutoLevel = sess.usedAutoAnywhere = true;
            std::string poly = makeLagrangeExpr(pts);
            if (poly.empty()) { std::cout << Col::RED << "Полином слишком сложный.\n" << Col::RST; continue; }
            std::cout << Col::MAG << "auto → f(x) = " << poly << "\n" << Col::RST;
            line = poly; fromAuto = true;
        } else if (cmd.rfind("poly ", 0) == 0) {
            std::istringstream iss(cmd.substr(5));
            int K = 0;
            if (!(iss >> K) || K < 2 || K > n) {
                std::cout << Col::RED << "Использование: poly K, 2 ≤ K ≤ " << n << "\n" << Col::RST;
                continue;
            }
            std::vector<Point> sub(pts.begin(), pts.begin() + K);
            std::string poly = makeLagrangeExpr(sub);
            if (poly.empty()) { std::cout << Col::RED << "Полином слишком сложный.\n" << Col::RST; continue; }
            std::cout << Col::MAG << "poly " << K << " → f(x) = " << poly << "\n" << Col::RST;
            line = poly;
        } else if (cmd.empty()) continue;

        std::string expr = preprocess(line);
        try { ExprParser t(expr, 0.0); t.parse(); }
        catch (const std::exception& e) {
            std::cout << Col::RED << "Ошибка: " << e.what() << "\n" << Col::RST;
            continue;
        }

        attempts++; sess.totalAttempts++; sess.anyAttempt = true;
        std::string stripped = stripSpaces(expr);
        distinctExprs.insert(stripped);
        if (distinctExprs.size() >= 5) sess.explorer = true;

        int hc = 0;
        std::cout << "\n";
        for (size_t i = 0; i < pts.size(); ++i) {
            double y;
            try { ExprParser p(expr, pts[i].x); y = p.parse(); }
            catch (const std::exception& e) {
                std::cout << Col::RED << "  [ERR]  " << Col::RST << e.what() << "\n";
                continue;
            }
            double tol = 1e-6 * (1.0 + std::fabs(pts[i].y));
            if (std::isfinite(y) && std::fabs(y - pts[i].y) < tol) {
                hc++;
                std::cout << Col::GRN << "  [OK]   " << Col::RST
                          << "f(" << pts[i].x << ") = " << y << "  -> " << pts[i].y << "\n";
            } else {
                std::cout << Col::RED << "  [MISS] " << Col::RST
                          << "f(" << pts[i].x << ") = " << y << "  (нужно " << pts[i].y << ")\n";
            }
        }
        int exprLen = (int)stripped.size();
        std::string rating = complexityRating(exprLen);
        std::cout << "\nПопаданий: " << Col::BOLD << hc << "/" << n << Col::RST
                  << "   длина: " << exprLen
                  << "   рейтинг: " << rateColor(rating) << Col::BOLD << rating << Col::RST;
        if (fromAuto) std::cout << Col::DIM << "  [auto]" << Col::RST;
        std::cout << "\n\n";

        history.push_back({expr, hc, exprLen});
        if (hc > bestHits || (hc == bestHits && exprLen < bestLen)) {
            bestHits = hc; bestExpr = expr; bestLen = exprLen;
            std::cout << Col::YEL << "* Новый лучший: " << bestHits << "/" << n << Col::RST << "\n";
        }
        if (hc == n) {
            sess.anyPerfect = true;
            if (attempts == 1) sess.anySpeedrun = true;
            if (exprLen <= 12) sess.minimalist = true;
            if (hasBitwise(expr)) sess.bitwisePerfect = true;
            std::cout << Col::GRN << Col::BOLD << "* ИДЕАЛЬНО! " << n << "/" << n
                      << "  (рейтинг " << rating << ")\n" << Col::RST;
            commitRecord();
            std::cout << "\nНажми Enter, чтобы вернуться в меню...";
            std::string tmp; std::getline(std::cin, tmp);
            return 0;
        }
        std::cout << "Лучшее: " << Col::YEL << bestHits << "/" << n
                  << Col::RST << "  (f(x) = " << bestExpr << ")\n";
        std::cout << Col::DIM
                  << "Enter — ещё, 'auto', 'undo', 'hint', 'back', 'end'\n" << Col::RST << "\n";
    }
}

// ============ PvE ============
static std::string computerPickFunction(const std::vector<Point>& pts, std::mt19937& rng) {
    int n = (int)pts.size();
    int fit;
    switch (n) {
        case 2: fit = 1; break;
        case 3: fit = 1; break;
        case 4: fit = 2; break;
        case 6: fit = 3; break;
        default: fit = std::max(1, n / 2);
    }
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

static int playPvELevel(int /*idx*/, const std::string& name, int n, std::mt19937& rng) {
    std::cout << "\n" << Col::BOLD << Col::CYA
              << "==== PvE: " << name << " (" << n << " точек) ====\n" << Col::RST << "\n";
    auto pts = generatePoints(n, rng);
    std::cout << "Общие точки:\n";
    for (auto& p : pts) std::cout << "  (" << p.x << ", " << p.y << ")\n";

    std::string compExpr = computerPickFunction(pts, rng);
    int compHits = evaluateHits(compExpr, pts);

    std::cout << "\n" << Col::MAG << "Компьютер сделал ход."
              << Col::RST << "  Результат: " << Col::BOLD << compHits << "/" << n
              << Col::RST << "  (функция скрыта)\n";
    std::cout << "Цель: " << Col::BOLD << ">" << compHits << Col::RST
              << " для победы, =" << compHits << " для ничьей.\n";
    std::cout << Col::DIM << "auto/poly в PvE недоступны. 'end' — завершить игру.\n\n" << Col::RST;

    int bestHits = 0;
    std::string bestExpr;
    bool userWon = false;
    while (true) {
        std::cout << Col::BOLD << "f(x) = " << Col::RST; std::cout.flush();
        std::string line;
        if (!std::getline(std::cin, line)) return -2;
        std::string cmd = trimLower(line);
        if (cmd == "quit" || cmd == "exit") return -2;
        if (cmd == "end")                   return -3;
        if (cmd == "giveup" || cmd == "accept" || cmd == "back") break;
        if (cmd == "help") { printHelp(); continue; }
        if (cmd.empty()) continue;
        if (cmd == "auto" || cmd == "poly" || cmd.rfind("poly ", 0) == 0) {
            std::cout << Col::RED << "auto/poly в PvE недоступны.\n" << Col::RST;
            continue;
        }
        std::string expr = preprocess(line);
        try { ExprParser t(expr, 0.0); t.parse(); }
        catch (const std::exception& e) {
            std::cout << Col::RED << "Ошибка: " << e.what() << "\n" << Col::RST;
            continue;
        }
        int hits = 0;
        std::cout << "\n";
        for (auto& p : pts) {
            try {
                ExprParser par(expr, p.x);
                double y = par.parse();
                double tol = 1e-6 * (1.0 + std::fabs(p.y));
                if (std::isfinite(y) && std::fabs(y - p.y) < tol) {
                    hits++;
                    std::cout << Col::GRN << "  [OK]   " << Col::RST
                              << "f(" << p.x << ") = " << y << " -> " << p.y << "\n";
                } else {
                    std::cout << Col::RED << "  [MISS] " << Col::RST
                              << "f(" << p.x << ") = " << y << " (нужно " << p.y << ")\n";
                }
            } catch (const std::exception& e) {
                std::cout << Col::RED << "  [ERR]  " << Col::RST << e.what() << "\n";
            }
        }
        if (hits > bestHits) { bestHits = hits; bestExpr = expr; }
        std::cout << "\nПопаданий: " << Col::BOLD << hits << "/" << n << Col::RST << "\n";
        if (hits > compHits) { userWon = true; break; }
        std::cout << Col::DIM << "Ещё попытка, 'giveup' или 'end':\n\n" << Col::RST;
    }

    std::cout << "\n" << Col::MAG << "Компьютер: f(x) = " << compExpr
              << Col::RST << "  (" << compHits << "/" << n << ")\n";
    if (!bestExpr.empty())
        std::cout << "Твой лучший: f(x) = " << bestExpr << "  (" << bestHits << "/" << n << ")\n";

    int result;
    if (userWon || bestHits > compHits) {
        std::cout << Col::GRN << Col::BOLD << ">>> ПОБЕДА! <<<\n" << Col::RST;
        result = 1;
    } else if (bestHits == compHits) {
        std::cout << Col::YEL << Col::BOLD << ">>> НИЧЬЯ <<<\n" << Col::RST;
        result = 0;
    } else {
        std::cout << Col::RED << Col::BOLD << ">>> ПОРАЖЕНИЕ <<<\n" << Col::RST;
        result = -1;
    }
    return result;
}

// ============ СЕТЕВАЯ ИГРА ============
// Общая логика: обе стороны выполняют этот цикл.
static void playNetworkGame(NetConn& conn, bool is_host,
                            const std::vector<Point>& pts) {
    int n = (int)pts.size();
    const char* my_name    = is_host ? "host"   : "client";
    const char* their_name = is_host ? "client" : "host";

    int my_best = 0, their_best = 0;
    bool my_passed = false, their_passed = false;
    bool my_turn = is_host;
    bool game_over = false;

    while (!game_over) {
        if (my_turn) {
            if (my_passed) {
                if (!conn.sendLine("PASS")) {
                    std::cout << Col::RED << "Связь потеряна.\n" << Col::RST;
                    return;
                }
            } else {
                bool got = false;
                while (!got) {
                    std::cout << Col::BOLD << "f(x) (или 'done', 'quit'): " << Col::RST;
                    std::cout.flush();
                    std::string expr;
                    if (!std::getline(std::cin, expr)) {
                        conn.sendLine("QUIT"); return;
                    }
                    std::string cmd = trimLower(expr);
                    if (cmd == "done" || cmd == "pass") {
                        my_passed = true;
                        if (!conn.sendLine("PASS")) return;
                        got = true;
                    } else if (cmd == "quit" || cmd == "exit" || cmd == "end") {
                        conn.sendLine("QUIT"); return;
                    } else if (cmd == "help") {
                        printHelp();
                    } else if (cmd.empty()) {
                        // пропустить
                    } else {
                        std::string code = preprocess(expr);
                        try { ExprParser t(code, 0.0); t.parse(); }
                        catch (const std::exception& e) {
                            std::cout << Col::RED << "Ошибка: " << e.what() << "\n" << Col::RST;
                            continue;
                        }
                        int hc = evaluateHits(code, pts);
                        std::cout << "Вы: f(x) = " << code << " -> "
                                  << Col::BOLD << hc << "/" << n << Col::RST << "\n";
                        if (hc > my_best) my_best = hc;
                        if (!conn.sendLine("ATTEMPT " + std::to_string(hc) + " " + code)) return;
                        got = true;
                        if (hc == n) {
                            std::cout << Col::GRN << Col::BOLD
                                      << "* ИДЕАЛЬНО! Победа!\n" << Col::RST;
                            // Соперник увидит hits==n и завершит игру у себя.
                            return;
                        }
                    }
                }
            }
            my_turn = false;
        } else {
            std::string line;
            if (!conn.recvLine(line)) {
                std::cout << Col::RED << "Соперник отключился.\n" << Col::RST;
                return;
            }
            if (line.rfind("ATTEMPT ", 0) == 0) {
                size_t sp = line.find(' ', 8);
                if (sp == std::string::npos) { return; }
                int sent_hits = 0;
                try { sent_hits = std::stoi(line.substr(8, sp - 8)); } catch (...) { return; }
                std::string expr = line.substr(sp + 1);
                int local_hits = evaluateHits(expr, pts);
                int hits = local_hits;
                if (sent_hits != local_hits) {
                    std::cout << Col::YEL << "  (расхождение: пир " << sent_hits
                              << ", локально " << local_hits
                              << " — доверяем локальному)\n" << Col::RST;
                }
                std::cout << "Соперник: f(x) = " << expr << " -> "
                          << Col::BOLD << hits << "/" << n << Col::RST << "\n";
                if (hits > their_best) their_best = hits;
                if (hits == n) {
                    std::cout << Col::RED << Col::BOLD
                              << "* Соперник победил!\n" << Col::RST;
                    return;
                }
            } else if (line == "PASS") {
                if (!their_passed)
                    std::cout << Col::YEL << "Соперник сказал 'done'.\n" << Col::RST;
                their_passed = true;
            } else if (line.rfind("GAMEOVER ", 0) == 0) {
                std::string who = line.substr(9);
                if (who == my_name) {
                    std::cout << Col::GRN << Col::BOLD
                              << "Соперник признал вашу победу!\n" << Col::RST;
                } else if (who == their_name) {
                    std::cout << Col::RED << Col::BOLD
                              << "Соперник празднует победу.\n" << Col::RST;
                } else {
                    std::cout << Col::YEL << "Объявлена ничья.\n" << Col::RST;
                }
                return;
            } else if (line == "QUIT") {
                std::cout << Col::RED << "Соперник покинул игру.\n" << Col::RST;
                return;
            }
            my_turn = true;
            if (my_passed && their_passed) game_over = true;
        }
    }

    // Оба сказали done — сравнение
    std::cout << "\n" << Col::BOLD << "=== Итог сетевой игры ===\n" << Col::RST;
    std::cout << "Вы: " << my_best << "/" << n
              << "   Соперник: " << their_best << "/" << n << "\n";
    if (my_best > their_best) {
        std::cout << Col::GRN << Col::BOLD << ">>> ПОБЕДА! <<<\n" << Col::RST;
        conn.sendLine(std::string("GAMEOVER ") + my_name);
    } else if (their_best > my_best) {
        std::cout << Col::RED << Col::BOLD << ">>> ПОРАЖЕНИЕ <<<\n" << Col::RST;
        conn.sendLine(std::string("GAMEOVER ") + their_name);
    } else {
        std::cout << Col::YEL << Col::BOLD << ">>> НИЧЬЯ <<<\n" << Col::RST;
        conn.sendLine("GAMEOVER tie");
    }
}

// ---------- Хост ----------
static void hostRoom(std::mt19937& rng, Session& sess) {
    if (!netInit()) {
        std::cout << Col::RED << "Ошибка инициализации сети.\n" << Col::RST;
        return;
    }
    std::cout << "\n" << Col::BOLD << "--- Создание комнаты ---\n" << Col::RST;
    std::cout << "Порт (Enter = 5555): "; std::cout.flush();
    std::string portStr;
    std::getline(std::cin, portStr);
    int port = 5555;
    if (!portStr.empty()) {
        try { port = std::stoi(portStr); }
        catch (...) { std::cout << Col::RED << "Неверный порт.\n" << Col::RST; return; }
    }
    if (port < 1 || port > 65535) {
        std::cout << Col::RED << "Порт вне диапазона.\n" << Col::RST; return;
    }

    sock_t listen_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCK) {
        std::cout << Col::RED << "Не удалось создать сокет.\n" << Col::RST; return;
    }
    int yes = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);
    if (::bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cout << Col::RED << "Не удалось привязаться к порту " << port << ".\n" << Col::RST;
        closeSock(listen_sock);
        return;
    }
    if (::listen(listen_sock, 1) != 0) {
        std::cout << Col::RED << "Ошибка listen.\n" << Col::RST;
        closeSock(listen_sock);
        return;
    }

    std::string ip = getLocalIp();
    std::cout << "\n" << Col::CYA << "Ваш IP: " << Col::BOLD
              << (ip.empty() ? "(неизвестен, узнайте вручную)" : ip) << Col::RST << "\n";
    std::cout << "Порт: " << Col::BOLD << port << Col::RST << "\n";
    std::cout << "Ожидание клиента (Ctrl+C для отмены)... " << std::flush;

    sock_t client_sock = INVALID_SOCK;
    while (true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_sock, &fds);
        struct timeval tv;
        tv.tv_sec = 5; tv.tv_usec = 0;
        int r = select((int)listen_sock + 1, &fds, nullptr, nullptr, &tv);
        if (r > 0) {
            client_sock = ::accept(listen_sock, nullptr, nullptr);
            break;
        } else if (r == 0) {
            std::cout << "." << std::flush;
        } else {
            std::cout << "\n" << Col::RED << "Ошибка select.\n" << Col::RST;
            closeSock(listen_sock);
            return;
        }
    }
    closeSock(listen_sock);
    std::cout << "\n" << Col::GRN << "Клиент подключился!\n" << Col::RST;

    NetConn conn(client_sock);

    // Генерируем и отправляем точки
    auto pts = generatePoints(4, rng);
    std::cout << "\nОбщие точки (4):\n";
    for (auto& p : pts) std::cout << "  (" << p.x << ", " << p.y << ")\n";

    if (!sendPoints(conn, pts)) {
        std::cout << Col::RED << "Ошибка отправки точек.\n" << Col::RST;
        return;
    }

    std::cout << "\n" << Col::BOLD
              << "Игра началась. Хост ходит первым.\n"
              << "Введите f(x) или 'done'. Введите 'quit' для выхода.\n" << Col::RST << "\n";

    playNetworkGame(conn, true, pts);
    sess.pvpGames++;
}

// ---------- Клиент ----------
static void joinRoom(Session& sess) {
    if (!netInit()) {
        std::cout << Col::RED << "Ошибка инициализации сети.\n" << Col::RST;
        return;
    }
    std::cout << "\n" << Col::BOLD << "--- Подключение к комнате ---\n" << Col::RST;
    std::cout << "IP хоста: "; std::cout.flush();
    std::string ip;
    if (!std::getline(std::cin, ip) || ip.empty()) return;
    std::cout << "Порт (Enter = 5555): "; std::cout.flush();
    std::string portStr;
    std::getline(std::cin, portStr);
    int port = 5555;
    if (!portStr.empty()) {
        try { port = std::stoi(portStr); }
        catch (...) { std::cout << Col::RED << "Неверный порт.\n" << Col::RST; return; }
    }

    sock_t s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCK) { std::cout << Col::RED << "Ошибка сокета.\n" << Col::RST; return; }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        std::cout << Col::RED << "Неверный IP.\n" << Col::RST;
        closeSock(s); return;
    }
    std::cout << "Подключение к " << ip << ":" << port << "... " << std::flush;
    if (::connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cout << Col::RED << "не удалось.\n" << Col::RST;
        closeSock(s); return;
    }
    std::cout << Col::GRN << "успешно!\n" << Col::RST;

    NetConn conn(s);
    std::vector<Point> pts;
    if (!recvPoints(conn, pts)) {
        std::cout << Col::RED << "Ошибка получения точек.\n" << Col::RST;
        return;
    }
    std::cout << "\nОбщие точки (" << pts.size() << "):\n";
    for (auto& p : pts) std::cout << "  (" << p.x << ", " << p.y << ")\n";

    std::cout << "\n" << Col::BOLD
              << "Игра началась. Хост ходит первым.\n"
              << "Введите f(x) или 'done'. Введите 'quit' для выхода.\n" << Col::RST << "\n";

    playNetworkGame(conn, false, pts);
    sess.pvpGames++;
}

static void networkMenu(std::mt19937& rng, Session& sess) {
    while (true) {
        std::cout << "\n" << Col::BOLD << Col::CYA
                  << "=== СЕТЕВАЯ ИГРА (PvP) ===\n" << Col::RST;
        std::cout << "  1. Создать комнату (хост)\n";
        std::cout << "  2. Подключиться к комнате (клиент)\n";
        std::cout << "  0. Назад\n";
        std::cout << Col::BOLD << "> " << Col::RST; std::cout.flush();
        std::string line;
        if (!std::getline(std::cin, line)) return;
        std::string cmd = trimLower(line);
        if (cmd.empty()) continue;
        if (cmd == "0" || cmd == "back" || cmd == "menu") return;
        if (cmd == "1") {
            int before = sess.pvpGames;
            hostRoom(rng, sess);
            // Проверка на победу по результатам — упрощённо: если сессия
            // пополнилась pvpGames, значит игра состоялась.
            if (sess.pvpGames > before) {
                std::cout << Col::DIM << "Нажми Enter...\n" << Col::RST;
                std::string tmp; std::getline(std::cin, tmp);
            }
            continue;
        }
        if (cmd == "2") {
            int before = sess.pvpGames;
            joinRoom(sess);
            if (sess.pvpGames > before) {
                std::cout << Col::DIM << "Нажми Enter...\n" << Col::RST;
                std::string tmp; std::getline(std::cin, tmp);
            }
            continue;
        }
        std::cout << Col::RED << "Неизвестная команда.\n" << Col::RST;
    }
}

// ============ main ============
int main() {
    setlocale(LC_ALL, ".utf-8");
    enableAnsi();
    std::mt19937 rng(std::random_device{}());
    Records rec; loadRecords(rec);
    Session sess;

    const std::vector<std::pair<std::string, int>> levels = {
        {"Лёгкий", 2}, {"Средний", 3}, {"Сложный", 4}, {"Безумный", 6}
    };

    while (true) {
        std::cout << "\n" << Col::CYA << Col::BOLD
                  << "=============================================================\n"
                  << "                  ФУНКЦИЯ-ДЕТЕКТИВ  v8.0\n"
                  << "=============================================================\n"
                  << Col::RST << "\n";
        std::cout << "Выбери режим:\n\n";
        for (int i = 0; i < 4; i++) {
            std::cout << "  " << (i + 1) << ". Тренировка — " << levels[i].first
                      << " (" << levels[i].second << " точек)";
            if (rec.score[i] > 0)
                std::cout << "   " << Col::YEL << "рекорд: " << rec.score[i]
                          << "/" << levels[i].second << Col::RST;
            std::cout << "\n";
        }
        std::cout << "  5. " << Col::MAG << "PvE" << Col::RST
                  << " — турнир против компьютера (4 уровня)\n";
        std::cout << "  6. Статистика и достижения\n";
        std::cout << "  7. " << Col::GRN << "Сетевая игра" << Col::RST
                  << " — PvP по сети (хост / клиент)\n";
        std::cout << "  h. Справка по синтаксису\n";
        std::cout << "  " << Col::GRN << "end" << Col::RST
                  << " — завершить игру с показом итогов\n";
        std::cout << "  0. Выход (без итогов)\n\n";
        std::cout << Col::BOLD << "> " << Col::RST; std::cout.flush();

        std::string line;
        if (!std::getline(std::cin, line)) { netCleanup(); return 0; }
        std::string cmd = trimLower(line);
        if (cmd.empty()) continue;
        if (cmd == "0" || cmd == "exit" || cmd == "quit") {
            std::cout << "До свидания!\n";
            netCleanup();
            return 0;
        }
        if (cmd == "end") {
            showStats(sess, rec);
            std::cout << "Спасибо за игру!\n";
            netCleanup();
            return 0;
        }
        if (cmd == "h" || cmd == "help") { printHelp(); continue; }
        if (cmd == "6") {
            showStats(sess, rec);
            std::cout << "\nНажми Enter, чтобы вернуться в меню...";
            std::string tmp; std::getline(std::cin, tmp);
            continue;
        }
        if (cmd == "7") { networkMenu(rng, sess); continue; }

        if (cmd == "5") {
            std::cout << "\n" << Col::BOLD << Col::CYA
                      << "====== PvE ТУРНИР ======\n" << Col::RST
                      << "Ты сражаешься с компьютером. У каждого свои точки.\n\n"
                      << "Нажми Enter для старта...";
            std::string tmp; std::getline(std::cin, tmp);

            int wins = 0, ties = 0, losses = 0;
            bool pveEnd = false;
            for (int i = 0; i < 4; i++) {
                int r = playPvELevel(i, levels[i].first, levels[i].second, rng);
                if (r == -3) { pveEnd = true; break; }
                if (r == -2) { std::cout << "Выход из турнира.\n"; break; }
                if (r > 0) wins++;
                else if (r == 0) ties++;
                else losses++;
                if (i < 3) {
                    std::cout << "\nEnter для продолжения...";
                    std::getline(std::cin, tmp);
                }
            }
            if (pveEnd) {
                showStats(sess, rec);
                std::cout << "Спасибо за игру!\n";
                netCleanup();
                return 0;
            }
            if (wins + ties + losses > 0) {
                std::cout << "\n" << Col::BOLD << "=== ИТОГ PvE ===\n" << Col::RST;
                std::cout << "Побед: " << Col::GRN << wins << Col::RST
                          << "   Ничьих: " << Col::YEL << ties << Col::RST
                          << "   Поражений: " << Col::RED << losses << Col::RST << "\n";
                if (wins > losses && wins + ties + losses == 4) {
                    std::cout << Col::GRN << Col::BOLD
                              << "Ты победил компьютер в турнире!\n" << Col::RST;
                    sess.pveChampion = true;
                } else if (wins < losses) {
                    std::cout << Col::RED << "Компьютер победил в турнире.\n" << Col::RST;
                } else {
                    std::cout << "Ничья в турнире.\n";
                }
                std::cout << "\nEnter для возврата в меню...";
                std::getline(std::cin, tmp);
            }
            continue;
        }

        if (cmd.size() == 1 && cmd[0] >= '1' && cmd[0] <= '4') {
            int idx = cmd[0] - '1';
            int r = playLevel(idx, levels[idx].first, levels[idx].second, rng, rec, sess);
            if (r == 1) { netCleanup(); return 0; }
            if (r == 2) {
                showStats(sess, rec);
                std::cout << "Спасибо за игру!\n";
                netCleanup();
                return 0;
            }
            continue;
        }
        std::cout << Col::RED << "Неизвестная команда. Введи 0–7, h или end.\n" << Col::RST;
    }
}
