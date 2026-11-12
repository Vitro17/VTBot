#include "overlay.h"
#include "log.h"
#include "config.h"
#include "ui.h"
#include <windows.h>
#include <mutex>
#include <atomic>
#include <climits>

namespace vt::overlay {

static std::mutex g_mu;
static std::wstring g_text;
static DWORD g_until = 0, g_start = 0;
static unsigned g_seq = 0;
static volatile bool g_stop = false;
static std::atomic<int> g_dx{0}, g_dy{0};
static HANDLE g_thread = nullptr;

static const COLORREF kKey = RGB(255, 0, 255);   // color transparente
static const int kFade = 600;                    // ms de desvanecimiento final

static std::wstring Widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

struct FindCtx { DWORD pid; HWND hwnd; };
static BOOL CALLBACK EnumCb(HWND h, LPARAM lp) {
    auto* c = (FindCtx*)lp;
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    if (pid != c->pid || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    wchar_t cls[64]; GetClassNameW(h, cls, 64);
    if (wcsncmp(cls, L"VTBot", 5) == 0) return TRUE;
    wchar_t title[128]; GetWindowTextW(h, title, 128);
    if (wcsncmp(title, L"Tibia", 5) == 0) { c->hwnd = h; return FALSE; }
    return TRUE;
}
static HWND FindClient() {
    FindCtx c{GetCurrentProcessId(), nullptr};
    EnumWindows(EnumCb, (LPARAM)&c);
    return c.hwnd;
}

static HFONT MakeFont() {
    return CreateFontW(-17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Verdana");
}

static LRESULT CALLBACK OvProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HBRUSH br = CreateSolidBrush(kKey);
        FillRect(dc, &rc, br);
        DeleteObject(br);
        std::wstring t;
        { std::lock_guard<std::mutex> lk(g_mu); t = g_text; }
        HFONT f = MakeFont();
        HGDIOBJ old = SelectObject(dc, f);
        SetBkMode(dc, TRANSPARENT);
        const int d[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
        SetTextColor(dc, RGB(0, 0, 0));
        for (auto& o : d) {
            RECT r = rc; OffsetRect(&r, 2 + o[0], 2 + o[1]);
            DrawTextW(dc, t.c_str(), (int)t.size(), &r, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);
        }
        SetTextColor(dc, RGB(255, 255, 255));
        RECT r = rc; OffsetRect(&r, 2, 2);
        DrawTextW(dc, t.c_str(), (int)t.size(), &r, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);
        SelectObject(dc, old); DeleteObject(f);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

// --- Boton "VT" dentro del cliente: oculta / muestra la ventana del HUD -------------------------------
static const int kBW = 40, kBH = 20;
static HWND g_client = nullptr;
static bool g_down = false, g_moved = false, g_dragging = false;
static POINT g_downPt{0, 0};
static RECT g_downRc{0, 0, 0, 0};

static LRESULT CALLBACK BtnProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_SETCURSOR: SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE;
    case WM_LBUTTONDOWN:
        g_down = true; g_moved = false;
        GetCursorPos(&g_downPt); GetWindowRect(h, &g_downRc);
        SetCapture(h);
        return 0;
    case WM_MOUSEMOVE:
        if (g_down) {
            POINT p; GetCursorPos(&p);
            int dx = p.x - g_downPt.x, dy = p.y - g_downPt.y;
            if (!g_moved && (abs(dx) > 4 || abs(dy) > 4)) g_moved = true;   // arrastrar para recolocar
            if (g_moved) {
                g_dragging = true;
                SetWindowPos(h, nullptr, g_downRc.left + dx, g_downRc.top + dy, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_down) {
            ReleaseCapture();
            g_down = false;
            if (g_moved && g_client) {
                RECT wr, cr; GetWindowRect(h, &wr); GetClientRect(g_client, &cr);
                POINT o{0, 0}; ClientToScreen(g_client, &o);
                auto& t = GetConfig().tools;
                t.btnRight = (int)(o.x + cr.right - wr.right);
                t.btnTop = (int)(wr.top - o.y);
                SaveConfig();
                g_dragging = false;
            } else {
                ui::ToggleVisible();
            }
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        bool on = ui::IsVisible();
        HBRUSH bg = CreateSolidBrush(RGB(28, 28, 28));
        FillRect(dc, &rc, bg); DeleteObject(bg);
        HBRUSH fr = CreateSolidBrush(on ? RGB(26, 199, 133) : RGB(120, 120, 120));
        FrameRect(dc, &rc, fr);
        InflateRect(&rc, -1, -1); FrameRect(dc, &rc, fr); DeleteObject(fr);
        HFONT f = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Verdana");
        HGDIOBJ old = SelectObject(dc, f);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, on ? RGB(26, 199, 133) : RGB(200, 200, 200));
        DrawTextW(dc, L"VT", 2, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(dc, old); DeleteObject(f);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, msg, w, l);
}

static void ManageButton(HWND client, HWND& btn, HINSTANCE hi) {
    auto& t = GetConfig().tools;
    bool want = client && t.hudButton && IsWindowVisible(client) && !IsIconic(client);
    if (!want) { if (btn && IsWindowVisible(btn)) ShowWindow(btn, SW_HIDE); return; }
    if (!btn) {
        btn = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"VTBotButton", L"VT", WS_POPUP, 0, 0, kBW, kBH,
                              client, nullptr, hi, nullptr);
        if (!btn) { static bool once = false; if (!once) { once = true; Log("overlay: no se pudo crear el boton (%lu)", GetLastError()); } return; }
        Log("overlay: boton VT creado (%p)", (void*)btn);
    }
    if (!g_dragging) {
        RECT cr; GetClientRect(client, &cr);
        POINT p{0, 0}; ClientToScreen(client, &p);
        int x = p.x + cr.right - t.btnRight - kBW, y = p.y + t.btnTop;
        RECT wr; GetWindowRect(btn, &wr);
        if (wr.left != x || wr.top != y) {
            SetWindowPos(btn, nullptr, x, y, kBW, kBH, SWP_NOACTIVATE | SWP_NOZORDER);
            Log("overlay: boton VT en (%d,%d) cliente=%dx%d origen=(%ld,%ld)", x, y, (int)cr.right, (int)cr.bottom, p.x, p.y);
        }
    }
    if (!IsWindowVisible(btn)) ShowWindow(btn, SW_SHOWNOACTIVATE);
    static bool lastOn = false;
    bool on = ui::IsVisible();
    if (on != lastOn) { lastOn = on; InvalidateRect(btn, nullptr, TRUE); }
}

static DWORD WINAPI Thread(LPVOID) {
    HINSTANCE hi = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = OvProc;
    wc.hInstance = hi;
    wc.lpszClassName = L"VTBotOverlay";
    RegisterClassExW(&wc);
    WNDCLASSEXW wb{sizeof(wb)};
    wb.lpfnWndProc = BtnProc;
    wb.hInstance = hi;
    wb.hCursor = LoadCursor(nullptr, IDC_HAND);
    wb.lpszClassName = L"VTBotButton";
    RegisterClassExW(&wb);

    HWND client = nullptr, win = nullptr, btn = nullptr;
    unsigned shownSeq = 0;
    int curW = 10, curH = 10, lastX = INT_MIN, lastY = INT_MIN;
    while (!g_stop) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }

        if (!client || !IsWindow(client)) {
            client = FindClient();
            if (win) { DestroyWindow(win); win = nullptr; }
            if (btn) { DestroyWindow(btn); btn = nullptr; }
        }
        g_client = client;
        ManageButton(client, btn, hi);
        DWORD now = GetTickCount();
        std::wstring text; DWORD until, start; unsigned seq;
        { std::lock_guard<std::mutex> lk(g_mu); text = g_text; until = g_until; start = g_start; seq = g_seq; }
        bool active = client && seq && (int)(until - now) > 0 && !IsIconic(client);

        if (!active) {
            if (win && IsWindowVisible(win)) ShowWindow(win, SW_HIDE);
            Sleep(30);
            continue;
        }
        if (!win) {
            win = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"VTBotOverlay",
                                  L"", WS_POPUP, 0, 0, 10, 10, client, nullptr, hi, nullptr);
            if (!win) { Log("overlay: no se pudo crear la ventana (%lu)", GetLastError()); Sleep(500); continue; }
            shownSeq = 0;
        }
        if (shownSeq != seq) {   // texto nuevo: medir y recolocar
            HDC dc = GetDC(win);
            HFONT f = MakeFont(); HGDIOBJ old = SelectObject(dc, f);
            RECT tr{0, 0, 0, 0};
            DrawTextW(dc, text.c_str(), (int)text.size(), &tr, DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE);
            SelectObject(dc, old); DeleteObject(f); ReleaseDC(win, dc);
            curW = tr.right + 6; curH = tr.bottom + 6;
            InvalidateRect(win, nullptr, TRUE);
            shownSeq = seq;
            lastX = lastY = INT_MIN;   // forzar reposicion
            (void)start;
        }
        {   // posicion (cada ciclo, para que los ajustes de la UI se vean en vivo)
            RECT cr; GetClientRect(client, &cr);
            POINT p{0, 0}; ClientToScreen(client, &p);
            int x = p.x + (cr.right - curW) / 2 + g_dx, y = p.y + (int)(cr.bottom * 0.40) + g_dy;
            if (x != lastX || y != lastY) {
                SetWindowPos(win, nullptr, x, y, curW, curH, SWP_NOACTIVATE | SWP_NOZORDER);
                lastX = x; lastY = y;
            }
        }
        // desvanecimiento final
        int left = (int)(until - now);
        BYTE a = left >= kFade ? 255 : (BYTE)(255 * left / kFade);
        SetLayeredWindowAttributes(win, kKey, a, LWA_COLORKEY | LWA_ALPHA);
        if (!IsWindowVisible(win)) ShowWindow(win, SW_SHOWNOACTIVATE);
        Sleep(30);
    }
    if (win) DestroyWindow(win);
    if (btn) DestroyWindow(btn);
    UnregisterClassW(L"VTBotOverlay", hi);
    UnregisterClassW(L"VTBotButton", hi);
    return 0;
}

void Start() {
    g_stop = false;
    g_thread = CreateThread(nullptr, 0, Thread, nullptr, 0, nullptr);
}

void Stop() {
    g_stop = true;
    if (g_thread) { WaitForSingleObject(g_thread, 3000); CloseHandle(g_thread); g_thread = nullptr; }
}

void SetOffset(int dx, int dy) { g_dx = dx; g_dy = dy; }

void Show(const std::string& text, unsigned ms) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_text = Widen(text);
    g_start = GetTickCount();
    g_until = g_start + ms;
    ++g_seq;
}

}
