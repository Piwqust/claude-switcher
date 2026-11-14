#include "code_auth.h"
#include <bcrypt.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <sddl.h>
#include <algorithm>

namespace CodeAuth {
namespace {
    const wchar_t* TokenEnv = L"CLAUDE_CODE_OAUTH_TOKEN";
    struct Handle {
        HANDLE h;
        explicit Handle(HANDLE v) : h(v) {}
        ~Handle() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    };
    struct Lock {
        Handle mutex;
        Lock() : mutex(CreateMutexW(nullptr, FALSE, Paths::Demo ? L"Local\\CpsCodeAuthDemo" : L"Local\\CpsCodeAuth")) {
            if (!mutex.h) throw std::wstring(Tr(L"Couldn't lock Claude Code settings", L"Не удалось заблокировать настройки Claude Code"));
            DWORD r = WaitForSingleObject(mutex.h, 15000);
            if (r != WAIT_OBJECT_0 && r != WAIT_ABANDONED) throw std::wstring(Tr(L"Claude Code settings are busy. Try again", L"Настройки Claude Code заняты. Повторите попытку"));
        }
        ~Lock() { ReleaseMutex(mutex.h); }
    };
    std::wstring StatePath() { return Paths::Home + L"\\code-auth.json"; }
    std::wstring ConfigPath() {
        if (Paths::Demo) return Paths::Root + L"\\.code\\settings.json";
        wchar_t home[MAX_PATH * 2]{};
        GetEnvironmentVariableW(L"USERPROFILE", home, ARRAYSIZE(home));
        if (!home[0]) throw std::wstring(Tr(L"USERPROFILE is missing", L"Не задана папка пользователя Windows"));
        return std::wstring(home) + L"\\.claude\\settings.json";
    }
    void CheckFile(const std::wstring& path) {
        DWORD a = GetFileAttributesW(path.c_str());
        if (a == INVALID_FILE_ATTRIBUTES && GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND)
            throw std::wstring(Tr(L"Couldn't access Claude Code settings. The file was left unchanged", L"Нет доступа к настройкам Claude Code. Файл не изменён"));
        if (a != INVALID_FILE_ATTRIBUTES && (a & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)))
            throw std::wstring(Tr(L"Claude Code settings must be a regular file", L"Настройки Claude Code должны быть обычным файлом"));
    }
    json::Value ReadObject(const std::wstring& path) {
        CheckFile(path);
        if (!Util::Exists(path)) return json::Value::MakeObject();
        json::Value value;
        if (!json::Parser::Parse(Util::ReadText(path, true), value) || value.type != json::Value::Object)
            throw std::wstring(Tr(L"Couldn't read Claude Code settings. The file was left unchanged", L"Не удалось прочитать настройки Claude Code. Файл не изменён"));
        return value;
    }
    // Access tokens must not inherit a permissive ACL from an existing settings
    // file. Both the temporary file and the replacement are private from birth.
    void PrivateWrite(const std::wstring& path, const json::Value& value) {
        CheckFile(path);
        CreateDirectoryW(Util::Parent(path).c_str(), nullptr);
        Handle processToken(nullptr);
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &processToken.h)) throw std::wstring(Tr(L"Couldn't read Windows user identity", L"Не удалось определить пользователя Windows"));
        DWORD size = 0;
        GetTokenInformation(processToken.h, TokenUser, nullptr, 0, &size);
        std::vector<BYTE> info(size);
        if (!size || !GetTokenInformation(processToken.h, TokenUser, info.data(), size, &size)) throw std::wstring(Tr(L"Couldn't read Windows user identity", L"Не удалось определить пользователя Windows"));
        wchar_t* sid = nullptr;
        if (!ConvertSidToStringSidW(((TOKEN_USER*)info.data())->User.Sid, &sid)) throw std::wstring(Tr(L"Couldn't read Windows user identity", L"Не удалось определить пользователя Windows"));
        std::wstring sddl = L"D:P(A;;FA;;;SY)(A;;FA;;;" + std::wstring(sid) + L")";
        LocalFree(sid);
        PSECURITY_DESCRIPTOR sd = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr)) throw std::wstring(Tr(L"Couldn't protect Claude Code settings", L"Не удалось защитить настройки Claude Code"));
        SECURITY_ATTRIBUTES sa{ sizeof sa, sd, FALSE };
        auto tmp = path + Util::Format(L".cps-%lu-%llu.tmp", GetCurrentProcessId(), GetTickCount64());
        HANDLE file = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, &sa, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        LocalFree(sd);
        if (file == INVALID_HANDLE_VALUE) throw std::wstring(Tr(L"Couldn't save Claude Code settings", L"Не удалось сохранить настройки Claude Code"));
        std::wstring text; json::Write(text, value);
        int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
        std::string bytes(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), bytes.data(), n, nullptr, nullptr);
        DWORD wrote = 0;
        bool ok = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &wrote, nullptr) && wrote == bytes.size() && FlushFileBuffers(file);
        CloseHandle(file);
        if (ok) ok = MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!ok) {
            DeleteFileW(tmp.c_str());
            throw std::wstring(Tr(L"Couldn't save Claude Code settings. Try again", L"Не удалось сохранить настройки Claude Code. Повторите попытку"));
        }
    }
    std::vector<BYTE> Decode(const std::wstring& text) {
        DWORD size = 0;
        if (!CryptStringToBinaryW(text.data(), (DWORD)text.size(), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr)) throw std::wstring(Tr(L"Couldn't decode Desktop sign-in", L"Не удалось прочитать сохранённый вход Desktop"));
        std::vector<BYTE> bytes(size);
        if (!CryptStringToBinaryW(text.data(), (DWORD)text.size(), CRYPT_STRING_BASE64, bytes.data(), &size, nullptr, nullptr)) throw std::wstring(Tr(L"Couldn't decode Desktop sign-in", L"Не удалось прочитать сохранённый вход Desktop"));
        bytes.resize(size); return bytes;
    }
    std::vector<BYTE> Unprotect(const BYTE* bytes, size_t size) {
        DATA_BLOB input{ (DWORD)size, const_cast<BYTE*>(bytes) }, out{};
        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
            throw std::wstring(Tr(L"Windows couldn't read this Desktop sign-in", L"Windows не удалось прочитать сохранённый вход Desktop"));
        std::vector<BYTE> result(out.pbData, out.pbData + out.cbData);
        SecureZeroMemory(out.pbData, out.cbData); LocalFree(out.pbData); return result;
    }
    std::wstring Wide(const BYTE* data, size_t size) {
        int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char*)data, (int)size, nullptr, 0);
        if (!n) throw std::wstring(Tr(L"Couldn't read Desktop sign-in encoding", L"Не удалось прочитать данные входа Desktop"));
        std::wstring out(n, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char*)data, (int)size, out.data(), n); return out;
    }
    json::Value DecryptCache(const std::wstring& encoded, const std::wstring& folder) {
        auto bytes = Decode(encoded);
        std::vector<BYTE> plain;
        if (bytes.size() >= 31 && memcmp(bytes.data(), "v10", 3) == 0) {
            auto state = ReadObject(folder + L"\\Local State");
            auto crypto = state.Find(L"os_crypt");
            auto wrapped = Decode(crypto ? crypto->Str(L"encrypted_key") : L"");
            if (wrapped.size() <= 5 || memcmp(wrapped.data(), "DPAPI", 5)) throw std::wstring(Tr(L"Unsupported Desktop encryption", L"Этот формат шифрования Desktop пока не поддерживается"));
            auto key = Unprotect(wrapped.data() + 5, wrapped.size() - 5);
            BCRYPT_ALG_HANDLE aes = nullptr; BCRYPT_KEY_HANDLE secret = nullptr;
            NTSTATUS status = BCryptOpenAlgorithmProvider(&aes, BCRYPT_AES_ALGORITHM, nullptr, 0);
            if (status >= 0) status = BCryptSetProperty(aes, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
            if (status >= 0) status = BCryptGenerateSymmetricKey(aes, &secret, nullptr, 0, key.data(), (ULONG)key.size(), 0);
            BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
            BCRYPT_INIT_AUTH_MODE_INFO(auth);
            auth.pbNonce = bytes.data() + 3; auth.cbNonce = 12;
            auth.pbTag = bytes.data() + bytes.size() - 16; auth.cbTag = 16;
            plain.resize(bytes.size() - 31); ULONG got = 0;
            if (status >= 0) status = BCryptDecrypt(secret, bytes.data() + 15, (ULONG)plain.size(), &auth, nullptr, 0, plain.data(), (ULONG)plain.size(), &got, 0);
            if (secret) BCryptDestroyKey(secret);
            if (aes) BCryptCloseAlgorithmProvider(aes, 0);
            SecureZeroMemory(key.data(), key.size());
            if (status < 0) throw std::wstring(Tr(L"Couldn't decrypt Desktop sign-in", L"Не удалось расшифровать сохранённый вход Desktop"));
            plain.resize(got);
        } else plain = Unprotect(bytes.data(), bytes.size());
        json::Value cache;
        auto text = Wide(plain.data(), plain.size());
        SecureZeroMemory(plain.data(), plain.size());
        bool ok = json::Parser::Parse(text, cache) && cache.type == json::Value::Object;
        SecureZeroMemory(text.data(), text.size() * sizeof(wchar_t));
        if (!ok) throw std::wstring(Tr(L"Couldn't read Desktop sign-in cache", L"Не удалось прочитать кэш входа Desktop"));
        return cache;
    }
    struct Token { std::wstring value, account; double expires = 0; };
    Token ReadToken(const Profile& profile) {
        if (Paths::Demo) return { L"cps-demo-token-" + profile.id, L"demo", (double)Core::NowMs() + 86400000 };
        auto cfg = ReadObject(profile.path + L"\\config.json");
        Token result; result.account = cfg.Str(L"lastKnownAccountUuid");
        for (auto field : { L"oauth:tokenCacheV2", L"oauth:tokenCache" }) {
            auto encoded = cfg.Str(field);
            if (encoded.empty()) continue;
            auto cache = DecryptCache(encoded, profile.path);
            for (auto& kv : cache.obj) {
                auto& scopes = kv.first;
                if (scopes.find(L"user:inference") == std::wstring::npos || scopes.find(L"user:profile") == std::wstring::npos || scopes.find(L"user:sessions:") != std::wstring::npos) continue;
                auto token = kv.second.Str(L"token"); auto expiry = kv.second.Num(L"expiresAt");
                if (!token.empty() && expiry > (double)Core::NowMs() + 60000 && expiry > result.expires) {
                    result.value = token; result.expires = expiry;
                }
            }
            // V1 may retain obsolete credentials after an account change. Never
            // fall back to it when the current V2 cache exists, even when empty.
            if (field == std::wstring(L"oauth:tokenCacheV2")) break;
        }
        if (result.value.empty() || result.account.empty())
            throw std::wstring(Tr(L"Open this account in Claude Desktop to renew its saved sign-in, then try again. No new Claude Code login is needed",
                                 L"Откройте этот аккаунт в Claude Desktop, чтобы обновился сохранённый вход, затем повторите. Отдельный вход в Claude Code не нужен"));
        return result;
    }
    struct Http {
        HINTERNET h;
        explicit Http(HINTERNET value) : h(value) {}
        ~Http() { if (h) WinHttpCloseHandle(h); }
    };
    void Validate(const Token& token) {
        if (Paths::Demo) return;
        Http session(WinHttpOpen(L"ClaudeProfileSwitcher/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session.h) throw std::wstring(Tr(L"Couldn't connect to Claude", L"Не удалось соединиться с сервером Claude"));
        WinHttpSetTimeouts(session.h, 10000, 10000, 10000, 10000);
        Http connection(WinHttpConnect(session.h, L"api.anthropic.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
        Http request(connection.h ? WinHttpOpenRequest(connection.h, L"GET", L"/api/oauth/profile", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr);
        DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!request.h || !WinHttpSetOption(request.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof redirects)) throw std::wstring(Tr(L"Couldn't connect to Claude", L"Не удалось соединиться с сервером Claude"));
        auto headers = L"Authorization: Bearer " + token.value + L"\r\nanthropic-beta: oauth-2025-04-20\r\n";
        bool sent = WinHttpSendRequest(request.h, headers.c_str(), (DWORD)headers.size(), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != 0;
        SecureZeroMemory(headers.data(), headers.size() * sizeof(wchar_t));
        if (!sent || !WinHttpReceiveResponse(request.h, nullptr)) throw std::wstring(Tr(L"Couldn't verify the account with Claude. Check your connection and try again", L"Не удалось проверить аккаунт на сервере Claude. Проверьте соединение и повторите"));
        DWORD status = 0, size = sizeof status;
        if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) || status != 200)
            throw std::wstring(Tr(L"Claude didn't accept this saved sign-in. Open the account in Desktop, then try again", L"Claude не принял сохранённый вход. Откройте аккаунт в Desktop, затем повторите"));
        std::vector<BYTE> body; BYTE buffer[4096]; DWORD got = 0;
        do {
            if (!WinHttpReadData(request.h, buffer, sizeof buffer, &got)) throw std::wstring(Tr(L"Couldn't verify Claude account", L"Не удалось проверить аккаунт Claude"));
            body.insert(body.end(), buffer, buffer + got);
            if (body.size() > 1024 * 1024) throw std::wstring(Tr(L"Unexpected Claude response", L"Сервер Claude вернул неожиданный ответ"));
        } while (got);
        json::Value profile;
        if (!json::Parser::Parse(Wide(body.data(), body.size()), profile)) throw std::wstring(Tr(L"Couldn't verify Claude account", L"Не удалось проверить аккаунт Claude"));
        auto account = profile.Find(L"account");
        if (!account || account->Str(L"uuid") != token.account) throw std::wstring(Tr(L"The saved token belongs to another account. Open this profile in Desktop and try again", L"Сохранённый токен относится к другому аккаунту. Откройте этот профиль в Desktop и повторите"));
    }
    std::wstring Hash(const std::wstring& text) {
        BYTE hash[32];
        if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, (PUCHAR)text.data(), (ULONG)(text.size() * sizeof(wchar_t)), hash, sizeof hash) < 0) throw std::wstring(Tr(L"Couldn't verify Claude Code settings", L"Не удалось проверить настройки Claude Code"));
        std::wstring out; for (BYTE b : hash) out += Util::Format(L"%02x", b); return out;
    }
    std::wstring ConfigToken(const json::Value& cfg) {
        auto env = cfg.Find(L"env");
        if (env && env->type != json::Value::Object) throw std::wstring(Tr(L"Claude Code settings.env must be an object", L"Неверный формат env в настройках Claude Code"));
        auto token = env ? env->Find(TokenEnv) : nullptr;
        if (token && token->type != json::Value::String) throw std::wstring(Tr(L"Claude Code OAuth token must be a string", L"Неверный формат токена в настройках Claude Code"));
        return token ? token->s : L"";
    }
    bool Owned(const json::Value& state, const std::wstring& token) {
        if (token.empty()) return true;
        auto hash = Hash(token);
        return !state.Str(L"profileId").empty() && (hash == state.Str(L"tokenHash") || hash == state.Str(L"previousHash"));
    }
    void CheckOverrides(const json::Value& cfg) {
        if (Paths::Demo) return;
        const wchar_t* names[] = { L"CLAUDE_CONFIG_DIR", L"CLAUDE_CODE_OAUTH_TOKEN", L"ANTHROPIC_API_KEY", L"ANTHROPIC_AUTH_TOKEN", L"ANTHROPIC_PROFILE", L"CLAUDE_CODE_USE_BEDROCK", L"CLAUDE_CODE_USE_VERTEX", L"CLAUDE_CODE_USE_FOUNDRY" };
        for (auto name : names) {
            bool present = GetEnvironmentVariableW(name, nullptr, 0) > 1;
            for (auto key : { L"Environment", L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment" }) {
                HKEY root = key == std::wstring(L"Environment") ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
                DWORD size = 0;
                if (RegGetValueW(root, key, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, nullptr, nullptr, &size) == ERROR_SUCCESS && size > sizeof(wchar_t)) present = true;
            }
            auto env = cfg.Find(L"env");
            if (name != std::wstring(TokenEnv) && env && env->Find(name) && !env->Str(name).empty()) present = true;
            if (present) throw Util::Format(Tr(L"%s overrides the default Claude Code account. Remove that explicit override first", L"%s переопределяет аккаунт Claude Code. Сначала уберите эту явную настройку"), name);
        }
        auto helper = cfg.Find(L"apiKeyHelper");
        if (helper && !helper->IsNull()) throw std::wstring(Tr(L"apiKeyHelper overrides the Claude Code account. Remove that explicit setting first", L"apiKeyHelper переопределяет аккаунт Claude Code. Сначала уберите эту явную настройку"));
    }
    void Apply(const Profile& profile, json::Value state) {
        auto path = ConfigPath();
        auto cfg = ReadObject(path);
        auto previous = ConfigToken(cfg);
        if (!Owned(state, previous)) throw std::wstring(Tr(L"Claude Code has a token set outside the switcher. It was left unchanged", L"Токен Claude Code задан вне переключателя. Он не изменён"));
        CheckOverrides(cfg);
        auto token = ReadToken(profile);
        if (previous == token.value && state.Str(L"profileId") == profile.id) return;
        Validate(token);
        // Re-read after the network request; don't discard unrelated edits made
        // while verification was in progress.
        cfg = ReadObject(path);
        if (ConfigToken(cfg) != previous) throw std::wstring(Tr(L"Claude Code settings changed. Try again", L"Настройки Claude Code изменились. Повторите попытку"));
        CheckOverrides(cfg);
        auto next = json::Value::MakeObject();
        next[L"profileId"] = profile.id;
        next[L"hadEnv"] = state.Find(L"hadEnv") ? state.Flag(L"hadEnv", false) : cfg.Find(L"env") != nullptr;
        auto env = cfg.Find(L"env");
        next[L"hadToken"] = state.Find(L"hadToken") ? state.Flag(L"hadToken", false) : env && env->Find(TokenEnv);
        next[L"tokenHash"] = Hash(token.value);
        next[L"previousHash"] = previous.empty() ? L"" : Hash(previous);
        next[L"previousProfileId"] = state.Str(L"profileId");
        // Journal ownership first. After a crash either the old or the new token
        // is recognised; no credential is stored in this journal.
        PrivateWrite(StatePath(), next);
        cfg[L"env"][TokenEnv] = token.value;
        try { PrivateWrite(path, cfg); }
        catch (...) {
            // A failed replacement must not leave a new selection queued for a
            // later timer tick after the person was told the action failed.
            PrivateWrite(StatePath(), state);
            throw;
        }
    }
}
    std::wstring Selected() {
        auto state = ReadObject(StatePath());
        if (state.Str(L"profileId").empty()) return L"";
        auto token = ConfigToken(ReadObject(ConfigPath()));
        if (!token.empty() && Hash(token) == state.Str(L"previousHash") && Hash(token) != state.Str(L"tokenHash")) return state.Str(L"previousProfileId");
        return state.Str(L"profileId");
    }
    void Use(const Profile& profile) {
        Lock lock;
        auto state = ReadObject(StatePath());
        Apply(profile, state);
    }
    void Restore() {
        Lock lock;
        auto state = ReadObject(StatePath());
        if (state.Str(L"profileId").empty()) return;
        auto path = ConfigPath(); auto cfg = ReadObject(path); auto token = ConfigToken(cfg);
        if (!Owned(state, token)) {
            PrivateWrite(StatePath(), json::Value::MakeObject());
            throw std::wstring(Tr(L"The Claude Code token was changed outside the switcher. It was kept and automatic syncing is now off", L"Токен Claude Code изменён вне переключателя. Он сохранён, автоматическая синхронизация отключена"));
        }
        if (!token.empty()) {
            if (state.Flag(L"hadToken", false)) cfg[L"env"][TokenEnv] = L"";
            else cfg[L"env"].Remove(TokenEnv);
            if (cfg[L"env"].obj.empty() && !state.Flag(L"hadEnv", false)) cfg.Remove(L"env");
            PrivateWrite(path, cfg);
        }
        PrivateWrite(StatePath(), json::Value::MakeObject());
    }
    void Sync() {
        Lock lock;
        auto state = ReadObject(StatePath()); auto id = state.Str(L"profileId");
        if (id.empty()) return;
        Profile profile;
        if (!Core::Resolve(id, profile)) throw std::wstring(Tr(L"The Claude Code profile no longer exists. Restore the previous login in Settings", L"Профиля Claude Code больше нет. Верните прежний вход в настройках"));
        Apply(profile, state);
    }
}
