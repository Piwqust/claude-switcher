// Drawing and the small immediate-mode UI layer every window is built on.
//
// Each window draws itself in Build(): while drawing, interactive parts register a
// hit region with a click handler. Hover, press and focus are tracked by region id and
// eased over ~100 ms, the way Windows 11 controls fade between states. Text is DirectWrite
// with Segoe UI Variable, the same text stack and font the system's own UI uses.
#pragma once
#include "core.h"
#include <d2d1_1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <unordered_map>

namespace Gfx {
    extern ID2D1Factory* D2D;
    extern IDWriteFactory3* DW;
    extern IWICImagingFactory* WIC;
    void Init();

    inline D2D1_COLOR_F Rgba(UINT rgb, float a = 1) { return D2D1::ColorF(((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, a); }
    inline D2D1_COLOR_F FromColorref(COLORREF c, float a = 1) { return D2D1::ColorF(GetRValue(c) / 255.f, GetGValue(c) / 255.f, GetBValue(c) / 255.f, a); }
    D2D1_COLOR_F Mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float t);

    // Windows 11 palettes: translucent layers over Mica, Claude's coral as the accent.
    struct Theme {
        bool dark;
        D2D1_COLOR_F bg, text, text2, text3, layer, layerHover, layerPress, layerStroke, card, cardStroke,
            control, controlHover, controlPress, controlStroke, controlStrong, menu, menuStroke,
            accent, accentHover, accentPress, accentText, onAccent, accentTint, success, warning, danger, focus, badge;
    };
    extern Theme T;
    bool SystemDark();
    void ApplyTheme();                     // re-reads the system setting
    bool BackdropSupported();

    IDWriteTextFormat* Font(float size, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL, bool display = false, bool icons = false);

    ID2D1Bitmap* BitmapFromIcon(ID2D1RenderTarget* rt, HICON icon);
    HICON ClaudeIcon(int size);           // Claude's own icon, from the installed app
}

struct RectF {
    float x = 0, y = 0, w = 0, h = 0;
    float R() const { return x + w; }
    float B() const { return y + h; }
    bool Has(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    RectF Inset(float d) const { return { x + d, y + d, w - 2 * d, h - 2 * d }; }
    RectF Inset(float dx, float dy) const { return { x + dx, y + dy, w - 2 * dx, h - 2 * dy }; }
    D2D1_RECT_F D() const { return D2D1::RectF(x, y, x + w, y + h); }
};

enum class Align { Left, Center, Right };

struct Region {
    UINT id; RectF r;
    std::function<void()> click, rclick;
    bool focusable; bool hand;
};

inline UINT Id(const char* s, int i = 0) { UINT h = 2166136261u; while (*s) h = (h ^ (BYTE)*s++) * 16777619u; return (h ^ (UINT)i) * 16777619u | 1; }

class Surface {
public:
    HWND hwnd = nullptr;
    float dpi = 96, scale = 1;
    bool backdropOn = false;
    int backdrop = 2;                      // DWMSBT_MAINWINDOW (Mica) or 3 (Acrylic)
    float contentHeight = 0;               // set by Build; windows that size to content follow it
    bool sizeToContent = false;
    float width = 600;                     // DIPs
    float scrollY = 0, scrollMax = 0;
    bool keyboardCues = false;
    UINT hot = 0, pressed = 0, focus = 0;

    virtual ~Surface();
    bool Create(const wchar_t* title, DWORD style, DWORD exStyle, float w, float h, HWND owner = nullptr, bool borderless = false);
    void Show(bool activate = true);
    void Hide();
    bool Visible() const { return hwnd && IsWindowVisible(hwnd); }
    void Invalidate() { if (hwnd) InvalidateRect(hwnd, nullptr, FALSE); }
    void Restyle();                        // theme changed

    // ---- used while building
    float A(UINT id, float target, float ms = 110);
    float HotT(UINT id) { return A(id ^ 0x51, hot == id ? 1.f : 0.f); }
    float PressT(UINT id) { return A(id ^ 0x73, pressed == id && hot == id ? 1.f : 0.f, 60); }
    void Fill(RectF r, D2D1_COLOR_F c, float radius = 0);
    void Stroke(RectF r, D2D1_COLOR_F c, float radius = 0, float width = 1);
    void Circle(float cx, float cy, float rad, D2D1_COLOR_F c);
    void Ring(float cx, float cy, float rad, D2D1_COLOR_F c, float width);
    void Line(float x1, float y1, float x2, float y2, D2D1_COLOR_F c, float width = 1);
    void Text(const std::wstring& s, RectF r, IDWriteTextFormat* f, D2D1_COLOR_F c, Align a = Align::Left, bool vcenter = true, bool wrap = false);
    void Glyph(const wchar_t* g, RectF r, float size, D2D1_COLOR_F c);
    D2D1_SIZE_F Measure(const std::wstring& s, IDWriteTextFormat* f, float maxW = 10000, bool wrap = false);
    void Image(ID2D1Bitmap* b, RectF r, float opacity = 1);
    void Hit(UINT id, RectF r, std::function<void()> click, std::function<void()> rclick = nullptr, bool focusable = true, bool hand = true);
    void FocusRing(UINT id, RectF r, float radius);
    void DashedStroke(RectF r, D2D1_COLOR_F c, float radius);
    void PushOpacity(float o);
    void PopOpacity();
    void Translate(float dx, float dy);
    void Animate() { animating = true; }   // keep drawing frames (countdowns and the like)
    bool Over(RectF r) const { return r.Has(mx, my); }   // pointer inside, whatever region it is on
    void RefreshHover();                   // re-read the pointer, after a menu or dialog
    RectF Client() const;
    float FrameMs() const { return frameDt; }
    ID2D1RenderTarget* RT() { return rt; }

    // ---- common controls drawn in the Windows 11 style
    bool Button(UINT id, RectF r, const std::wstring& label, bool primary, std::function<void()> click, bool danger = false, bool enabled = true);
    void IconButton(UINT id, RectF r, const wchar_t* glyph, std::function<void()> click);
    void Toggle(UINT id, float x, float y, bool on, std::function<void()> click, bool enabled = true);
    void Check(UINT id, float x, float y, bool on, D2D1_COLOR_F fill);
    void Avatar(float cx, float cy, float d, const std::wstring& letters, COLORREF color, bool claude, bool running, D2D1_COLOR_F ringBg);
    void Bar(RectF r, double percent, bool stale = false);   // usage meter

    // Native text box over the drawing, for renaming.
    void ShowEdit(RectF r, const std::wstring& text, std::function<void(bool commit, const std::wstring&)> done);
    void CloseEdit(bool commit);
    bool Editing() const { return edit != nullptr; }

protected:
    virtual void Build() = 0;
    virtual bool OnKey(UINT vk, bool alt) { return false; }
    virtual void OnClose() { Hide(); }
    virtual void OnMouseDown() {}
    virtual void OnDeactivate() {}
    virtual void OnShown() {}
    virtual LRESULT Extra(UINT m, WPARAM w, LPARAM l, bool& handled) { return 0; }

    float mx = -1, my = -1;
    bool borderless = false;

private:
    ID2D1HwndRenderTarget* rt = nullptr;
    std::vector<Region> regions, building;
    std::unordered_map<UINT, std::pair<float, float>> anims;
    LARGE_INTEGER lastFrame{};
    float frameDt = 0;
    bool animating = false, timerOn = false;
    HWND edit = nullptr; HFONT editFont = nullptr; HBRUSH editBrush = nullptr; ULONGLONG editSince = 0;
    std::function<void(bool, const std::wstring&)> editDone;

    void Render();
    void EnsureTarget();
    const Region* At(float x, float y) const;
    void MoveFocus(int dir);
    void ApplyBackdrop();
    static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l);
    static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref);
    LRESULT Handle(UINT m, WPARAM w, LPARAM l);
};

namespace Glyphs {
    constexpr const wchar_t* Settings = L"";
    constexpr const wchar_t* More = L"";
    constexpr const wchar_t* Add = L"";
    constexpr const wchar_t* Check = L"";
    constexpr const wchar_t* Timer = L"";
    constexpr const wchar_t* Contact = L"";
    constexpr const wchar_t* Open = L"";
    constexpr const wchar_t* Power = L"";
    constexpr const wchar_t* Pin = L"";
    constexpr const wchar_t* Clear = L"";
    constexpr const wchar_t* Keyboard = L"";
    constexpr const wchar_t* Restart = L"";
}
