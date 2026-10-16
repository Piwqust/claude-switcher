// Minimal JSON: enough for settings.json, chat session files and plan usage history.
// Objects keep key order so a rewrite of settings.json stays readable and diffable.
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace json {

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double n = 0;
    std::wstring s;
    std::vector<Value> arr;
    std::vector<std::pair<std::wstring, Value>> obj;

    Value() = default;
    Value(bool v) : type(Bool), b(v) {}
    Value(int v) : type(Number), n(v) {}
    Value(double v) : type(Number), n(v) {}
    Value(const wchar_t* v) : type(String), s(v) {}
    Value(std::wstring v) : type(String), s(std::move(v)) {}
    static Value MakeArray() { Value v; v.type = Array; return v; }
    static Value MakeObject() { Value v; v.type = Object; return v; }

    bool IsNull() const { return type == Null; }
    const Value* Find(const std::wstring& key) const {
        if (type != Object) return nullptr;
        for (auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    Value& operator[](const std::wstring& key) {
        if (type != Object) { *this = MakeObject(); }
        for (auto& kv : obj) if (kv.first == key) return kv.second;
        obj.emplace_back(key, Value());
        return obj.back().second;
    }
    void Remove(const std::wstring& key) {
        for (size_t i = 0; i < obj.size(); i++) if (obj[i].first == key) { obj.erase(obj.begin() + i); return; }
    }
    std::wstring Str(const std::wstring& key, const std::wstring& def = L"") const {
        auto v = Find(key); return v && v->type == String ? v->s : def;
    }
    double Num(const std::wstring& key, double def = 0) const {
        auto v = Find(key); return v && v->type == Number ? v->n : def;
    }
    bool Flag(const std::wstring& key, bool def) const {
        auto v = Find(key); return v && v->type == Bool ? v->b : def;
    }
};

class Parser {
    const wchar_t* p; const wchar_t* end; bool ok = true;
    void Ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++; }
    bool Lit(const wchar_t* w) {
        const wchar_t* q = p; while (*w) { if (q >= end || *q != *w) return false; q++; w++; }
        p = q; return true;
    }
    std::wstring ParseString() {
        std::wstring out; p++; // opening quote
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                p++;
                switch (*p) {
                case 'n': out += L'\n'; break; case 't': out += L'\t'; break; case 'r': out += L'\r'; break;
                case 'b': out += L'\b'; break; case 'f': out += L'\f'; break;
                case 'u': {
                    if (p + 4 >= end) { ok = false; return out; }
                    out += (wchar_t)wcstoul(std::wstring(p + 1, p + 5).c_str(), nullptr, 16); p += 4; break;
                }
                default: out += *p;
                }
                p++;
            } else out += *p++;
        }
        if (p < end) p++; else ok = false;
        return out;
    }
    Value ParseValue(int depth) {
        Ws();
        if (p >= end || depth > 64) { ok = false; return {}; }
        if (*p == '{') {
            Value v = Value::MakeObject(); p++; Ws();
            if (p < end && *p == '}') { p++; return v; }
            while (ok && p < end) {
                Ws(); if (p >= end || *p != '"') { ok = false; break; }
                auto key = ParseString(); Ws();
                if (p >= end || *p != ':') { ok = false; break; }
                p++;
                v.obj.emplace_back(key, ParseValue(depth + 1)); Ws();
                if (p < end && *p == ',') { p++; continue; }
                if (p < end && *p == '}') { p++; break; }
                ok = false;
            }
            return v;
        }
        if (*p == '[') {
            Value v = Value::MakeArray(); p++; Ws();
            if (p < end && *p == ']') { p++; return v; }
            while (ok && p < end) {
                v.arr.push_back(ParseValue(depth + 1)); Ws();
                if (p < end && *p == ',') { p++; continue; }
                if (p < end && *p == ']') { p++; break; }
                ok = false;
            }
            return v;
        }
        if (*p == '"') return Value(ParseString());
        if (Lit(L"true")) return Value(true);
        if (Lit(L"false")) return Value(false);
        if (Lit(L"null")) return Value();
        wchar_t* stop = nullptr;
        double d = wcstod(p, &stop);
        if (stop == p) { ok = false; return {}; }
        p = stop; return Value(d);
    }
public:
    static bool Parse(const std::wstring& text, Value& out) {
        Parser ps; ps.p = text.c_str(); ps.end = ps.p + text.size();
        if (ps.p < ps.end && *ps.p == 0xFEFF) ps.p++;
        out = ps.ParseValue(0);
        return ps.ok;
    }
};

inline void Escape(std::wstring& out, const std::wstring& s) {
    out += L'"';
    for (wchar_t c : s) {
        switch (c) {
        case '"': out += L"\\\""; break; case '\\': out += L"\\\\"; break;
        case '\n': out += L"\\n"; break; case '\r': out += L"\\r"; break; case '\t': out += L"\\t"; break;
        default:
            if (c < 0x20) { wchar_t b[8]; swprintf_s(b, L"\\u%04x", c); out += b; }
            else out += c;
        }
    }
    out += L'"';
}

inline void Write(std::wstring& out, const Value& v, int indent = 0) {
    std::wstring pad(indent * 2, L' '), pad2((indent + 1) * 2, L' ');
    switch (v.type) {
    case Value::Null: out += L"null"; break;
    case Value::Bool: out += v.b ? L"true" : L"false"; break;
    case Value::Number: {
        wchar_t b[64];
        if (v.n == (long long)v.n) swprintf_s(b, L"%lld", (long long)v.n); else swprintf_s(b, L"%.17g", v.n);
        out += b; break;
    }
    case Value::String: Escape(out, v.s); break;
    case Value::Array:
        if (v.arr.empty()) { out += L"[]"; break; }
        out += L"[\n";
        for (size_t i = 0; i < v.arr.size(); i++) { out += pad2; Write(out, v.arr[i], indent + 1); out += i + 1 < v.arr.size() ? L",\n" : L"\n"; }
        out += pad + L"]"; break;
    case Value::Object:
        if (v.obj.empty()) { out += L"{}"; break; }
        out += L"{\n";
        for (size_t i = 0; i < v.obj.size(); i++) {
            out += pad2; Escape(out, v.obj[i].first); out += L": "; Write(out, v.obj[i].second, indent + 1);
            out += i + 1 < v.obj.size() ? L",\n" : L"\n";
        }
        out += pad + L"}"; break;
    }
}

} // namespace json
