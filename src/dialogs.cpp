#include "app.h"
#include <set>

using namespace Gfx;

namespace {
    std::set<Surface*> open;

    // Runs a window modally: the owner is disabled until the dialog is done.
    struct Modal : Surface {
        bool done = false;
        int result = 0;
        HWND owner = nullptr;

        void Run(const wchar_t* title, float w, HWND own) {
            owner = own && IsWindowVisible(own) ? own : nullptr;
            sizeToContent = true; backdrop = 2;
            Create(title, WS_POPUP | WS_CAPTION | WS_SYSMENU, owner ? 0 : WS_EX_TOPMOST, w, 200, owner);
            open.insert(this);
            InvalidateRect(hwnd, nullptr, FALSE); UpdateWindow(hwnd);
            RECT wr; GetWindowRect(hwnd, &wr);
            RECT ref;
            if (owner) GetWindowRect(owner, &ref);
            else { POINT pt; GetCursorPos(&pt); MONITORINFO mi{ sizeof mi }; GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi); ref = mi.rcWork; }
            int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
            SetWindowPos(hwnd, nullptr, (ref.left + ref.right - ww) / 2, (ref.top + ref.bottom - wh) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            if (owner) EnableWindow(owner, FALSE);
            Show(true);
            MSG m;
            while (!done && GetMessageW(&m, nullptr, 0, 0)) {
                if (m.message == WM_QUIT) { PostQuitMessage((int)m.wParam); break; }
                TranslateMessage(&m); DispatchMessageW(&m);
            }
            if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
            open.erase(this);
            ShowWindow(hwnd, SW_HIDE);
        }
        void End(int r) { result = r; done = true; }
        void OnClose() override { End(0); }

        // ContentDialog layout: body on the window, buttons on a darker band at the bottom.
        float Footer(float y, const std::vector<std::pair<std::wstring, int>>& buttons, bool danger, const std::wstring& note = L"") {
            float w = Client().w;
            Fill({ 0, y, w, 80 }, T.layer);
            Line(0, y, w, y, T.layerStroke);
            float bx = w - 24;
            for (int i = (int)buttons.size() - 1; i >= 0; i--) {
                float bw = std::max(120.f, Measure(buttons[i].first, Font(14, DWRITE_FONT_WEIGHT_SEMI_BOLD)).width + 32);
                bx -= bw;
                int v = buttons[i].second;
                Button(Id("dlg", i), { bx, y + 24, bw, 32 }, buttons[i].first, i == 0 && !danger, [this, v] { End(v); }, i == 0 && danger);
                bx -= 8;
            }
            if (!note.empty()) Text(note, { 24, y + 12, bx - 40, 56 }, Font(12), T.text2, Align::Left, true, true);
            return 80;
        }
    };

    struct AskDialog : Modal {
        std::wstring title, text, yes, no; bool danger = false;
        void Build() override {
            float w = Client().w, y = 32;
            auto th = Measure(title, Font(20, DWRITE_FONT_WEIGHT_SEMI_BOLD, true), w - 48, true).height;
            Text(title, { 24, y, w - 48, th }, Font(20, DWRITE_FONT_WEIGHT_SEMI_BOLD, true), T.text, Align::Left, false, true);
            y += th + 10;
            if (!text.empty()) {
                auto h = Measure(text, Font(14), w - 48, true).height;
                Text(text, { 24, y, w - 48, h }, Font(14), T.text2, Align::Left, false, true);
                y += h;
            }
            y += 24;
            std::vector<std::pair<std::wstring, int>> b{ { yes, 1 } };
            if (!no.empty()) b.push_back({ no, 0 });
            y += Footer(y, b, danger);
            contentHeight = y;
            // Keyboard focus on the safe choice for destructive actions.
            if (!focus) focus = Id("dlg", danger && !no.empty() ? 1 : 0);
        }
        bool OnKey(UINT vk, bool) override {
            if (vk == VK_RETURN && !keyboardCues) { End(danger ? 0 : 1); return true; }
            return false;
        }
    };

    struct TransferDialog : Modal {
        std::wstring source;
        std::vector<CodeSession> sessions;
        std::vector<bool> picked;
        std::vector<std::wstring> targets;
        size_t target = 0;
        float listScroll = 0;

        void Build() override {
            float w = Client().w, y = 32;
            auto src = App::Find(source);
            Text(Util::Format(Tr(L"Chats in %s", L"Чаты в %s"), Util::Q(src ? src->name : source).c_str()), { 24, y, w - 48, 30 }, Font(20, DWRITE_FONT_WEIGHT_SEMI_BOLD, true), T.text);
            y += 44;
            bool all = std::find(picked.begin(), picked.end(), false) == picked.end();
            UINT allId = Id("tr-all");
            Check(allId, 24, y + 4, all, T.control);
            Text(Tr(L"Select all", L"Выбрать все"), { 54, y, 200, 28 }, Font(14), T.text);
            Hit(allId, { 20, y, 200, 28 }, [this, all] { picked.assign(picked.size(), !all); Invalidate(); });
            y += 36;

            float listH = std::min(260.f, sessions.size() * 34.f + 8);
            RectF list{ 24, y, w - 48, listH };
            Fill(list, T.card, 6); Stroke(list, T.cardStroke, 6);
            RT()->PushAxisAlignedClip(list.Inset(1).D(), D2D1_ANTIALIAS_MODE_ALIASED);
            float maxScroll = std::max(0.f, sessions.size() * 34.f + 8 - listH);
            if (listScroll > maxScroll) listScroll = maxScroll;
            for (size_t i = 0; i < sessions.size(); i++) {
                float ry = y + 4 + i * 34 - listScroll;
                if (ry + 34 < y || ry > list.B()) continue;
                UINT id = Id("tr-row", (int)i);
                RectF row{ 28, ry, w - 56, 32 };
                Fill(row, Mix(D2D1::ColorF(0, 0, 0, 0), T.layerHover, HotT(id)), 4);
                Check(id, 36, ry + 6, picked[i], T.control);
                auto& s = sessions[i];
                auto meta = (s.folder.empty() ? L"" : s.folder + L" · ") + Util::When(s.updated);
                float mw = Measure(meta, Font(12)).width;
                Text(s.title, { 66, ry, w - 56 - 66 - mw - 12, 32 }, Font(14), T.text);
                Text(meta, { w - 40 - mw, ry, mw, 32 }, Font(12), T.text3, Align::Right);
                if (ry >= y && ry + 32 <= list.B()) Hit(id, row, [this, i] { picked[i] = !picked[i]; Invalidate(); });
            }
            RT()->PopAxisAlignedClip();
            listMax = maxScroll; listRect = list;
            y += listH + 18;

            Text(Tr(L"Copy to", L"Куда"), { 24, y, 200, 20 }, Font(14, DWRITE_FONT_WEIGHT_SEMI_BOLD), T.text);
            y += 26;
            float x = 24;
            for (size_t i = 0; i < targets.size(); i++) {
                auto it = App::Find(targets[i]);
                auto name = it ? it->name : targets[i];
                float bw = Measure(name, Font(13)).width + 24;
                RectF b{ x, y, bw, 30 };
                UINT id = Id("tr-to", (int)i);
                float t = A(id ^ 0x5E, target == i ? 1.f : 0.f, 140);
                Fill(b, Mix(Mix(T.control, T.layerHover, HotT(id)), T.accent, t), 4);
                if (t < 1) Stroke(b, T.controlStroke, 4);
                Text(name, b, Font(13), Mix(T.text, T.onAccent, t), Align::Center);
                Hit(id, b, [this, i] { target = i; Invalidate(); });
                FocusRing(id, b, 4);
                x += bw + 6;
                if (x > w - 120 && i + 1 < targets.size()) { x = 24; y += 36; }
            }
            y += 30 + 24;
            y += Footer(y, { { Tr(L"Copy", L"Скопировать"), 1 }, { Tr(L"Cancel", L"Отмена"), 0 } }, false,
                        Tr(L"The chat stays here too. Carry on in one account at a time: the copies share their history", L"Чат останется и здесь. Продолжайте его в одном аккаунте: история у копий общая"));
            contentHeight = y;
        }

        float listMax = 0; RectF listRect;
        LRESULT Extra(UINT m, WPARAM w, LPARAM l, bool& handled) override {
            if (m == WM_MOUSEWHEEL && listMax > 0) {
                listScroll = std::max(0.f, std::min(listMax, listScroll - GET_WHEEL_DELTA_WPARAM(w) / 120.f * 40.f));
                Invalidate(); handled = true;
            }
            return 0;
        }
    };
}

namespace Dialogs {
    bool Ask(HWND owner, const std::wstring& title, const std::wstring& text, const std::wstring& yes, bool danger, const std::wstring& no) {
        AskDialog d; d.title = title; d.text = text; d.yes = yes; d.no = no; d.danger = danger;
        d.Run(title.c_str(), 460, owner);
        return d.result == 1;
    }

    void Info(HWND owner, const std::wstring& title, const std::wstring& text) { Ask(owner, title, text, Tr(L"OK", L"Понятно"), false, L""); }

    void Transfer(HWND owner, const std::wstring& sourceId) {
        auto src = App::Find(sourceId);
        if (!src) return;
        auto sessions = Core::CodeSessions(src->p);
        std::vector<std::wstring> targets;
        for (auto& i : App::items) if (i.p.id != sourceId) targets.push_back(i.p.id);
        if (sessions.empty()) { Info(owner, Tr(L"Nothing to move", L"Переносить нечего"), Util::Format(Tr(L"%s has no Claude Code chats yet", L"В %s пока нет чатов Claude Code"), Util::Q(src->name).c_str())); return; }
        if (targets.empty()) { Info(owner, Tr(L"Add a second account first", L"Нужен второй аккаунт"), Tr(L"Chats can be moved once there is another account to move them to", L"Добавьте ещё один аккаунт, и в него можно будет перенести чаты")); return; }
        TransferDialog d; d.source = sourceId; d.sessions = sessions; d.picked.assign(sessions.size(), false); d.targets = targets;
        d.Run(Tr(L"Move Code chats", L"Перенести чаты Code"), 600, owner);
        if (d.result != 1) return;
        std::vector<CodeSession> picked;
        for (size_t i = 0; i < sessions.size(); i++) if (d.picked[i]) picked.push_back(sessions[i]);
        if (picked.empty()) { Info(owner, Tr(L"Nothing selected", L"Ничего не выбрано"), Tr(L"Tick the chats to copy", L"Отметьте чаты, которые нужно скопировать")); return; }
        App::CopyChats(owner, picked, targets[d.target]);
    }

    void RestyleAll() { for (auto s : open) s->Restyle(); }
}
