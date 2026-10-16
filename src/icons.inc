namespace {
    // Bumped whenever the drawing changes: Explorer caches icons by path, so a new look
    // needs new file names to show up.
    const wchar_t* IconStyle = L"v2";

    template <class T> struct Com { T* p = nullptr; ~Com() { if (p) p->Release(); } T** operator&() { return &p; } T* operator->() { return p; } operator T*() { return p; } };

    D2D1_COLOR_F Lighten(D2D1_COLOR_F c, float t) { return D2D1::ColorF(c.r + (1 - c.r) * t, c.g + (1 - c.g) * t, c.b + (1 - c.b) * t, c.a); }
    D2D1_COLOR_F Darken(D2D1_COLOR_F c, float t) { return D2D1::ColorF(c.r * (1 - t), c.g * (1 - t), c.b * (1 - t), c.a); }

    // Top-lit vertical gradient, the way Fluent icons shade a solid shape.
    Com<ID2D1LinearGradientBrush> Gradient(ID2D1RenderTarget* rt, D2D1_COLOR_F top, D2D1_COLOR_F bottom, float y0, float y1) {
        D2D1_GRADIENT_STOP stops[] = { { 0, top }, { 1, bottom } };
        Com<ID2D1GradientStopCollection> coll; rt->CreateGradientStopCollection(stops, 2, &coll);
        Com<ID2D1LinearGradientBrush> b;
        rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({ 0, y0 }, { 0, y1 }), coll, &b);
        return b;
    }

    // Everything outside a circle: drawing through it cuts the gap that keeps a badge
    // apart from what is behind it, instead of painting a white ring.
    Com<ID2D1PathGeometry> Outside(float cx, float cy, float r, float size) {
        Com<ID2D1RectangleGeometry> all; D2D->CreateRectangleGeometry(D2D1::RectF(-1, -1, size + 1, size + 1), &all);
        Com<ID2D1EllipseGeometry> hole; D2D->CreateEllipseGeometry(D2D1::Ellipse({ cx, cy }, r, r), &hole);
        Com<ID2D1PathGeometry> path; D2D->CreatePathGeometry(&path);
        Com<ID2D1GeometrySink> sink; path->Open(&sink);
        all->CombineWithGeometry(hole, D2D1_COMBINE_MODE_EXCLUDE, nullptr, sink);
        sink->Close();
        return path;
    }

    void PushMask(ID2D1RenderTarget* rt, ID2D1Geometry* g) {
        rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), g, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE), nullptr);
    }

    // A person, as in the Windows "people" glyph: a head and the top of the shoulders,
    // clipped to the coin it sits on.
    void Person(ID2D1RenderTarget* rt, float cx, float cy, float r, ID2D1Brush* b) {
        rt->FillEllipse(D2D1::Ellipse({ cx, cy - r * .22f }, r * .33f, r * .33f), b);
        Com<ID2D1EllipseGeometry> coin; D2D->CreateEllipseGeometry(D2D1::Ellipse({ cx, cy }, r * .999f, r * .999f), &coin);
        PushMask(rt, coin);
        rt->FillEllipse(D2D1::Ellipse({ cx, cy + r * .80f }, r * .66f, r * .52f), b);
        rt->PopLayer();
    }

    // The switcher's own mark: two account coins, one in front of the other. Coral is
    // Claude's colour; the slate one behind reads as "another account".
    void DrawApp(ID2D1RenderTarget* rt, float s) {
        float k = s / 256.f;
        auto coral = Rgba(0xD9704C), slate = Rgba(0x55617A);
        float bx = 92 * k, by = 96 * k, br = 78 * k;      // back coin
        float fx = 158 * k, fy = 158 * k, fr = 92 * k;    // front coin
        float gap = std::max(1.f, 10 * k);
        auto out = Outside(fx, fy, fr + gap, s);
        PushMask(rt, out);
        auto back = Gradient(rt, Lighten(slate, .16f), Darken(slate, .14f), by - br, by + br);
        rt->FillEllipse(D2D1::Ellipse({ bx, by }, br, br), back);
        if (s >= 24) {
            Com<ID2D1SolidColorBrush> soft; rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, .78f), &soft);
            Person(rt, bx, by, br, soft);
        }
        rt->PopLayer();
        auto front = Gradient(rt, Lighten(coral, .14f), Darken(coral, .16f), fy - fr, fy + fr);
        rt->FillEllipse(D2D1::Ellipse({ fx, fy }, fr, fr), front);
        Com<ID2D1SolidColorBrush> white; rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &white);
        Person(rt, fx, fy, fr, white);
        // A hairline on the lower edge gives the coin a little depth at large sizes.
        if (s >= 48) {
            Com<ID2D1SolidColorBrush> rim; rt->CreateSolidColorBrush(Darken(coral, .35f), &rim);
            rim->SetOpacity(.35f);
            rt->DrawEllipse(D2D1::Ellipse({ fx, fy }, fr - .5f * k, fr - .5f * k), rim, std::max(1.f, 2 * k));
        }
    }

    // Claude's icon with a small coloured badge in the corner: initials from 32 px up,
    // a plain dot below that, where letters would only be a smudge.
    void DrawBadged(ID2D1RenderTarget* rt, float s, const std::wstring& letters, COLORREF color, bool plain, bool plus) {
        float d = plain ? 0 : std::round(s * (s >= 32 ? .52f : .50f)), cx = s - d / 2, cy = s - d / 2;
        float gap = std::max(1.f, std::round(s * .045f));
        HICON icon = ClaudeIcon(std::max((int)s, 32));
        Com<ID2D1Bitmap> logo; logo.p = BitmapFromIcon(rt, icon);
        if (icon) DestroyIcon(icon);
        if (!plain) { auto out = Outside(cx, cy, d / 2 + gap, s); PushMask(rt, out); }
        if (logo) rt->DrawBitmap(logo, D2D1::RectF(0, 0, s, s), 1, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        if (plain) return;
        rt->PopLayer();

        auto base = FromColorref(color);
        auto fill = Gradient(rt, Lighten(base, .22f), Darken(base, .08f), cy - d / 2, cy + d / 2);
        rt->FillEllipse(D2D1::Ellipse({ cx, cy }, d / 2, d / 2), fill);
        Com<ID2D1SolidColorBrush> white; rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &white);
        if (plus) {
            float a = d * .24f, w = std::max(1.5f, d * .11f);
            Com<ID2D1StrokeStyle> round;
            D2D->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND), nullptr, 0, &round);
            rt->DrawLine({ cx - a, cy }, { cx + a, cy }, white, w, round);
            rt->DrawLine({ cx, cy - a }, { cx, cy + a }, white, w, round);
        } else if (s >= 32) {
            auto f = Font(d * (letters.size() > 1 ? .40f : .50f), DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
            Com<IDWriteTextLayout> l;
            DW->CreateTextLayout(letters.c_str(), (UINT32)letters.size(), f, d, d, &l);
            l->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); l->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            rt->DrawTextLayout({ cx - d / 2, cy - d / 2 }, l, white);
        }
    }

    std::vector<BYTE> RenderPng(int size, const std::function<void(ID2D1RenderTarget*, float)>& draw) {
        std::vector<BYTE> out;
        Com<IWICBitmap> bmp; Com<ID2D1RenderTarget> rt;
        WIC->CreateBitmap(size, size, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bmp);
        auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
        if (!bmp || FAILED(D2D->CreateWicBitmapRenderTarget(bmp, props, &rt))) return out;
        rt->BeginDraw();
        rt->Clear(D2D1::ColorF(0, 0, 0, 0));
        draw(rt, (float)size);
        rt->EndDraw();

        Com<IStream> stream; CreateStreamOnHGlobal(nullptr, TRUE, &stream);
        Com<IWICBitmapEncoder> enc; Com<IWICBitmapFrameEncode> frame;
        WIC->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
        enc->Initialize(stream, WICBitmapEncoderNoCache);
        enc->CreateNewFrame(&frame, nullptr);
        frame->Initialize(nullptr); frame->SetSize(size, size);
        WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA; frame->SetPixelFormat(&fmt);
        frame->WriteSource(bmp, nullptr); frame->Commit(); enc->Commit();
        STATSTG st; stream->Stat(&st, STATFLAG_NONAME);
        out.resize((size_t)st.cbSize.QuadPart);
        LARGE_INTEGER zero{}; stream->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG got; stream->Read(out.data(), (ULONG)out.size(), &got);
        return out;
    }

    void WriteIco(const std::wstring& path, const std::function<void(ID2D1RenderTarget*, float)>& draw) {
        std::vector<int> sizes = { 16, 20, 24, 32, 40, 48, 64, 256 };
        std::vector<std::vector<BYTE>> pngs;
        for (int s : sizes) pngs.push_back(RenderPng(s, draw));
        size_t n = sizes.size();
        std::vector<BYTE> file(6 + 16 * n);
        auto w16 = [&](size_t at, WORD v) { file[at] = (BYTE)v; file[at + 1] = (BYTE)(v >> 8); };
        auto w32 = [&](size_t at, DWORD v) { for (int i = 0; i < 4; i++) file[at + i] = (BYTE)(v >> (8 * i)); };
        w16(2, 1); w16(4, (WORD)n);
        DWORD offset = (DWORD)file.size();
        for (size_t i = 0; i < n; i++) {
            size_t e = 6 + 16 * i;
            file[e] = file[e + 1] = (BYTE)(sizes[i] >= 256 ? 0 : sizes[i]);
            w16(e + 4, 1); w16(e + 6, 32); w32(e + 8, (DWORD)pngs[i].size()); w32(e + 12, offset);
            offset += (DWORD)pngs[i].size();
        }
        for (auto& p : pngs) file.insert(file.end(), p.begin(), p.end());
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        DWORD wrote; WriteFile(h, file.data(), (DWORD)file.size(), &wrote, nullptr); CloseHandle(h);
    }

    void RemoveIconsFor(const std::wstring& id) {
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((Paths::IconDir + L"\\*.ico").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        auto lid = Util::Lower(id);
        do {
            std::wstring base = Util::Lower(fd.cFileName); base = base.substr(0, base.size() - 4);
            if (base == lid || base.rfind(lid + L"-", 0) == 0) DeleteFileW((Paths::IconDir + L"\\" + fd.cFileName).c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    std::wstring IconFile(const std::wstring& id, const std::wstring& letters, COLORREF color, bool plain, bool plus = false) {
        // The letters are in the file name because Explorer caches icons by path.
        auto file = Paths::IconDir + L"\\" + id + (plain ? L"" : L"-" + letters) + L"-" + IconStyle + L".ico";
        if (Util::Exists(file)) return file;
        CreateDirectoryW(Paths::Home.c_str(), nullptr);
        CreateDirectoryW(Paths::IconDir.c_str(), nullptr);
        RemoveIconsFor(id);
        WriteIco(file, [&](ID2D1RenderTarget* rt, float s) { DrawBadged(rt, s, letters, color, plain, plus); });
        return file;
    }
}

namespace Core {
    std::wstring ProfileIconPath(const Profile& p) {
        if (p.isDefault) return IconFile(p.id, L"", 0, true);
        return IconFile(p.id, BadgeLetter(p.name), ProfileColor(p.id), false);
    }
    std::wstring AddAccountIconPath() { return IconFile(L"add-account", L"plus", RGB(46, 125, 74), false, true); }
    void WriteAppIcon(const std::wstring& path) { WriteIco(path, DrawApp); }

    // For reviewing the look: the app icon and a few account icons, written to a folder.
    void WriteIconSamples(const std::wstring& dir) {
        WriteIco(dir + L"\\app.ico", DrawApp);
        WriteIco(dir + L"\\default.ico", [](ID2D1RenderTarget* rt, float s) { DrawBadged(rt, s, L"", 0, true, false); });
        WriteIco(dir + L"\\a2.ico", [](ID2D1RenderTarget* rt, float s) { DrawBadged(rt, s, L"A2", RGB(66, 133, 180), false, false); });
        WriteIco(dir + L"\\w.ico", [](ID2D1RenderTarget* rt, float s) { DrawBadged(rt, s, L"W", RGB(150, 96, 180), false, false); });
        WriteIco(dir + L"\\add.ico", [](ID2D1RenderTarget* rt, float s) { DrawBadged(rt, s, L"", RGB(46, 125, 74), false, true); });
        // And the accounts that exist here (the demo's, for the README pictures): account-1.ico, ...
        int n = 0;
        for (auto& p : ProfileList(false)) {
            auto letters = BadgeLetter(DisplayName(p)); auto color = ProfileColor(p.id); bool plain = p.isDefault;
            WriteIco(Util::Format(L"%s\\account-%d.ico", dir.c_str(), ++n), [&](ID2D1RenderTarget* rt, float s) { DrawBadged(rt, s, letters, color, plain, false); });
        }
    }
