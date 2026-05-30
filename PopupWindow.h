// PopupWindow.cpp
// Custom Win32 tray popup rendered with Direct2D.
// Replaces the WinRT DevicePicker flyout from the original AudioPlaybackConnector.

#include "pch.h"
#include "PopupWindow.h"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

// ─────────────────────────────────────────────────────────────
//  Colour palettes
// ─────────────────────────────────────────────────────────────
struct Palette
{
    D2D1_COLOR_F bg;        // window background
    D2D1_COLOR_F surface;   // card / row background
    D2D1_COLOR_F text;
    D2D1_COLOR_F subtext;
    D2D1_COLOR_F accent;    // blue
    D2D1_COLOR_F green;
    D2D1_COLOR_F hover;
    D2D1_COLOR_F sliderBg;
    D2D1_COLOR_F divider;
};

static Palette DarkPalette()
{
    return {
        { 0.114f, 0.114f, 0.118f, 1.f },   // bg  #1D1D1E
        { 0.176f, 0.176f, 0.180f, 1.f },   // surface #2D2D2E
        { 1.f,    1.f,    1.f,    1.f },   // text
        { 0.6f,   0.6f,   0.6f,   1.f },   // subtext
        { 0.000f, 0.478f, 1.000f, 1.f },   // accent #007AFF
        { 0.196f, 0.843f, 0.294f, 1.f },   // green  #32D74B
        { 1.f,    1.f,    1.f,    0.07f},   // hover overlay
        { 0.3f,   0.3f,   0.3f,   1.f },   // slider bg
        { 1.f,    1.f,    1.f,    0.10f},   // divider
    };
}

static Palette LightPalette()
{
    return {
        { 0.957f, 0.957f, 0.961f, 1.f },   // bg  #F4F4F5
        { 1.f,    1.f,    1.f,    1.f },   // surface white
        { 0.f,    0.f,    0.f,    1.f },   // text
        { 0.40f,  0.40f,  0.40f,  1.f },   // subtext
        { 0.000f, 0.478f, 1.000f, 1.f },   // accent
        { 0.118f, 0.671f, 0.208f, 1.f },   // green
        { 0.f,    0.f,    0.f,    0.06f},   // hover
        { 0.85f,  0.85f,  0.85f,  1.f },   // slider bg
        { 0.f,    0.f,    0.f,    0.12f},   // divider
    };
}

// ─────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────
static inline D2D1_RECT_F RectF(float l, float t, float r, float b)
{
    return D2D1::RectF(l, t, r, b);
}

static inline D2D1_ROUNDED_RECT RoundedRectF(float l, float t, float r, float b, float rx, float ry = -1.f)
{
    return D2D1::RoundedRect(D2D1::RectF(l, t, r, b), rx, ry < 0 ? rx : ry);
}

static bool QueryDarkMode()
{
    DWORD value = 0;
    DWORD size  = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_DWORD, nullptr, &value, &size);
    return value == 0;   // 0 = dark, 1 = light
}

// ─────────────────────────────────────────────────────────────
//  Window class name
// ─────────────────────────────────────────────────────────────
static constexpr wchar_t kPopupClass[] = L"BTAudioPopup";

// ─────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────
PopupWindow::PopupWindow(
    HINSTANCE hInst, HWND hwndHost,
    const std::unordered_map<std::wstring, std::wstring>& devices,
    const std::wstring& connectedId,
    std::function<void(const std::wstring&)> fnConnect,
    std::function<void()> fnDisconnect)
    : m_hwndHost(hwndHost)
    , m_devices(devices)
    , m_connectedId(connectedId)
    , m_fnConnect(std::move(fnConnect))
    , m_fnDisconnect(std::move(fnDisconnect))
{
    // D2D / DWrite factories
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                      IID_PPV_ARGS(&m_d2dFactory));
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                        __uuidof(IDWriteFactory3),
                        reinterpret_cast<IUnknown**>(m_dwFactory.GetAddressOf()));

    RegisterClass(hInst);
    CreatePopupHwnd(hInst);
    CreateDeviceResources();
    RebuildOtherList();
}

PopupWindow::~PopupWindow()
{
    if (m_hwnd) DestroyWindow(m_hwnd);
}

// ─────────────────────────────────────────────────────────────
//  Win32 window class
// ─────────────────────────────────────────────────────────────
void PopupWindow::RegisterClass(HINSTANCE hInst)
{
    WNDCLASSEXW wcx = {};
    wcx.cbSize       = sizeof(wcx);
    wcx.lpfnWndProc  = s_WndProc;
    wcx.hInstance    = hInst;
    wcx.hCursor      = LoadCursorW(nullptr, IDC_ARROW);
    wcx.lpszClassName = kPopupClass;
    RegisterClassExW(&wcx);
}

void PopupWindow::CreatePopupHwnd(HINSTANCE hInst)
{
    int h = ComputeWindowHeight();
    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        kPopupClass, L"",
        WS_POPUP,
        0, 0, kPopupW, h,
        nullptr, nullptr, hInst, this);

    // Rounded corners (Windows 11)
    DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));

    // Drop shadow
    MARGINS m = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(m_hwnd, &m);
}

// ─────────────────────────────────────────────────────────────
//  Static WndProc dispatcher
// ─────────────────────────────────────────────────────────────
LRESULT CALLBACK PopupWindow::s_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PopupWindow* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<PopupWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    else
    {
        self = reinterpret_cast<PopupWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->WndProc(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─────────────────────────────────────────────────────────────
//  Instance WndProc
// ─────────────────────────────────────────────────────────────
LRESULT PopupWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;   // handled by D2D

    case WM_ACTIVATE:
        if (wp == WA_INACTIVE)
            Hide();
        return 0;

    // ── Mouse ───────────────────────────────────────────────
    case WM_MOUSEMOVE:
    {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        auto hr   = HitTest(pt);
        if (hr.zone != m_hover.zone || hr.index != m_hover.index)
        {
            m_hover = hr;
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        if (m_draggingSlider)
        {
            float sliderW = kPopupW - kPaddingX * 2;
            float x = static_cast<float>(pt.x) - kPaddingX;
            SetVolume(std::clamp(x / sliderW, 0.f, 1.f));
        }

        // Track mouse-leave
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        m_hover = { HitZone::None, -1 };
        m_draggingSlider = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN:
    {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        auto hr   = HitTest(pt);
        if (hr.zone == HitZone::SliderTrack)
        {
            m_draggingSlider = true;
            SetCapture(hwnd);
            float sliderW = kPopupW - kPaddingX * 2;
            float x = static_cast<float>(pt.x) - kPaddingX;
            SetVolume(std::clamp(x / sliderW, 0.f, 1.f));
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        if (m_draggingSlider)
        {
            m_draggingSlider = false;
            ReleaseCapture();
            return 0;
        }

        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        auto hr   = HitTest(pt);

        if (hr.zone == HitZone::DeviceRow && hr.index >= 0 &&
            hr.index < static_cast<int>(m_otherIds.size()))
        {
            m_fnConnect(m_otherIds[hr.index]);
        }
        else if (hr.zone == HitZone::BtnSettings)
        {
            ShellExecuteW(nullptr, L"open",
                          L"ms-settings:bluetooth", nullptr, nullptr, SW_SHOWNORMAL);
            Hide();
        }
        else if (hr.zone == HitZone::BtnDisconnect)
        {
            m_fnDisconnect();
        }
        return 0;
    }

    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        DiscardDeviceResources();
        CreateDeviceResources();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─────────────────────────────────────────────────────────────
//  Show / Hide / Toggle
// ─────────────────────────────────────────────────────────────
void PopupWindow::PositionAboveTaskbar()
{
    // Find taskbar and position popup just above it
    HWND hTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    RECT rcTask   = {};
    if (hTaskbar) GetWindowRect(hTaskbar, &rcTask);

    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);

    int h = ComputeWindowHeight();
    int w = kPopupW;

    // X: align to right edge of work area, a bit inset
    int x = rcWork.right - w - 12;
    // Y: just above taskbar
    int y = (hTaskbar ? rcTask.top : rcWork.bottom) - h - 8;

    // Resize in case device count changed
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // Recreate render target if size changed
    if (m_rt)
    {
        D2D1_SIZE_U sz = { static_cast<UINT32>(w), static_cast<UINT32>(h) };
        m_rt->Resize(sz);
    }
}

void PopupWindow::Show()
{
    RebuildOtherList();
    PositionAboveTaskbar();
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
    m_visible = true;
}

void PopupWindow::Hide()
{
    ShowWindow(m_hwnd, SW_HIDE);
    m_visible = false;
}

void PopupWindow::Toggle()
{
    if (m_visible) Hide(); else Show();
}

bool PopupWindow::IsVisible() const { return m_visible; }

// ─────────────────────────────────────────────────────────────
//  Notifications from host
// ─────────────────────────────────────────────────────────────
void PopupWindow::OnDeviceListChanged()
{
    RebuildOtherList();
    if (m_visible)
    {
        PositionAboveTaskbar();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void PopupWindow::OnConnectionChanged()
{
    RebuildOtherList();
    if (m_visible)
        InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ─────────────────────────────────────────────────────────────
//  Device list management
// ─────────────────────────────────────────────────────────────
void PopupWindow::RebuildOtherList()
{
    m_otherIds.clear();
    for (auto& [id, name] : m_devices)
        if (id != m_connectedId)
            m_otherIds.push_back(id);

    // Sort by name for stability
    std::sort(m_otherIds.begin(), m_otherIds.end(),
              [&](const std::wstring& a, const std::wstring& b) {
                  return m_devices.at(a) < m_devices.at(b);
              });

    if (m_otherIds.size() > kMaxOtherDevices)
        m_otherIds.resize(kMaxOtherDevices);
}

// ─────────────────────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────────────────────
int PopupWindow::ComputeWindowHeight() const
{
    float h = kPaddingY + kHeaderH;
    if (!m_connectedId.empty())
        h += kConnectedH + kSliderH + kDividerH;
    if (!m_otherIds.empty())
        h += kSectionLabelH + m_otherIds.size() * kDeviceRowH + kDividerH;
    h += kFooterH + kPaddingY;
    return static_cast<int>(h);
}

// ─────────────────────────────────────────────────────────────
//  Volume
// ─────────────────────────────────────────────────────────────
void PopupWindow::SetVolume(float v)
{
    m_volume = v;
    // In a real build you'd call IAudioEndpointVolume here.
    // For now, just redraw.
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ─────────────────────────────────────────────────────────────
//  Hit Testing
// ─────────────────────────────────────────────────────────────
bool PopupWindow::IsDarkMode() const { return QueryDarkMode(); }

PopupWindow::HitResult PopupWindow::HitTest(POINT pt) const
{
    float x = static_cast<float>(pt.x);
    float y = static_cast<float>(pt.y);

    // Volume slider
    if (!m_connectedId.empty())
    {
        float sliderTop = m_sliderY + 14.f;
        float sliderBot = sliderTop + 20.f;
        if (y >= sliderTop && y <= sliderBot)
            return { HitZone::SliderTrack, -1 };
    }

    // Device rows
    if (!m_otherIds.empty())
    {
        for (int i = 0; i < static_cast<int>(m_otherIds.size()); ++i)
        {
            float rowTop = m_devicesY0 + i * kDeviceRowH;
            float rowBot = rowTop + kDeviceRowH;
            if (y >= rowTop && y < rowBot)
                return { HitZone::DeviceRow, i };
        }
    }

    // Footer buttons
    if (y >= m_footerY && y <= m_winH)
    {
        float mid = kPopupW / 2.f;
        if (x < mid) return { HitZone::BtnSettings, -1 };
        else          return { HitZone::BtnDisconnect, -1 };
    }

    return { HitZone::None, -1 };
}

// ─────────────────────────────────────────────────────────────
//  D2D resource creation
// ─────────────────────────────────────────────────────────────
void PopupWindow::CreateDeviceResources()
{
    if (!m_rt)
    {
        RECT rc; GetClientRect(m_hwnd, &rc);
        D2D1_RENDER_TARGET_PROPERTIES rtp = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED));
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwrtp =
            D2D1::HwndRenderTargetProperties(
                m_hwnd,
                D2D1::SizeU(rc.right, rc.bottom),
                D2D1_PRESENT_OPTIONS_NONE);
        m_d2dFactory->CreateHwndRenderTarget(rtp, hwrtp, &m_rt);
    }

    Palette p = IsDarkMode() ? DarkPalette() : LightPalette();

    auto MakeBrush = [&](const D2D1_COLOR_F& c, ComPtr<ID2D1SolidColorBrush>& br) {
        m_rt->CreateSolidColorBrush(c, &br);
    };

    MakeBrush(p.bg,       m_brushBg);
    MakeBrush(p.surface,  m_brushSurface);
    MakeBrush(p.text,     m_brushText);
    MakeBrush(p.subtext,  m_brushSubtext);
    MakeBrush(p.accent,   m_brushAccent);
    MakeBrush(p.green,    m_brushGreen);
    MakeBrush(p.hover,    m_brushHover);
    MakeBrush(p.sliderBg, m_brushSliderBg);
    MakeBrush(p.divider,  m_brushDivider);

    // Text formats
    auto MakeFmt = [&](float size, DWRITE_FONT_WEIGHT wt,
                        ComPtr<IDWriteTextFormat>& fmt) {
        m_dwFactory->CreateTextFormat(
            L"Segoe UI Variable", nullptr,
            wt, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size, L"en-US", &fmt);
        if (fmt) fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    };

    MakeFmt(13.f, DWRITE_FONT_WEIGHT_SEMI_BOLD,  m_fmtTitle);
    MakeFmt(14.f, DWRITE_FONT_WEIGHT_SEMI_BOLD,  m_fmtDevice);
    MakeFmt(11.f, DWRITE_FONT_WEIGHT_NORMAL,     m_fmtSub);
    MakeFmt(10.f, DWRITE_FONT_WEIGHT_MEDIUM,     m_fmtLabel);
    MakeFmt(13.f, DWRITE_FONT_WEIGHT_NORMAL,     m_fmtButton);
}

void PopupWindow::DiscardDeviceResources()
{
    m_rt.Reset();
    m_brushBg.Reset(); m_brushSurface.Reset(); m_brushText.Reset();
    m_brushSubtext.Reset(); m_brushAccent.Reset(); m_brushGreen.Reset();
    m_brushHover.Reset(); m_brushSliderBg.Reset(); m_brushDivider.Reset();
}

// ─────────────────────────────────────────────────────────────
//  Rendering
// ─────────────────────────────────────────────────────────────
void PopupWindow::Render()
{
    if (!m_rt) CreateDeviceResources();
    if (!m_rt) return;

    RECT rc; GetClientRect(m_hwnd, &rc);
    float W = static_cast<float>(rc.right);
    float H = static_cast<float>(rc.bottom);
    m_winH  = H;

    m_rt->BeginDraw();
    m_rt->SetTransform(D2D1::Matrix3x2F::Identity());

    float y = kPaddingY;

    DrawBackground(m_rt.Get(), W, H);
    DrawHeader(m_rt.Get(), W, y);

    if (!m_connectedId.empty())
    {
        DrawConnectedSection(m_rt.Get(), W, y);
        DrawVolumeSlider(m_rt.Get(), W, y);
    }

    if (!m_otherIds.empty())
    {
        DrawDivider(m_rt.Get(), W, y);
        DrawOtherDevices(m_rt.Get(), W, y);
    }

    DrawDivider(m_rt.Get(), W, y);
    DrawFooter(m_rt.Get(), W, y);

    HRESULT hr = m_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
        DiscardDeviceResources();
}

// ── Individual drawing routines ─────────────────────────────

void PopupWindow::DrawBackground(ID2D1HwndRenderTarget* dc, float w, float h)
{
    dc->Clear(D2D1::ColorF(0, 0));   // transparent (DWM composites)
    dc->FillRoundedRectangle(
        RoundedRectF(0, 0, w, h, kCornerRadius),
        m_brushBg.Get());
}

void PopupWindow::DrawHeader(ID2D1HwndRenderTarget* dc, float w, float& y)
{
    // App title
    auto rect = RectF(kPaddingX, y, w - kPaddingX, y + kHeaderH);
    dc->DrawText(L"BT Audio Receiver", 17,
                 m_fmtTitle.Get(), rect, m_brushText.Get());
    y += kHeaderH;
}

void PopupWindow::DrawConnectedSection(ID2D1HwndRenderTarget* dc, float w, float& y)
{
    // Surface card
    dc->FillRoundedRectangle(
        RoundedRectF(kPaddingX, y, w - kPaddingX, y + kConnectedH, 8.f),
        m_brushSurface.Get());

    // Green dot
    float dotR = 5.f;
    float dotX = kPaddingX + 16.f;
    float dotY = y + kConnectedH / 2.f;
    dc->FillEllipse(D2D1::Ellipse({ dotX, dotY }, dotR, dotR),
                    m_brushGreen.Get());

    // Device name
    auto it = m_devices.find(m_connectedId);
    const std::wstring& name = (it != m_devices.end()) ? it->second : L"Unknown Device";

    float textX = dotX + dotR + 10.f;
    dc->DrawText(name.c_str(), static_cast<UINT32>(name.size()),
                 m_fmtDevice.Get(),
                 RectF(textX, y, w - kPaddingX - 8.f, y + kConnectedH * 0.6f),
                 m_brushText.Get());

    // "Connected" subtitle
    dc->DrawText(L"Connected", 9,
                 m_fmtSub.Get(),
                 RectF(textX, y + kConnectedH * 0.5f, w - kPaddingX - 8.f, y + kConnectedH),
                 m_brushGreen.Get());

    y += kConnectedH;
}

void PopupWindow::DrawVolumeSlider(ID2D1HwndRenderTarget* dc, float w, float& y)
{
    m_sliderY = y;

    float trackX  = kPaddingX;
    float trackY  = y + 14.f;
    float trackW  = w - kPaddingX * 2;
    float trackH  = 4.f;
    float fillW   = trackW * m_volume;
    float thumbR  = 7.f;

    // Volume icon (simple speaker glyph using text — Segoe MDL2)
    dc->DrawText(L"\uE767", 1,   // Speaker icon in Segoe MDL2 Assets
                 m_fmtSub.Get(),
                 RectF(trackX, y, trackX + 20.f, y + kSliderH),
                 m_brushSubtext.Get());

    float sx = trackX + 24.f;

    // Track bg
    dc->FillRoundedRectangle(
        RoundedRectF(sx, trackY, sx + trackW - 24.f, trackY + trackH, 2.f),
        m_brushSliderBg.Get());

    // Fill
    float fw = (trackW - 24.f) * m_volume;
    if (fw > 0.f)
        dc->FillRoundedRectangle(
            RoundedRectF(sx, trackY, sx + fw, trackY + trackH, 2.f),
            m_brushAccent.Get());

    // Thumb
    float thumbX = sx + fw;
    float thumbY = trackY + trackH / 2.f;
    dc->FillEllipse(D2D1::Ellipse({ thumbX, thumbY }, thumbR, thumbR),
                    m_brushAccent.Get());

    y += kSliderH;
}

void PopupWindow::DrawDivider(ID2D1HwndRenderTarget* dc, float w, float& y)
{
    dc->FillRectangle(RectF(kPaddingX, y, w - kPaddingX, y + kDividerH),
                      m_brushDivider.Get());
    y += kDividerH + 4.f;
}

void PopupWindow::DrawOtherDevices(ID2D1HwndRenderTarget* dc, float w, float& y)
{
    // Section label
    dc->DrawText(L"OTHER DEVICES", 13,
                 m_fmtLabel.Get(),
                 RectF(kPaddingX, y, w - kPaddingX, y + kSectionLabelH),
                 m_brushSubtext.Get());
    y += kSectionLabelH;

    m_devicesY0 = y;

    for (int i = 0; i < static_cast<int>(m_otherIds.size()); ++i)
    {
        float rowTop = y + i * kDeviceRowH;
        float rowBot = rowTop + kDeviceRowH;

        // Hover highlight
        bool hovered = (m_hover.zone == HitZone::DeviceRow && m_hover.index == i);
        if (hovered)
            dc->FillRoundedRectangle(
                RoundedRectF(kPaddingX, rowTop, w - kPaddingX, rowBot, 6.f),
                m_brushHover.Get());

        // BT icon
        dc->DrawText(L"\uE702", 1,   // Bluetooth glyph
                     m_fmtSub.Get(),
                     RectF(kPaddingX + 6.f, rowTop, kPaddingX + 28.f, rowBot),
                     m_brushSubtext.Get());

        // Name
        const auto& name = m_devices.at(m_otherIds[i]);
        dc->DrawText(name.c_str(), static_cast<UINT32>(name.size()),
                     m_fmtDevice.Get(),
                     RectF(kPaddingX + 32.f, rowTop, w - kPaddingX - 8.f, rowBot),
                     m_brushText.Get());
    }

    y += m_otherIds.size() * kDeviceRowH;
}

void PopupWindow::DrawFooter(ID2D1HwndRenderTarget* dc, float w, float& y)
{
    m_footerY = y;

    float mid     = w / 2.f;
    float btnH    = kFooterH;
    bool  hovSett = (m_hover.zone == HitZone::BtnSettings);
    bool  hovDisc = (m_hover.zone == HitZone::BtnDisconnect);

    // Settings button (left half)
    if (hovSett)
        dc->FillRoundedRectangle(
            RoundedRectF(kPaddingX, y, mid - 4.f, y + btnH - kPaddingY, 6.f),
            m_brushHover.Get());

    dc->DrawText(L"Settings", 8,
                 m_fmtButton.Get(),
                 RectF(kPaddingX, y, mid - 4.f, y + btnH - kPaddingY),
                 m_brushText.Get());

    // Disconnect button (right half) — accent coloured if connected
    bool connected = !m_connectedId.empty();
    if (hovDisc)
        dc->FillRoundedRectangle(
            RoundedRectF(mid + 4.f, y, w - kPaddingX, y + btnH - kPaddingY, 6.f),
            m_brushHover.Get());

    dc->DrawText(L"Disconnect", 10,
                 m_fmtButton.Get(),
                 RectF(mid + 4.f, y, w - kPaddingX, y + btnH - kPaddingY),
                 connected ? m_brushAccent.Get() : m_brushSubtext.Get());
}
