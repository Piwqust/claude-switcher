#include "gfx.h"
#include <dwmapi.h>
#include <commctrl.h>
#include <windowsx.h>
#include <cmath>

namespace Gfx {
    ID2D1Factory* D2D; IDWriteFactory3* DW; IWICImagingFactory* WIC;
    Theme T;
    static std::wstring textFamily = L"Segoe UI Variable Text", displayFamily = L"Segoe UI Variable Display", iconFamily = L"Segoe Fluent Icons";

    static bool HasFamily(const wchar_t* name) {
        IDWriteFontCollection* c = nullptr; DW->GetSystemFontCollection(&c);
        UINT32 i; BOOL exists = FALSE; c->FindFamilyName(name, &i, &exists); c->Release();
        return exists != FALSE;
    }

    void Init() {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2D);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory3), (IUnknown**)&DW);
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&WIC));
        // Windows 10 has neither Segoe UI Variable nor Segoe Fluent Icons.
        if (!HasFamily(textFamily.c_str())) textFamily = displayFamily = L"Segoe UI";
        if (!HasFamily(iconFamily.c_str())) iconFamily = L"Segoe MDL2 Assets";
        ApplyTheme();
    }

    D2D1_COLOR_F Mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float t) {
        return D2D1::ColorF(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
    }

    bool SystemDark() {
        DWORD v = 1, n = sizeof v;
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &n);
        return v == 0;
    }

    bool BackdropSupported() {
        static int build = [] {
            typedef LONG(WINAPI * Fn)(OSVERSIONINFOW*);
            OSVERSIONINFOW o{ sizeof o };
            ((Fn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"))(&o);
            return (int)o.dwBuildNumber;
        }();
        return build >= 22621;
    }

    void ApplyTheme() {
        wchar_t forced[16] = {};
        GetEnvironmentVariableW(L"CPS_THEME", forced, 16);   // light|dark, for checking both looks
        bool dark = wcscmp(forced, L"dark") == 0 || (wcscmp(forced, L"light") != 0 && SystemDark());
        auto c = [](UINT rgb, float a = 1) { return Rgba(rgb, a); };
        if (dark) T = { true, c(0x202020), c(0xFFFFFF), c(0xC8C8C8), c(0x8F8F8F),
            c(0xFFFFFF, .05f), c(0xFFFFFF, .09f), c(0xFFFFFF, .03f), c(0xFFFFFF, .08f), c(0xFFFFFF, .05f), c(0x000000, .10f),
            c(0xFFFFFF, .06f), c(0xFFFFFF, .09f), c(0xFFFFFF, .03f), c(0xFFFFFF, .08f), c(0xFFFFFF, .55f), c(0x2C2C2C), c(0x3D3D3D),
            c(0xE08A6B), c(0xE69A7E), c(0xCC7A5D), c(0xF0A386), c(0x1A1A1A), c(0xE08A6B, .15f), c(0x6CCB5F), c(0xFCE100), c(0xFF99A4), c(0xFFFFFF), c(0xFFFFFF, .08f) };
        else T = { false, c(0xF3F3F3), c(0x1B1B1B), c(0x5D5D5D), c(0x8B8B8B),
            c(0xFFFFFF, .70f), c(0xFFFFFF, .95f), c(0xFFFFFF, .50f), c(0x000000, .07f), c(0xFFFFFF, .70f), c(0x000000, .06f),
            c(0xFFFFFF, .70f), c(0xF9F9F9, .50f), c(0xF9F9F9, .30f), c(0x000000, .08f), c(0x000000, .45f), c(0xF9F9F9), c(0xE2E2E2),
            c(0xC15F3C), c(0xCB6C4A), c(0xAA5333), c(0xB0532F), c(0xFFFFFF), c(0xC15F3C, .10f), c(0x0F7B0F), c(0x9D5D00), c(0xC42B1C), c(0x1B1B1B), c(0x000000, .05f) };
    }

    struct FontKey { float size; int weight; int kind; bool operator==(const FontKey& o) const { return size == o.size && weight == o.weight && kind == o.kind; } };
    struct FontHash { size_t operator()(const FontKey& k) const { return std::hash<float>()(k.size) ^ (k.weight << 8) ^ (k.kind << 20); } };

    IDWriteTextFormat* Font(float size, DWRITE_FONT_WEIGHT weight, bool display, bool icons) {
        static std::unordered_map<FontKey, IDWriteTextFormat*, FontHash> cache;
        FontKey k{ size, (int)weight, icons ? 2 : display ? 1 : 0 };
        auto it = cache.find(k);
        if (it != cache.end()) return it->second;
        IDWriteTextFormat* f = nullptr;
        auto& fam = icons ? iconFamily : display ? displayFamily : textFamily;
        DW->CreateTextFormat(fam.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L10n::ru ? L"ru-ru" : L"en-us", &f);
        cache[k] = f;
        return f;
    }

    ID2D1Bitmap* BitmapFromIcon(ID2D1RenderTarget* rt, HICON icon) {
        if (!icon) return nullptr;
        IWICBitmap* wb = nullptr; IWICFormatConverter* conv = nullptr; ID2D1Bitmap* out = nullptr;
        if (SUCCEEDED(WIC->CreateBitmapFromHICON(icon, &wb)) && SUCCEEDED(WIC->CreateFormatConverter(&conv)) &&
            SUCCEEDED(conv->Initialize(wb, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
            rt->CreateBitmapFromWicBitmap(conv, nullptr, &out);
        if (conv) conv->Release();
        if (wb) wb->Release();
        return out;
    }

    HICON ClaudeIcon(int size) {
        HICON icon = nullptr; UINT id = 0;
        PrivateExtractIconsW(Core::App.exe.c_str(), 0, size, size, &icon, &id, 1, 0);
        return icon;
    }
}

using namespace Gfx;

// ============================================================ window plumbing

Surface::~Surface() {
    if (rt) rt->Release();
    if (editFont) DeleteObject(editFont);
    if (editBrush) DeleteObject(editBrush);
    if (hwnd) { SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); DestroyWindow(hwnd); }
}

// CPS_SCALE=2 in the demo renders every window at that scale whatever the monitor's, so
// screenshots come out sharp on high-DPI screens. Never applies to the real switcher.
static float ForcedScale() {
    static float s = [] { wchar_t b[8] = {}; GetEnvironmentVariableW(L"CPS_SCALE", b, 8); float v = (float)_wtof(b); return Paths::Demo && v >= 1 && v <= 4 ? v : 0.f; }();
    return s;
}
static float WindowDpi(HWND h) { return ForcedScale() ? 96.f * ForcedScale() : (float)GetDpiForWindow(h); }

bool Surface::Create(const wchar_t* title, DWORD style, DWORD exStyle, float w, float h, HWND owner, bool noFrame) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{ sizeof wc };
        wc.lpfnWndProc = Proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"ClaudeSwitcherSurface";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
        wc.hIconSm = (HICON)LoadImageW(wc.hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        RegisterClassExW(&wc);
        registered = true;
    }
    borderless = noFrame;
    width = w;
    // WS_CLIPCHILDREN: the drawing must not paint over child windows such as the text box.
    style |= WS_CLIPCHILDREN;
    hwnd = CreateWindowExW(exStyle, L"ClaudeSwitcherSurface", title, style, CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, owner, nullptr, GetModuleHandleW(nullptr), this);
    if (!hwnd) return false;
    dpi = WindowDpi(hwnd); scale = dpi / 96.f;
    RECT rc{ 0, 0, (LONG)std::lround(w * scale), (LONG)std::lround(h * scale) };
    if (!borderless) AdjustWindowRectExForDpi(&rc, style, FALSE, exStyle, GetDpiForWindow(hwnd));   // the frame is drawn at the real DPI
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    ApplyBackdrop();
    return true;
}

void Surface::ApplyBackdrop() {
    BOOL dark = T.dark;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
    int round = 2;
    DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &round, sizeof round);
    // Mica takes its tint from the system theme, so a forced theme gets a plain background.
    // Screenshots (CPS_SCALE) get the plain fill: PrintWindow cannot capture the backdrop.
    backdropOn = BackdropSupported() && T.dark == SystemDark() && !ForcedScale();
    MARGINS m = backdropOn ? MARGINS{ -1, -1, -1, -1 } : MARGINS{ 0, 0, 0, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &m);
    if (BackdropSupported()) {
        int type = backdropOn ? backdrop : 1;
        DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &type, sizeof type);
    }
}

void Surface::Restyle() { if (!hwnd) return; ApplyBackdrop(); if (editBrush) { DeleteObject(editBrush); editBrush = nullptr; } Invalidate(); }

void Surface::Show(bool activate) {
    ShowWindow(hwnd, activate ? SW_SHOW : SW_SHOWNOACTIVATE);
    if (activate) { SetForegroundWindow(hwnd); SetFocus(hwnd); }
    OnShown();
    Invalidate();
}

void Surface::Hide() { if (edit) CloseEdit(false); ShowWindow(hwnd, SW_HIDE); hot = pressed = 0; }

void Surface::EnsureTarget() {
    RECT rc; GetClientRect(hwnd, &rc);
    if (!rt) {
        // Software rendering: no GPU device to bring up, so the first frame lands in tens of
        // milliseconds, and a window this size draws in well under one.
        auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                     D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
        D2D->CreateHwndRenderTarget(props, D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(rc.right, rc.bottom)), &rt);
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    } else {
        auto s = rt->GetPixelSize();
        if (s.width != (UINT32)rc.right || s.height != (UINT32)rc.bottom) rt->Resize(D2D1::SizeU(rc.right, rc.bottom));
        rt->SetDpi(dpi, dpi);
    }
}

void Surface::Render() {
    EnsureTarget();
    LARGE_INTEGER now, freq; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&freq);
    // The first frame after a quiet spell counts as one ordinary frame, so a hover that
    // starts from rest eases in instead of jumping.
    frameDt = lastFrame.QuadPart ? (float)((now.QuadPart - lastFrame.QuadPart) * 1000.0 / freq.QuadPart) : 16.f;
    if (frameDt > 100) frameDt = 16;
    lastFrame = now;
    animating = false;
    building.clear();
    rt->BeginDraw();
    rt->SetTransform(D2D1::Matrix3x2F::Identity());
    rt->Clear(backdropOn ? D2D1::ColorF(0, 0, 0, 0) : (borderless ? T.menu : T.bg));
    Build();
    if (rt->EndDraw() == D2DERR_RECREATE_TARGET) { rt->Release(); rt = nullptr; }
    regions.swap(building);

    if (animating && !timerOn) { SetTimer(hwnd, 1, 8, nullptr); timerOn = true; }
    if (!animating && timerOn) { KillTimer(hwnd, 1); timerOn = false; lastFrame.QuadPart = 0; }

    if (sizeToContent && contentHeight > 0) {
        RECT rc; GetClientRect(hwnd, &rc);
        int want = (int)std::lround(contentHeight * scale);
        if (want != rc.bottom) {
            RECT wr{ 0, 0, rc.right, want };
            DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE), ex = (DWORD)GetWindowLongW(hwnd, GWL_EXSTYLE);
            if (!borderless) AdjustWindowRectExForDpi(&wr, style, FALSE, ex, GetDpiForWindow(hwnd));
            SetWindowPos(hwnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            Invalidate();
        }
    }
}

const Region* Surface::At(float x, float y) const {
    for (auto it = regions.rbegin(); it != regions.rend(); ++it) if (it->r.Has(x, y)) return &*it;
    return nullptr;
}

void Surface::MoveFocus(int dir) {
    std::vector<UINT> ids;
    for (auto& r : regions) if (r.focusable) ids.push_back(r.id);
    if (ids.empty()) return;
    int i = -1;
    for (int k = 0; k < (int)ids.size(); k++) if (ids[k] == focus) i = k;
    i = i < 0 ? (dir > 0 ? 0 : (int)ids.size() - 1) : (i + dir + (int)ids.size()) % (int)ids.size();
    focus = ids[i]; keyboardCues = true; Invalidate();
}

LRESULT CALLBACK Surface::Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams);
    auto self = (Surface*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!self) return DefWindowProcW(h, m, w, l);
    if (m == WM_NCCREATE) self->hwnd = h;
    return self->Handle(m, w, l);
}

LRESULT Surface::Handle(UINT m, WPARAM w, LPARAM l) {
    bool handled = false;
    LRESULT r = Extra(m, w, l, handled);
    if (handled) return r;
    switch (m) {
    case WM_NCCALCSIZE:
        if (borderless && w) return 0;   // keep the DWM shadow and rounded corners, lose the frame
        break;
    case WM_NCHITTEST:
        if (borderless) return HTCLIENT;
        break;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(hwnd, &ps); Render(); EndPaint(hwnd, &ps); return 0; }
    case WM_SIZE: Invalidate(); return 0;
    case WM_TIMER: if (w == 1) Invalidate(); return 0;
    case WM_DPICHANGED: {
        if (ForcedScale()) return 0;
        dpi = (float)HIWORD(w); scale = dpi / 96.f;
        auto rc = (RECT*)l;
        SetWindowPos(hwnd, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, SWP_NOZORDER | SWP_NOACTIVATE);
        if (editFont) { DeleteObject(editFont); editFont = nullptr; }
        Invalidate(); return 0;
    }
    case WM_MOUSEMOVE: {
        mx = GET_X_LPARAM(l) / scale; my = GET_Y_LPARAM(l) / scale;
        TRACKMOUSEEVENT t{ sizeof t, TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&t);
        auto hit = At(mx, my); UINT id = hit ? hit->id : 0;
        if (id != hot) { hot = id; Invalidate(); }
        return 0;
    }
    case WM_MOUSELEAVE: mx = my = -1; if (hot) { hot = 0; Invalidate(); } return 0;
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT) {
            auto hit = At(mx, my);
            SetCursor(LoadCursorW(nullptr, hit && hit->hand ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_MOUSEWHEEL:
        if (scrollMax > 0) {
            scrollY = std::max(0.f, std::min(scrollMax, scrollY - GET_WHEEL_DELTA_WPARAM(w) / 120.f * 48.f));
            Invalidate(); return 0;
        }
        break;
    case WM_LBUTTONDOWN: {
        mx = GET_X_LPARAM(l) / scale; my = GET_Y_LPARAM(l) / scale;   // a click may arrive without a move first (touch, pen)
        if (edit) CloseEdit(true);
        OnMouseDown();
        keyboardCues = false;
        auto hit = At(mx, my);
        pressed = hit ? hit->id : 0;
        if (hit && hit->focusable) focus = hit->id;
        SetCapture(hwnd); Invalidate(); return 0;
    }
    case WM_LBUTTONUP: {
        ReleaseCapture();
        auto hit = At(GET_X_LPARAM(l) / scale, GET_Y_LPARAM(l) / scale);
        UINT was = pressed; pressed = 0; Invalidate();
        if (hit && hit->id == was && hit->click) { auto fn = hit->click; fn(); }
        return 0;
    }
    case WM_RBUTTONUP: {
        OnMouseDown();
        auto hit = At(GET_X_LPARAM(l) / scale, GET_Y_LPARAM(l) / scale);
        if (hit && hit->rclick) { auto fn = hit->rclick; fn(); }
        return 0;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        if (OnKey((UINT)w, alt)) return 0;
        if (w == VK_TAB) { MoveFocus(GetKeyState(VK_SHIFT) & 0x8000 ? -1 : 1); return 0; }
        if (w == VK_RIGHT || w == VK_DOWN) { MoveFocus(1); return 0; }
        if (w == VK_LEFT || w == VK_UP) { MoveFocus(-1); return 0; }
        if (w == VK_RETURN || w == VK_SPACE) {
            for (auto& rg : regions) if (rg.id == focus && rg.click) { auto fn = rg.click; fn(); return 0; }
        }
        if (w == VK_ESCAPE) { OnClose(); return 0; }
        break;
    }
    case WM_ACTIVATE: if (LOWORD(w) == WA_INACTIVE) OnDeactivate(); Invalidate(); return 0;
    case WM_CLOSE: OnClose(); return 0;
    case WM_CTLCOLOREDIT: {
        auto dc = (HDC)w;
        auto c = T.menu; auto t = T.text;
        SetTextColor(dc, RGB((int)(t.r * 255), (int)(t.g * 255), (int)(t.b * 255)));
        SetBkColor(dc, RGB((int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255)));
        if (!editBrush) editBrush = CreateSolidBrush(RGB((int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255)));
        return (LRESULT)editBrush;
    }
    case WM_APP + 50: CloseEdit(w != 0); return 0;
    case WM_APP + 52: if (edit) { SetFocus(edit); SendMessageW(edit, EM_SETSEL, 0, -1); } return 0;
    }
    return DefWindowProcW(hwnd, m, w, l);
}

// ============================================================ drawing helpers

float Surface::A(UINT id, float target, float ms) {
    auto it = anims.find(id);
    if (it == anims.end()) { anims[id] = { target, target }; return target; }
    auto& v = it->second;
    v.second = target;
    if (v.first != target) {
        // Exponential approach: smooth, and a change of mind mid-way simply heads elsewhere.
        float k = 1 - std::exp(-frameDt / (ms / 3.f));
        v.first += (target - v.first) * k;
        if (std::fabs(target - v.first) < 0.003f) v.first = target; else animating = true;
    }
    return v.first;
}

static ID2D1SolidColorBrush* Brush(ID2D1RenderTarget* rt, D2D1_COLOR_F c) {
    static ID2D1SolidColorBrush* b = nullptr; static ID2D1RenderTarget* owner = nullptr;
    if (owner != rt) { if (b) b->Release(); b = nullptr; owner = rt; }
    if (!b) rt->CreateSolidColorBrush(c, &b); else b->SetColor(c);
    return b;
}

void Surface::Fill(RectF r, D2D1_COLOR_F c, float radius) {
    if (c.a <= 0) return;
    if (radius > 0) rt->FillRoundedRectangle(D2D1::RoundedRect(r.D(), radius, radius), Brush(rt, c));
    else rt->FillRectangle(r.D(), Brush(rt, c));
}

void Surface::Stroke(RectF r, D2D1_COLOR_F c, float radius, float width) {
    if (c.a <= 0) return;
    auto d = r.Inset(width / 2).D();
    if (radius > 0) rt->DrawRoundedRectangle(D2D1::RoundedRect(d, radius - width / 2, radius - width / 2), Brush(rt, c), width);
    else rt->DrawRectangle(d, Brush(rt, c), width);
}

void Surface::Circle(float cx, float cy, float rad, D2D1_COLOR_F c) { rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), Brush(rt, c)); }
void Surface::Ring(float cx, float cy, float rad, D2D1_COLOR_F c, float w) { rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad - w / 2, rad - w / 2), Brush(rt, c), w); }
void Surface::Line(float x1, float y1, float x2, float y2, D2D1_COLOR_F c, float w) { rt->DrawLine({ x1, y1 }, { x2, y2 }, Brush(rt, c), w); }

static IDWriteTextLayout* Layout(const std::wstring& s, RectF r, IDWriteTextFormat* f, Align a, bool vcenter, bool wrap) {
    IDWriteTextLayout* l = nullptr;
    DW->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, std::max(1.f, r.w), std::max(1.f, r.h), &l);
    if (!l) return nullptr;
    l->SetTextAlignment(a == Align::Left ? DWRITE_TEXT_ALIGNMENT_LEADING : a == Align::Center ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_TRAILING);
    l->SetParagraphAlignment(vcenter ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    l->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (!wrap) {
        IDWriteInlineObject* sign = nullptr; DW->CreateEllipsisTrimmingSign(f, &sign);
        DWRITE_TRIMMING t{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        l->SetTrimming(&t, sign);
        if (sign) sign->Release();
    }
    return l;
}

void Surface::Text(const std::wstring& s, RectF r, IDWriteTextFormat* f, D2D1_COLOR_F c, Align a, bool vcenter, bool wrap) {
    if (s.empty() || c.a <= 0) return;
    auto l = Layout(s, r, f, a, vcenter, wrap);
    if (!l) return;
    rt->DrawTextLayout({ r.x, r.y }, l, Brush(rt, c), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    l->Release();
}

void Surface::Glyph(const wchar_t* g, RectF r, float size, D2D1_COLOR_F c) { Text(g, r, Font(size, DWRITE_FONT_WEIGHT_NORMAL, false, true), c, Align::Center); }

D2D1_SIZE_F Surface::Measure(const std::wstring& s, IDWriteTextFormat* f, float maxW, bool wrap) {
    auto l = Layout(s, { 0, 0, maxW, 10000 }, f, Align::Left, false, wrap);
    DWRITE_TEXT_METRICS m{}; if (l) { l->GetMetrics(&m); l->Release(); }
    return { m.widthIncludingTrailingWhitespace, m.height };
}

void Surface::Image(ID2D1Bitmap* b, RectF r, float opacity) { if (b) rt->DrawBitmap(b, r.D(), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR); }

void Surface::Hit(UINT id, RectF r, std::function<void()> click, std::function<void()> rclick, bool focusable, bool hand) {
    building.push_back({ id, r, std::move(click), std::move(rclick), focusable, hand });
}

void Surface::FocusRing(UINT id, RectF r, float radius) {
    if (keyboardCues && focus == id && GetForegroundWindow() == hwnd) Stroke({ r.x - 3, r.y - 3, r.w + 6, r.h + 6 }, T.focus, radius + 3, 2);
}

void Surface::DashedStroke(RectF r, D2D1_COLOR_F c, float radius) {
    static ID2D1StrokeStyle* dash = nullptr;
    if (!dash) {
        float d[] = { 4, 3 };
        D2D->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_MITER, 10, D2D1_DASH_STYLE_CUSTOM, 0), d, 2, &dash);
    }
    auto i = r.Inset(.6f).D();
    rt->DrawRoundedRectangle(D2D1::RoundedRect(i, radius, radius), Brush(rt, c), 1.2f, dash);
}

void Surface::PushOpacity(float o) {
    rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), o), nullptr);
}
void Surface::PopOpacity() { rt->PopLayer(); }
void Surface::Translate(float dx, float dy) { rt->SetTransform(D2D1::Matrix3x2F::Translation(dx, dy)); }

void Surface::RefreshHover() {
    POINT p; GetCursorPos(&p); ScreenToClient(hwnd, &p);
    RECT rc; GetClientRect(hwnd, &rc);
    if (PtInRect(&rc, p)) { mx = p.x / scale; my = p.y / scale; } else mx = my = -1;
    auto hit = At(mx, my);
    hot = hit ? hit->id : 0; pressed = 0;
    Invalidate();
}

RectF Surface::Client() const { RECT rc; GetClientRect(hwnd, &rc); return { 0, 0, rc.right / scale, rc.bottom / scale }; }

// ============================================================ controls

bool Surface::Button(UINT id, RectF r, const std::wstring& label, bool primary, std::function<void()> click, bool danger, bool enabled) {
    float h = enabled ? HotT(id) : 0, p = enabled ? PressT(id) : 0;
    auto f = Font(14, primary ? DWRITE_FONT_WEIGHT_SEMI_BOLD : DWRITE_FONT_WEIGHT_NORMAL);
    D2D1_COLOR_F bg, fg;
    if (danger) { bg = Mix(Mix(Rgba(0xC42B1C), Rgba(0xD13A2B), h), Rgba(0xA82518), p); fg = Rgba(0xFFFFFF); }
    else if (primary) { bg = Mix(Mix(T.accent, T.accentHover, h), T.accentPress, p); fg = T.onAccent; }
    else { bg = Mix(Mix(T.control, T.controlHover, h), T.controlPress, p); fg = Mix(T.text, T.text2, p); }
    if (!enabled) { bg.a *= .45f; fg.a *= .45f; }
    Fill(r, bg, 4);
    if (!primary && !danger) Stroke(r, T.controlStroke, 4);
    Text(label, r.Inset(10, 0), f, fg, Align::Center);
    if (enabled) { Hit(id, r, click); FocusRing(id, r, 4); }
    return true;
}

void Surface::IconButton(UINT id, RectF r, const wchar_t* glyph, std::function<void()> click) {
    float h = HotT(id), p = PressT(id);
    Fill(r, Mix(Mix(D2D1::ColorF(0, 0, 0, 0), T.layerHover, h), T.layerPress, p), 4);
    Glyph(glyph, r, 16, Mix(T.text2, T.text, h));
    Hit(id, r, click);
    FocusRing(id, r, 4);
}

void Surface::Toggle(UINT id, float x, float y, bool on, std::function<void()> click, bool enabled) {
    RectF track{ x, y, 40, 20 };
    float t = A(id ^ 0x99, on ? 1.f : 0.f, 160), h = enabled ? HotT(id) : 0;
    auto off = Mix(T.control, T.controlHover, h), onc = Mix(T.accent, T.accentHover, h);
    auto bg = Mix(off, onc, t);
    if (!enabled) bg.a *= .45f;
    Fill(track, bg, 10);
    if (t < 1) { auto s = T.controlStrong; s.a *= (1 - t) * (enabled ? 1 : .45f); Stroke(track, s, 10); }
    float knob = 12 + 2 * h, cx = x + 10 + 20 * t;
    auto kc = Mix(T.text2, T.onAccent, t); if (!enabled) kc.a *= .45f;
    Circle(cx, y + 10, knob / 2, kc);
    if (enabled) { Hit(id, { x - 4, y - 6, 48, 32 }, click); FocusRing(id, track, 10); }
}

void Surface::Check(UINT id, float x, float y, bool on, D2D1_COLOR_F fill) {
    float t = A(id ^ 0x3C, on ? 1.f : 0.f, 100);
    RectF b{ x, y, 20, 20 };
    Fill(b, Mix(fill, T.accent, t), 4);
    if (t < 1) { auto s = T.controlStrong; s.a *= 1 - t; Stroke(b, s, 4); }
    if (t > 0) { auto c = T.onAccent; c.a = t; Glyph(Glyphs::Check, b, 12, c); }
}

void Surface::Avatar(float cx, float cy, float d, const std::wstring& letters, COLORREF color, bool claude, bool running, D2D1_COLOR_F ringBg) {
    if (claude) {
        static std::unordered_map<ID2D1RenderTarget*, ID2D1Bitmap*> cache;
        auto& bmp = cache[rt];
        if (!bmp) { HICON i = ClaudeIcon(64); bmp = BitmapFromIcon(rt, i); if (i) DestroyIcon(i); }
        Image(bmp, { cx - d / 2, cy - d / 2, d, d });
    } else {
        // Top-lit, like the badges on the shortcut icons.
        auto base = FromColorref(color);
        D2D1_GRADIENT_STOP stops[] = { { 0, Mix(base, Rgba(0xFFFFFF), .20f) }, { 1, Mix(base, Rgba(0x000000), .08f) } };
        ID2D1GradientStopCollection* coll = nullptr; ID2D1LinearGradientBrush* grad = nullptr;
        rt->CreateGradientStopCollection(stops, 2, &coll);
        if (coll) rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ 0, cy - d / 2 }, { 0, cy + d / 2 }), coll, &grad);
        if (grad) { rt->FillEllipse(D2D1::Ellipse({ cx, cy }, d / 2, d / 2), grad); grad->Release(); }
        else Circle(cx, cy, d / 2, base);
        if (coll) coll->Release();
        auto f = Font(d * (letters.size() > 1 ? .36f : .42f), DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
        Text(letters, { cx - d / 2, cy - d / 2, d, d }, f, Rgba(0xFFFFFF), Align::Center);
    }
    if (running) {
        float r = d * .15f, ox = cx + d * .35f, oy = cy + d * .35f;
        Circle(ox, oy, r + 2.5f, ringBg);
        Circle(ox, oy, r, T.success);
    }
}

void Surface::Bar(RectF r, double pct, bool stale) {
    Fill(r, T.badge, r.h / 2);
    if (pct < 0) return;
    auto c = pct >= 90 ? T.danger : pct >= 70 ? T.warning : T.success;
    if (stale) c.a *= .45f;   // an old reading: still useful, but not to be read as now
    float w = (float)(r.w * std::min(100.0, std::max(0.0, pct)) / 100);
    if (w > 0) Fill({ r.x, r.y, std::max(w, r.h), r.h }, c, r.h / 2);
}

// ============================================================ text box

void Surface::ShowEdit(RectF r, const std::wstring& text, std::function<void(bool, const std::wstring&)> done) {
    if (edit) CloseEdit(true);
    editDone = std::move(done);
    if (!editFont) editFont = CreateFontW(-(int)std::lround(13 * scale), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI Variable Text");
    // A layered child, fully opaque: plain GDI text drawn on Mica would otherwise come out
    // see-through, and the box would look empty.
    edit = CreateWindowExW(WS_EX_LAYERED, L"EDIT", text.c_str(), WS_CHILD | WS_VISIBLE | ES_CENTER | ES_AUTOHSCROLL,
                           (int)(r.x * scale), (int)(r.y * scale), (int)(r.w * scale), (int)(r.h * scale), hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetLayeredWindowAttributes(edit, 0, 255, LWA_ALPHA);
    editSince = GetTickCount64();
    SendMessageW(edit, WM_SETFONT, (WPARAM)editFont, TRUE);
    SendMessageW(edit, EM_SETLIMITTEXT, 40, 0);
    SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(4, 4));
    SendMessageW(edit, EM_SETSEL, 0, -1);
    SetWindowSubclass(edit, EditProc, 1, (DWORD_PTR)this);
    SetFocus(edit);
}

void Surface::CloseEdit(bool commit) {
    if (!edit) return;
    int n = GetWindowTextLengthW(edit);
    std::wstring s(n, L'\0'); GetWindowTextW(edit, &s[0], n + 1);
    HWND e = edit; edit = nullptr;
    RemoveWindowSubclass(e, EditProc, 1);
    DestroyWindow(e);
    SetFocus(hwnd);
    auto fn = std::move(editDone); editDone = nullptr;
    if (fn) fn(commit, s);
    Invalidate();
}

LRESULT CALLBACK Surface::EditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR ref) {
    auto self = (Surface*)ref;
    if (m == WM_KEYDOWN && (w == VK_RETURN || w == VK_ESCAPE)) { PostMessageW(self->hwnd, WM_APP + 50, w == VK_RETURN, 0); return 0; }
    if (m == WM_CHAR && (w == VK_RETURN || w == VK_ESCAPE)) return 0;
    if (m == WM_KILLFOCUS) {
        // Clicking elsewhere finishes the edit. A focus change in the first moments is the
        // system settling focus after a menu, so the box takes focus back instead.
        if (GetTickCount64() - self->editSince < 400) { PostMessageW(self->hwnd, WM_APP + 52, 0, 0); return DefSubclassProc(h, m, w, l); }
        PostMessageW(self->hwnd, WM_APP + 50, 1, 0);
    }
    return DefSubclassProc(h, m, w, l);
}
