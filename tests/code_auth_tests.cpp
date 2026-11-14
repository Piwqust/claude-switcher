// Includes the module to exercise encrypted Desktop fixtures without a network
// service or a test-only entry point in the shipped application.
#include "../src/code_auth.cpp"
#include "../src/app.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <aclapi.h>

namespace fs = std::filesystem;
static int checks = 0;
static void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
    checks++;
}
template<class F> void Refuses(F action, const char* message) {
    bool refused = false;
    try { action(); } catch (const std::wstring&) { refused = true; }
    Check(refused, message);
}
static std::wstring Json(const json::Value& value) { std::wstring out; json::Write(out, value); return out; }
static int menusRefreshed = 0;
static void CALLBACK RefreshInsideMenu(HWND owner, UINT, UINT_PTR timer, DWORD) {
    KillTimer(owner, timer);
    auto next = App::items;
    std::reverse(next.begin(), next.end());
    // Free the original vector and its profile strings while TrackPopupMenuEx
    // is still dispatching messages, exactly as the two-second refresh does.
    std::vector<Item>().swap(App::items);
    std::vector<BYTE> churn(sizeof(Item) * next.size(), 0xa5);
    App::items = std::move(next);
    menusRefreshed++;
    EndMenu();
}
static void MenuRefreshTests() {
    Paths::Demo = true;
    Gfx::Init();
    HWND owner = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"CPS menu regression test", WS_POPUP, 20, 20, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(owner != nullptr, "create native menu test owner");
    ShowWindow(owner, SW_SHOWNOACTIVATE);
    for (int i = 0; i < 40; i++) {
        App::items.clear();
        for (int n = 0; n < 3; n++) {
            Item item; item.p.id = i % 2 ? std::wstring(64, L'P') + std::to_wstring(n) : L"Account " + std::to_wstring(n);
            item.p.name = item.name = L"Test account"; item.index = n + 1;
            App::items.push_back(item);
        }
        auto expected = App::items.front().p.id;
        Check(SetTimer(owner, 991, 30, RefreshInsideMenu) != 0, "schedule refresh during native menu");
        App::ShowMenu(App::items.front().p.id, { 25, 25 }, owner);
        KillTimer(owner, 991);
        Check(menusRefreshed == i + 1 && App::Find(expected), "native menu survives replacement of its source account list");
    }
    DestroyWindow(owner);
    App::items.clear();
}
static std::wstring Base64(const BYTE* bytes, size_t size) {
    DWORD n = 0;
    Check(CryptBinaryToStringW(bytes, (DWORD)size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &n) != 0, "base64 size");
    std::wstring out(n, L'\0');
    Check(CryptBinaryToStringW(bytes, (DWORD)size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out.data(), &n) != 0, "base64 encode");
    if (!out.empty() && !out.back()) out.pop_back(); return out;
}
static void Cache(const Profile& profile, const json::Value& cache, bool tamper = false) {
    fs::create_directories(profile.path);
    BYTE key[32]; for (int i = 0; i < 32; i++) key[i] = (BYTE)i;
    DATA_BLOB input{ sizeof key, key }, wrapped{};
    Check(CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &wrapped) != 0, "protect fixture key");
    std::vector<BYTE> keyBlob{ 'D','P','A','P','I' }; keyBlob.insert(keyBlob.end(), wrapped.pbData, wrapped.pbData + wrapped.cbData); LocalFree(wrapped.pbData);
    auto state = json::Value::MakeObject(); state[L"os_crypt"][L"encrypted_key"] = Base64(keyBlob.data(), keyBlob.size());
    CodeAuth::PrivateWrite(profile.path + L"\\Local State", state);
    auto text = Json(cache);
    int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::vector<BYTE> bytes(n); WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), (char*)bytes.data(), n, nullptr, nullptr);
    BCRYPT_ALG_HANDLE aes = nullptr; BCRYPT_KEY_HANDLE secret = nullptr;
    Check(BCryptOpenAlgorithmProvider(&aes, BCRYPT_AES_ALGORITHM, nullptr, 0) >= 0, "open fixture AES");
    Check(BCryptSetProperty(aes, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0) >= 0, "fixture GCM mode");
    Check(BCryptGenerateSymmetricKey(aes, &secret, nullptr, 0, key, sizeof key, 0) >= 0, "fixture AES key");
    std::vector<BYTE> out(3 + 12 + bytes.size() + 16, 0); memcpy(out.data(), "v10", 3);
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth; BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce = out.data() + 3; auth.cbNonce = 12; auth.pbTag = out.data() + out.size() - 16; auth.cbTag = 16;
    ULONG got = 0;
    Check(BCryptEncrypt(secret, bytes.data(), (ULONG)bytes.size(), &auth, nullptr, 0, out.data() + 15, (ULONG)bytes.size(), &got, 0) >= 0, "encrypt fixture");
    BCryptDestroyKey(secret); BCryptCloseAlgorithmProvider(aes, 0);
    if (tamper) out.back() ^= 1;
    auto cfg = json::Value::MakeObject(); cfg[L"lastKnownAccountUuid"] = L"fixture-account";
    cfg[L"oauth:tokenCacheV2"] = Base64(out.data(), out.size());
    CodeAuth::PrivateWrite(profile.path + L"\\config.json", cfg);
}
static void Tests() {
    Profile a; a.id = L"Account 2"; a.path = Paths::Root + L"\\Account 2";
    Profile b; b.id = L"Account 3"; b.path = Paths::Root + L"\\Account 3";
    fs::create_directories(a.path); fs::create_directories(b.path);
    auto original = json::Value::MakeObject(); original[L"theme"] = L"dark"; original[L"env"][L"KEEP_ME"] = L"unchanged";
    original[L"hooks"][L"SessionStart"] = json::Value::MakeArray();
    auto path = CodeAuth::ConfigPath(); CodeAuth::PrivateWrite(path, original);
    auto creds = Util::Parent(path) + L"\\.credentials.json";
    Util::WriteText(creds, L"{\"original-login\":\"keep\"}", false);
    CodeAuth::Use(a);
    Check(CodeAuth::Selected() == a.id, "select first account");
    auto cfg = CodeAuth::ReadObject(path);
    Check(CodeAuth::ConfigToken(cfg) == L"cps-demo-token-Account 2", "install default in global user settings");
    Check(cfg.Str(L"theme") == L"dark" && cfg[L"env"].Str(L"KEEP_ME") == L"unchanged" && cfg.Find(L"hooks"), "preserve unrelated settings");
    Check(Json(CodeAuth::ReadObject(CodeAuth::StatePath())).find(L"cps-demo-token") == std::wstring::npos, "ownership journal contains no raw tokens");
    PSECURITY_DESCRIPTOR sd = nullptr; PACL acl = nullptr;
    Check(GetNamedSecurityInfoW(path.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &acl, nullptr, &sd) == ERROR_SUCCESS, "read settings permissions");
    SECURITY_DESCRIPTOR_CONTROL control{}; DWORD revision = 0;
    Check(GetSecurityDescriptorControl(sd, &control, &revision) && (control & SE_DACL_PROTECTED) && acl && acl->AceCount == 2, "settings ACL is private and does not inherit broad permissions");
    LocalFree(sd);
    auto before = Util::FileTime(path); CodeAuth::Sync();
    Check(Util::FileTime(path) == before, "unchanged sync does not rewrite settings");
    CodeAuth::Use(b); Check(CodeAuth::Selected() == b.id, "switch second account");
    Check(Util::ReadText(creds) == L"{\"original-login\":\"keep\"}", "existing login untouched");
    CodeAuth::Restore(); Check(CodeAuth::Selected().empty(), "restore clears selection");
    Check(Json(CodeAuth::ReadObject(path)) == Json(original), "restore preserves original settings");

    original.Remove(L"env"); CodeAuth::PrivateWrite(path, original);
    CodeAuth::Use(a); CodeAuth::Restore();
    Check(Json(CodeAuth::ReadObject(path)) == Json(original), "restore removes only the env object it created");
    auto emptyToken = original; emptyToken[L"env"][CodeAuth::TokenEnv] = L""; CodeAuth::PrivateWrite(path, emptyToken);
    CodeAuth::Use(a); CodeAuth::Restore();
    Check(Json(CodeAuth::ReadObject(path)) == Json(emptyToken), "restore preserves a pre-existing empty token setting");
    cfg = original; cfg[L"env"][CodeAuth::TokenEnv] = L"manually-configured";
    CodeAuth::PrivateWrite(path, cfg);
    Refuses([&] { CodeAuth::Use(a); }, "do not overwrite external token");
    Check(Json(CodeAuth::ReadObject(path)) == Json(cfg), "external token retained");
    CodeAuth::PrivateWrite(path, original); CodeAuth::Use(a);
    cfg = CodeAuth::ReadObject(path); cfg[L"env"][CodeAuth::TokenEnv] = L"new-manual-token"; cfg[L"new-setting"] = true; CodeAuth::PrivateWrite(path, cfg);
    Refuses([&] { CodeAuth::Sync(); }, "do not overwrite manual changes on sync");
    Refuses([&] { CodeAuth::Restore(); }, "report manual login on stop");
    Check(CodeAuth::Selected().empty() && Json(CodeAuth::ReadObject(path)) == Json(cfg), "stop syncing without removing manual login");
    Util::WriteText(path, L"{ invalid json", false);
    Refuses([&] { CodeAuth::Use(a); }, "reject malformed settings");
    Check(Util::ReadText(path) == L"{ invalid json", "malformed file left intact");
    for (auto invalid : { L"{", L"[", L"{\"env\":{}", L"{} trailing", L"{\"env\":{},\"env\":{}}", L"{\"n\":01}", L"{\"n\":+1}", L"{\"n\":1e}", L"{\"s\":\"\\q\"}", L"{\"s\":\"\\uZZZZ\"}" }) {
        Util::WriteText(path, invalid, false);
        Refuses([&] { CodeAuth::Use(a); }, "reject ambiguous or incomplete JSON without rewriting it");
        Check(Util::ReadText(path) == invalid, "invalid JSON remains byte-equivalent");
    }
    json::Value precise;
    Check(json::Parser::Parse(L"{\"integer\":9007199254740993,\"scientific\":1.2500e+100}", precise), "parse unrelated numeric settings");
    CodeAuth::PrivateWrite(path, precise); CodeAuth::Use(a); CodeAuth::Restore();
    auto preserved = Util::ReadText(path);
    Check(preserved.find(L"9007199254740993") != std::wstring::npos && preserved.find(L"1.2500e+100") != std::wstring::npos, "preserve numeric settings without rounding");
    cfg = original; cfg[L"env"] = 1; CodeAuth::PrivateWrite(path, cfg);
    Refuses([&] { CodeAuth::Use(a); }, "reject malformed env");

    // Simulate a crash after the ownership journal is written but before settings.
    CodeAuth::PrivateWrite(path, original); CodeAuth::Use(a);
    auto journal = CodeAuth::ReadObject(CodeAuth::StatePath());
    journal[L"previousHash"] = journal.Str(L"tokenHash"); journal[L"previousProfileId"] = a.id;
    journal[L"tokenHash"] = CodeAuth::Hash(L"cps-demo-token-Account 3"); journal[L"profileId"] = b.id;
    CodeAuth::PrivateWrite(CodeAuth::StatePath(), journal);
    Check(CodeAuth::Selected() == a.id, "interrupted switch reports installed account");
    CodeAuth::Sync(); Check(CodeAuth::Selected() == b.id, "interrupted switch recovers"); CodeAuth::Restore();

    HANDLE held = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(held != INVALID_HANDLE_VALUE, "lock fixture against replacement");
    Refuses([&] { CodeAuth::Use(a); }, "report a failed settings replacement");
    CloseHandle(held);
    Check(CodeAuth::Selected().empty() && Json(CodeAuth::ReadObject(path)) == Json(original), "failed activation rolls back journal and settings");

    auto cache = json::Value::MakeObject();
    auto entry = json::Value::MakeObject(); entry[L"token"] = L"fake-inference-token"; entry[L"refreshToken"] = L"never-export-this";
    entry[L"expiresAt"] = (double)Core::NowMs() + 3600000;
    cache[L"client:org:aud:user:inference user:profile"] = entry;
    auto special = entry; special[L"token"] = L"wrong-sessions-token"; special[L"expiresAt"] = (double)Core::NowMs() + 7200000;
    cache[L"client:org:aud:user:inference user:profile user:sessions:claude_code"] = special;
    Cache(a, cache); Paths::Demo = false;
    auto validV1 = CodeAuth::ReadObject(a.path + L"\\config.json").Str(L"oauth:tokenCacheV2");
    auto token = CodeAuth::ReadToken(a);
    Check(token.value == L"fake-inference-token" && token.account == L"fixture-account", "decrypt Desktop token and exclude sessions credential");
    Cache(a, cache, true); Refuses([&] { CodeAuth::ReadToken(a); }, "reject AES-GCM tampering");
    entry[L"expiresAt"] = (double)Core::NowMs() - 1; cache = json::Value::MakeObject(); cache[L"client:org:aud:user:inference user:profile"] = entry;
    Cache(a, cache); Refuses([&] { CodeAuth::ReadToken(a); }, "reject expired token");
    Cache(a, json::Value::MakeObject());
    cfg = CodeAuth::ReadObject(a.path + L"\\config.json"); cfg[L"oauth:tokenCache"] = validV1; CodeAuth::PrivateWrite(a.path + L"\\config.json", cfg);
    Refuses([&] { CodeAuth::ReadToken(a); }, "do not resurrect stale V1 credentials when V2 is empty");
    Paths::Demo = true;
    CodeAuth::PrivateWrite(path, original);
    Paths::Demo = false;
    SetEnvironmentVariableW(L"ANTHROPIC_API_KEY", L"test-override");
    Refuses([&] { CodeAuth::CheckOverrides(original); }, "refuse environment credential override");
    SetEnvironmentVariableW(L"ANTHROPIC_API_KEY", nullptr);
    cfg = original; cfg[L"apiKeyHelper"] = L"test-helper";
    Refuses([&] { CodeAuth::CheckOverrides(cfg); }, "refuse apiKeyHelper override");
    Paths::Demo = true;
}
// Profiles hold paths past 260 characters, read-only files and links; a delete removes all of
// that, follows no link out of the folder, and leaves everything in place while a file is open.
static std::wstring Long(const std::wstring& p) { return L"\\\\?\\" + p; }
static std::wstring DeepTree(const std::wstring& dir) {
    auto d = dir;
    Check(CreateDirectoryW(Long(d).c_str(), nullptr) != 0, "create profile folder");
    for (int i = 0; i < 8; i++) { d += L"\\" + std::wstring(40, L'a' + i); Check(CreateDirectoryW(Long(d).c_str(), nullptr) != 0, "create deep folder"); }
    auto file = d + L"\\skill.md";
    Check(file.size() > 300, "fixture path is past the classic limit");
    HANDLE h = CreateFileW(Long(file).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_READONLY, nullptr);
    Check(h != INVALID_HANDLE_VALUE, "create long-path read-only file"); CloseHandle(h);
    return file;
}
static void DeleteTests() {
    Paths::IconDir = Paths::Home + L"\\icons";
    auto outside = Paths::Root + L"\\..\\" + fs::path(Paths::Root).filename().wstring() + L"-outside";
    fs::create_directories(outside);
    Util::WriteText(outside + L"\\keep.txt", L"keep", false);

    Profile p; p.id = L"CPS delete test"; p.name = L"CPS delete test"; p.path = Paths::Root + L"\\" + p.id;
    DeepTree(p.path);
    auto link = p.path + L"\\linked";
    Check(_wsystem((L"mklink /J \"" + link + L"\" \"" + outside + L"\" >nul").c_str()) == 0, "create junction out of the profile");
    auto all = json::Value::MakeObject();
    all[L"Labels"][p.id] = L"Renamed"; all[L"Labels"][L"Other"] = L"Kept";
    all[L"Colors"][p.id] = 3; all[L"Colors"][L"Other"] = 4;
    auto open = json::Value::MakeArray(); open.arr.emplace_back(p.id); open.arr.emplace_back(L"Other"); all[L"AutoOpen"] = open;
    Settings::Save(all);

    Core::Delete(p);
    Check(!fs::exists(p.path), "delete removes a profile with long paths");
    Check(Util::Exists(outside + L"\\keep.txt"), "delete does not follow a junction out of the profile");
    auto after = Settings::Load();
    Check(!after.Find(L"Labels")->Find(p.id) && after.Find(L"Labels")->Find(L"Other"), "delete forgets the label only");
    Check(!after.Find(L"Colors")->Find(p.id) && after.Find(L"Colors")->Find(L"Other"), "delete forgets the colour only");
    auto ids = Settings::List(L"AutoOpen");
    Check(ids.size() == 1 && ids[0] == L"Other", "delete forgets auto-open only");

    Profile q; q.id = L"CPS locked test"; q.name = q.id; q.path = Paths::Root + L"\\" + q.id;
    auto held = DeepTree(q.path);
    HANDLE h = CreateFileW(Long(held).c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(h != INVALID_HANDLE_VALUE, "hold a file of the profile open");
    Refuses([&] { Core::Delete(q); }, "delete refuses while a file is open");
    CloseHandle(h);
    Check(Util::Exists(Long(held)), "a refused delete touches nothing");
    Core::Delete(q);
    Check(!fs::exists(q.path), "delete succeeds once the file is closed");

    auto left = Paths::Root + L"\\.deleted-CPS sweep test-1";
    DeepTree(left);
    Core::SweepDeleted();
    Check(GetFileAttributesW(Long(left).c_str()) == INVALID_FILE_ATTRIBUTES, "start-up sweep finishes a delete");
    fs::remove_all(outside);
}
int wmain(int argc, wchar_t** argv) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (argc == 2 && std::wstring(argv[1]) == L"--menu-refresh") {
        try { MenuRefreshTests(); std::cout << menusRefreshed << " native menus survived account-list replacement\n"; return 0; }
        catch (...) { std::cerr << "Native menu refresh regression failed\n"; return 1; }
    }
    // This read-only probe exercises the production Windows decryption and server
    // check, then writes only a caller-specified isolated CLI configuration.
    if (argc == 4 && std::wstring(argv[1]) == L"--probe") {
        try {
            Profile p; p.path = argv[2]; Paths::Demo = false;
            auto token = CodeAuth::ReadToken(p); CodeAuth::Validate(token);
            auto cfg = json::Value::MakeObject(); cfg[L"env"][CodeAuth::TokenEnv] = token.value;
            auto output = fs::weakly_canonical(argv[3]); wchar_t temp[MAX_PATH]; GetTempPathW(ARRAYSIZE(temp), temp);
            Check(output.parent_path().parent_path() == fs::weakly_canonical(temp) && output.parent_path().filename().wstring().find(L"cps-code-probe-") == 0 && output.filename() == L"settings.json", "probe output must be an isolated temp configuration");
            CodeAuth::PrivateWrite(output.wstring(), cfg);
            std::cout << "Desktop account verified; isolated Code settings written\n"; return 0;
        } catch (...) { std::cerr << "Read-only probe failed\n"; return 1; }
    }
    wchar_t temp[MAX_PATH]; GetTempPathW(ARRAYSIZE(temp), temp);
    auto root = fs::weakly_canonical(temp) / Util::Format(L"cps-code-tests-%lu-%llu", GetCurrentProcessId(), GetTickCount64());
    int result = 0;
    try {
        Check(!fs::exists(root) && fs::create_directory(root), "create isolated test directory");
        Paths::Root = root.wstring(); Paths::Home = Paths::Root + L"\\.switcher"; Paths::SettingsFile = Paths::Root + L"\\settings.json"; Paths::Log = Paths::Root + L"\\error.log"; Paths::Demo = true;
        Core::App.defaultData = Paths::Root + L"\\.default";
        Tests(); DeleteTests(); std::cout << checks << " checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; result = 1; }
      catch (const std::wstring& e) { std::wcerr << e << L'\n'; result = 1; }
    if (root.parent_path() == fs::weakly_canonical(temp) && root.filename().wstring().find(L"cps-code-tests-") == 0) fs::remove_all(root);
    CoUninitialize(); return result;
}
