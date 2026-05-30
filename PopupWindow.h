#pragma once

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>

using Microsoft::WRL::ComPtr;

class PopupWindow
{
public:
    PopupWindow(
        HINSTANCE hInst,
        HWND hwndHost,
        const std::unordered_map<std::wstring, std::wstring>& devices,
        const std::wstring& connectedId,
        std::function<void(const std::wstring&)> fnConnect,
        std::function<void()> fnDisconnect);

    ~PopupWindow();

    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const;

    void OnDeviceListChanged();
    void OnConnectionChanged();

    // Call these from the host to sync state
    void SetConnectedId(const std::wstring& id) { m_connectedId = id; }
    void SetDevices(const std::unordered_map<std::wstring, std::wstring>& d) { m_devices = d; }

private:
    // ── Layout constants ────────────────────────────────────
    static constexpr int   kPopupW        = 320;
    static constexpr float kCornerRadius  = 12.f;
    static constexpr float kPaddingX      = 12.f;
    static constexpr float kPaddingY      = 12.f;
    static constexpr float kHeaderH       = 36.f;
    static constexpr float kConnectedH    = 60.f;
    static constexpr float kSliderH       = 36.f;
    static constexpr float kDividerH      = 1.f;
    static constexpr float kSectionLabelH = 24.f;
    static constexpr float kDeviceRowH    = 44.f;
    static constexpr float kFooterH       = 44.f;
    static constexpr size_t kMaxOtherDevices = 5;

    // ── Win32 ───────────────────────────────────────────────
    HWND m_hwnd     = nullptr;
    HWND m_hwndHost = nullptr;
    bool m_visible  = false;

    void RegisterClass(HINSTANCE hInst);
    void CreatePopupHwnd(HINSTANCE hInst);

    static LRESULT CALLBACK s_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    // ── Data ────────────────────────────────────────────────
    std::unordered_map<std::wstring, std::wstring> m_devices;
    std::wstring m_connectedId;
    std::vector<std::wstring> m_otherIds;

    std::function<void(const std::wstring&)> m_fnConnect;
    std::function<void()>                    m_fnDisconnect;

    void RebuildOtherList();

    // ── Layout state ────────────────────────────────────────
    float m_sliderY   = 0.f;
    float m_devicesY0 = 0.f;
    float m_footerY   = 0.f;
    float m_winH      = 0.f;
    float m_volume    = 0.5f;

    int ComputeWindowHeight() const;
    void SetVolume(float v);
    void PositionAboveTaskbar();

    // ── Hit testing ─────────────────────────────────────────
    enum class HitZone { None, SliderTrack, DeviceRow, BtnSettings, BtnDisconnect };
    struct HitResult { HitZone zone; int index; };

    HitResult m_hover         = { HitZone::None, -1 };
    bool      m_draggingSlider = false;

    HitResult HitTest(POINT pt) const;
    bool IsDarkMode() const;

    // ── Direct2D ────────────────────────────────────────────
    ComPtr<ID2D1Factory>            m_d2dFactory;
    ComPtr<ID2D1HwndRenderTarget>   m_rt;
    ComPtr<IDWriteFactory3>         m_dwFactory;

    ComPtr<ID2D1SolidColorBrush> m_brushBg;
    ComPtr<ID2D1SolidColorBrush> m_brushSurface;
    ComPtr<ID2D1SolidColorBrush> m_brushText;
    ComPtr<ID2D1SolidColorBrush> m_brushSubtext;
    ComPtr<ID2D1SolidColorBrush> m_brushAccent;
    ComPtr<ID2D1SolidColorBrush> m_brushGreen;
    ComPtr<ID2D1SolidColorBrush> m_brushHover;
    ComPtr<ID2D1SolidColorBrush> m_brushSliderBg;
    ComPtr<ID2D1SolidColorBrush> m_brushDivider;

    ComPtr<IDWriteTextFormat> m_fmtTitle;
    ComPtr<IDWriteTextFormat> m_fmtDevice;
    ComPtr<IDWriteTextFormat> m_fmtSub;
    ComPtr<IDWriteTextFormat> m_fmtLabel;
    ComPtr<IDWriteTextFormat> m_fmtButton;

    void CreateDeviceResources();
    void DiscardDeviceResources();

    // ── Rendering ───────────────────────────────────────────
    void Render();
    void DrawBackground(ID2D1HwndRenderTarget* dc, float w, float h);
    void DrawHeader(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawConnectedSection(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawVolumeSlider(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawDivider(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawOtherDevices(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawFooter(ID2D1HwndRenderTarget* dc, float w, float& y);
};
