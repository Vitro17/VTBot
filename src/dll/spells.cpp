#include "spells.h"
#include "memory.h"
#include "log.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace vt::spells {

// Registro de hechizo (15.03), relativo al QString de las palabras `W` (d, ptr, size):
//   W-0x28 id (u64)   W-0x20 id (u64, repetido)   W-0x18/-0x10/-0x08 QString del nombre (d, ptr, size)
// El cliente guarda varias copias de las palabras; solo el registro cumple id == id repetido.
constexpr int kIdOff = 0x28, kId2Off = 0x20, kNameSizeOff = 0x08;

struct Cached {
    int id = 0;
    DWORD nextTry = 0;
    bool running = false;
};

static std::mutex g_mx;
static std::map<std::string, Cached> g_cache;
static std::atomic<int> g_running{0};
static std::atomic<bool> g_stop{false};
static std::atomic<int> g_dTexts{0}, g_dQs{0}, g_dRej{0}, g_dExc{0}, g_dLast{0};
static int ExcFilter(unsigned code) { g_dExc = (int)code; return EXCEPTION_EXECUTE_HANDLER; }

static std::string Norm(const std::string& s) {
    size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    if (a == std::string::npos) return "";
    std::string r = s.substr(a, b - a + 1);
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static int Lookup(const std::string& words) {
    std::u16string w16;
    for (unsigned char c : words) w16 += (char16_t)c;      // palabras de hechizos: ASCII
    const size_t nch = w16.size();
    if (nch == 0 || nch > 40) return 0;
    const uint8_t* pat = (const uint8_t*)w16.data();
    const size_t plen = nch * 2;

    // pasada 1: ocurrencias del texto UTF-16
    std::vector<uintptr_t> texts;
    auto searcher = std::boyer_moore_horspool_searcher(pat, pat + plen);
    mem::ScanPrivate([&](uintptr_t base, const uint8_t* d, size_t n) {
        if (g_stop) return false;
        const uint8_t* p = d;
        const uint8_t* end = d + n;
        while (p < end) {
            auto it = std::search(p, end, searcher);
            if (it == end) break;
            if (((it - d) & 1) == 0 && texts.size() < 512) texts.push_back(base + (it - d));
            p = it + 1;
        }
        return true;
    });
    g_dTexts = (int)texts.size();
    if (texts.empty()) return 0;

    // pasada 2: QStrings {d = T-0x20, ptr = T, size = nch} que apuntan a esas ocurrencias
    std::unordered_set<uint64_t> tset(texts.begin(), texts.end());
    int found = 0;
    mem::ScanPrivate([&](uintptr_t base, const uint8_t* d, size_t n) {
        if (g_stop) return false;
        const uint64_t* q = (const uint64_t*)d;
        size_t cnt = n / 8;
        for (size_t i = 0; i + 2 < cnt; ++i) {
            if (!tset.count(q[i + 1])) continue;
            if (q[i] != q[i + 1] - 0x10 || q[i + 2] != nch) continue;
            uintptr_t W = base + i * 8;
            ++g_dQs;
            uint64_t id = mem::Read<uint64_t>(W - kIdOff), id2 = mem::Read<uint64_t>(W - kId2Off);
            uint64_t nameSize = mem::Read<uint64_t>(W - kNameSizeOff);
            if (id == id2 && id > 0 && id < 5000 && nameSize > 0 && nameSize < 48) { found = (int)id; return false; }
            ++g_dRej; g_dLast = (int)id;
        }
        return true;
    });
    return found;
}

// Sin objetos C++ con destructor: permite __try
static int SafeLookup(const std::string& words) {
    __try {
        return Lookup(words);
    } __except (ExcFilter(GetExceptionCode())) {
        return 0;
    }
}

int IdForWords(const std::string& wordsIn) {
    std::string words = Norm(wordsIn);
    if (words.empty()) return 0;
    DWORD now = GetTickCount();
    {
        std::lock_guard<std::mutex> l(g_mx);
        Cached& c = g_cache[words];
        if (c.id) return c.id;
        if (c.running || (int)(now - c.nextTry) < 0 || g_stop) return 0;
        c.running = true;
    }
    ++g_running;
    std::thread([words]() {
        g_dTexts = 0; g_dQs = 0; g_dRej = 0; g_dExc = 0;
        int id = SafeLookup(words);
        Log("spells diag: textos=%d qstrings=%d rechazados=%d ultimoId=%d exc=0x%x", g_dTexts.load(), g_dQs.load(), g_dRej.load(), g_dLast.load(), g_dExc.load());
        {
            std::lock_guard<std::mutex> l(g_mx);
            Cached& c = g_cache[words];
            c.id = id;
            c.running = false;
            c.nextTry = GetTickCount() + 15000;
        }
        Log("spells: '%s' -> id %d%s", words.c_str(), id, id ? "" : " (no encontrado; se reintenta)");
        --g_running;
    }).detach();
    return 0;
}

void Shutdown() {
    g_stop = true;
    for (int i = 0; i < 100 && g_running > 0; ++i) Sleep(100);
}
}
