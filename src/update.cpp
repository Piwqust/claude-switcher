// Updating in place: the latest GitHub release is checked, its exe downloaded and matched
// against the release's SHA256SUMS.txt, then run with --install, which closes this copy,
// takes its place and starts again. Claude's own windows are never touched.
#include "app.h"
#include "version.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <thread>

namespace {
    const wchar_t* ReleaseApi = L"https://api.github.com/repos/Piwqust/claude-switcher/releases/latest";
#if defined(_M_ARM64)
    const wchar_t* AssetName = L"ClaudeProfileSwitcher-arm64.exe";
#else
    const wchar_t* AssetName = L"ClaudeProfileSwitcher-x64.exe";
#endif

    Update::Phase phase = Update::Phase::Idle;
    std::wstring latest, assetUrl, sumsUrl, problem;
    ULONGLONG checkedAt = 0;   // FILETIME ticks

    struct Http {
        HINTERNET h;
        explicit Http(HINTERNET value) : h(value) {}
        ~Http() { if (h) WinHttpCloseHandle(h); }
    };

    std::wstring Wide(const std::string& s) {
        int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
        return w;
    }

    std::wstring NoConnection() { return Tr(L"Couldn't reach GitHub. Check your connection and try again", L"Не удалось связаться с GitHub. Проверьте соединение и повторите"); }

    // HTTPS GET. WinHTTP follows the redirect from github.com to its download servers by
    // itself, and never from HTTPS to HTTP.
    std::string Get(const std::wstring& url, size_t limit, bool api) {
        wchar_t host[256] = {}, path[4096] = {};
        URL_COMPONENTS u{ sizeof u };
        u.lpszHostName = host; u.dwHostNameLength = ARRAYSIZE(host);
        u.lpszUrlPath = path; u.dwUrlPathLength = ARRAYSIZE(path);
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u) || u.nScheme != INTERNET_SCHEME_HTTPS) throw NoConnection();
        Http session(WinHttpOpen(L"ClaudeProfileSwitcher/" CPS_VERSION_TEXT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.h) throw NoConnection();
        WinHttpSetTimeouts(session.h, 15000, 15000, 15000, 30000);
        Http connection(WinHttpConnect(session.h, host, u.nPort, 0));
        Http request(connection.h ? WinHttpOpenRequest(connection.h, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr);
        if (!request.h) throw NoConnection();
        const wchar_t* headers = api ? L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS;
        if (!WinHttpSendRequest(request.h, headers, api ? (DWORD)-1L : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.h, nullptr)) throw NoConnection();
        DWORD status = 0, size = sizeof status;
        WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        if (status != 200) throw Util::Format(Tr(L"GitHub answered with an error (%lu). Try again later", L"GitHub ответил ошибкой (%lu). Повторите позже"), status);
        std::string body; char buffer[16384]; DWORD got = 0;
        do {
            if (!WinHttpReadData(request.h, buffer, sizeof buffer, &got)) throw NoConnection();
            body.append(buffer, got);
            if (body.size() > limit) throw std::wstring(Tr(L"The update is larger than expected, so it wasn't installed", L"Обновление больше ожидаемого, поэтому не установлено"));
        } while (got);
        return body;
    }

    std::vector<int> Parts(std::wstring v) {
        if (!v.empty() && (v[0] == L'v' || v[0] == L'V')) v.erase(0, 1);
        std::vector<int> out;
        for (size_t s = 0; s <= v.size();) {
            auto e = v.find(L'.', s);
            out.push_back(_wtoi(v.substr(s, e == std::wstring::npos ? std::wstring::npos : e - s).c_str()));
            if (e == std::wstring::npos) break;
            s = e + 1;
        }
        return out;
    }

    bool Newer(const std::wstring& a, const std::wstring& b) {
        auto x = Parts(a), y = Parts(b);
        for (size_t i = 0; i < std::max(x.size(), y.size()); i++) {
            int p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
            if (p != q) return p > q;
        }
        return false;
    }

    std::wstring Sha256(const std::string& data) {
        BYTE hash[32];
        if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, (PUCHAR)data.data(), (ULONG)data.size(), hash, sizeof hash) < 0) throw std::wstring(Tr(L"Couldn't check the downloaded update", L"Не удалось проверить скачанное обновление"));
        std::wstring out; for (BYTE b : hash) out += Util::Format(L"%02x", b); return out;
    }

    // SHA256SUMS.txt lines read "<hash>  <file name>".
    std::wstring ExpectedHash(const std::string& sums) {
        auto text = Wide(sums);
        for (size_t s = 0; s < text.size();) {
            auto e = text.find(L'\n', s);
            auto line = text.substr(s, e == std::wstring::npos ? std::wstring::npos : e - s);
            while (!line.empty() && (line.back() == L'\r' || line.back() == L' ')) line.pop_back();
            auto sp = line.find(L' ');
            if (sp == 64 && line.size() > 64) {
                auto name = line.substr(line.find_first_not_of(L" *", sp));
                if (_wcsicmp(name.c_str(), AssetName) == 0) return Util::Lower(line.substr(0, 64));
            }
            if (e == std::wstring::npos) break;
            s = e + 1;
        }
        return L"";
    }

    std::wstring Folder() {
        wchar_t tmp[MAX_PATH + 1] = {};
        GetTempPathW(ARRAYSIZE(tmp), tmp);
        return std::wstring(tmp) + L"ClaudeProfileSwitcher-update";
    }

    void Redraw() { Flyout::Invalidate(); SettingsWin::Invalidate(); Picker::Invalidate(); }

    struct Found { std::wstring tag, asset, sums, error; };
}

namespace Update {
    const wchar_t* Version() { return L"" CPS_VERSION_TEXT; }
    Phase State() { return phase; }
    std::wstring Latest() { return latest; }
    std::wstring Problem() { return problem; }
    ULONGLONG CheckedAt() { return checkedAt; }
    bool Auto() { return Settings::Flag(L"CheckForUpdates", true); }
    void SetAuto(bool on) { Settings::Set(L"CheckForUpdates", on); if (on) Check(false); }

    void Check(bool manual) {
        if (Paths::Demo || phase == Phase::Checking || phase == Phase::Downloading || phase == Phase::Installing) return;
        auto before = phase;
        if (manual) { phase = Phase::Checking; Redraw(); }
        std::thread([manual, before] {
            auto f = std::make_shared<Found>();
            try {
                json::Value release;
                if (!json::Parser::Parse(Wide(Get(ReleaseApi, 1024 * 1024, true)), release)) throw std::wstring(Tr(L"GitHub sent an unexpected answer", L"GitHub прислал неожиданный ответ"));
                f->tag = release.Str(L"tag_name");
                if (auto assets = release.Find(L"assets"))
                    for (auto& a : assets->arr) {
                        if (_wcsicmp(a.Str(L"name").c_str(), AssetName) == 0) f->asset = a.Str(L"browser_download_url");
                        if (_wcsicmp(a.Str(L"name").c_str(), L"SHA256SUMS.txt") == 0) f->sums = a.Str(L"browser_download_url");
                    }
                if (f->tag.empty()) throw std::wstring(Tr(L"GitHub sent an unexpected answer", L"GitHub прислал неожиданный ответ"));
            } catch (const std::wstring& e) { f->error = e; }
              catch (...) { f->error = NoConnection(); }
            App::OnUi([f, manual, before] {
                FILETIME now; GetSystemTimeAsFileTime(&now);
                if (!f->error.empty()) {
                    // A quiet check that fails leaves things as they were; only a click shows the error.
                    if (manual) { phase = Phase::Failed; problem = f->error; } else phase = before == Phase::Checking ? Phase::Idle : before;
                    Redraw();
                    return;
                }
                checkedAt = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;
                problem.clear();
                auto tag = f->tag; if (!tag.empty() && (tag[0] == L'v' || tag[0] == L'V')) tag.erase(0, 1);
                if (Newer(tag, Version()) && !f->asset.empty() && !f->sums.empty()) {
                    latest = tag; assetUrl = f->asset; sumsUrl = f->sums;
                    phase = Phase::Ready;
                    // Once per version: the balloon is a nudge, the buttons stay for later.
                    if (!manual && Settings::Str(L"UpdateNotified") != tag) {
                        Settings::Set(L"UpdateNotified", tag);
                        App::Notify(Util::Format(Tr(L"Switcher %s is out", L"Вышел переключатель %s"), tag.c_str()),
                                    Tr(L"Click to update. It takes a few seconds, Claude windows stay open", L"Нажмите, чтобы обновить. Это займёт пару секунд, окна Claude останутся открытыми"),
                                    false, [] { Install(); });
                    }
                } else phase = Phase::Current;
                Redraw();
            });
        }).detach();
    }

    void Install() {
        if (Paths::Demo || phase != Phase::Ready) return;
        phase = Phase::Downloading; Redraw();
        // Comes back the way it was left: the window if it was open, the tray otherwise.
        std::wstring args = L"--install --updated";
        if (!Picker::Visible()) args += L" --tray";
        if (SettingsWin::Visible()) args += L" --settings";
        auto asset = assetUrl, sums = sumsUrl;
        std::thread([asset, sums, args] {
            std::wstring error, file;
            try {
                auto exe = Get(asset, 16 * 1024 * 1024, false);
                auto want = ExpectedHash(Get(sums, 64 * 1024, false));
                if (want.empty() || Sha256(exe) != want)
                    throw std::wstring(Tr(L"The download doesn't match the release's checksum, so it wasn't installed", L"Скачанный файл не совпал с контрольной суммой релиза, поэтому не установлен"));
                if (exe.size() < 2 || exe[0] != 'M' || exe[1] != 'Z') throw std::wstring(Tr(L"The download isn't a program", L"Скачанный файл не является программой"));
                auto dir = Folder();
                CreateDirectoryW(dir.c_str(), nullptr);
                file = dir + L"\\ClaudeProfileSwitcher.exe";
                HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                DWORD wrote = 0;
                bool ok = h != INVALID_HANDLE_VALUE && WriteFile(h, exe.data(), (DWORD)exe.size(), &wrote, nullptr) && wrote == exe.size();
                if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
                if (!ok) throw Util::Format(Tr(L"Couldn't save the update to %s", L"Не удалось сохранить обновление в %s"), dir.c_str());
            } catch (const std::wstring& e) { error = e; }
              catch (...) { error = NoConnection(); }
            App::OnUi([error, file, args] {
                if (error.empty()) {
                    phase = Phase::Installing; Redraw();
                    // The new copy closes this one through the exit event, so it can show its window in front.
                    AllowSetForegroundWindow(ASFW_ANY);
                    if ((INT_PTR)ShellExecuteW(nullptr, L"open", file.c_str(), args.c_str(), Paths::Home.c_str(), SW_SHOWNORMAL) > 32) return;
                    problem = Tr(L"Couldn't start the update", L"Не удалось запустить обновление");
                } else problem = error;
                phase = Phase::Failed;
                Core::Log(L"Update: " + problem);
                App::Notify(Tr(L"Couldn't update the switcher", L"Не удалось обновить переключатель"), problem, true);
                Redraw();
            });
        }).detach();
    }

    // The installer copy left in %TEMP% has done its job by the time the new switcher runs.
    void Cleanup() {
        auto dir = Folder(), file = dir + L"\\ClaudeProfileSwitcher.exe";
        if (!Util::Exists(file) || Paths::Same(Paths::Self, file)) return;
        std::thread([dir, file] {
            for (int i = 0; i < 20 && Util::Exists(file) && !DeleteFileW(file.c_str()); i++) Sleep(500);
            RemoveDirectoryW(dir.c_str());
        }).detach();
    }
}
