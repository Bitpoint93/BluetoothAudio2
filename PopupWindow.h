#pragma once

// PopupWindow.h
// Custom tray popup that replaces the original DevicePicker flyout.
// Rendered with Direct2D; no XAML, no WinForms.

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

using Microsoft::WRL::ComPtr;

// ─────────────────────────────────────────────────────────────
//  Layout constants (all in logical pixels)
// ─────────────────────────────────────────────────────────────
static constexpr int   kPopupW          = 300;
static constexpr float kPaddingX        = 16.f;
static constexpr float kPaddingY        = 14.f;
static constexpr float kHeaderH         = 44.f;
static constexpr float kConnectedH      = 72.f;
static constexpr float kSliderH         = 48.f;
static constexpr float kDividerH        = 1.f;
static constexpr float kSectionLabelH   = 24.f;
static constexpr float kDeviceRowH      = 44.f;
static constexpr float kFooterH         = 52.f;
static constexpr float kCornerRadius    = 12.f;
static constexpr int   kMaxOtherDevices = 5;

// ─────────────────────────────────────────────────────────────
//  PopupWindow
// ─────────────────────────────────────────────────────────────
class PopupWindow
{
public:
    PopupWindow(
        HINSTANCE hInst,
        HWND      hwndHost,
        const std::unordered_map<std::wstring, std::wstring>& devices,
        const std::wstring&                                    connectedId,
        std::function<void(const std::wstring&)>              fnConnect,
        std::function<void()>                                  fnDisconnect);

    ~PopupWindow();

    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const;

    void OnDeviceListChanged();
    void OnConnectionChanged();

private:
    // Win32
    static LRESULT CALLBACK s_WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

    void RegisterClass(HINSTANCE);
    void CreatePopupHwnd(HINSTANCE);
    void PositionAboveTaskbar();

    // D2D / DWrite
    void CreateDeviceResources();
    void DiscardDeviceResources();
    void Render();

    // Rendering helpers — match ID2D1HwndRenderTarget used in .cpp
    void DrawBackground(ID2D1HwndRenderTarget* dc, float w, float h);
    void DrawHeader(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawConnectedSection(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawVolumeSlider(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawDivider(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawOtherDevices(ID2D1HwndRenderTarget* dc, float w, float& y);
    void DrawFooter(ID2D1HwndRenderTarget* dc, float w, float& y);

    // Layout helpers
    int  ComputeWindowHeight() const;
    bool IsDarkMode() const;

    // Hit testing
    enum class HitZone { None, SliderTrack, DeviceRow, BtnSettings, BtnDisconnect };
    struct HitResult { HitZone zone; int index; };
    HitResult HitTest(POINT pt) const;

    // Volume
    float m_volume         = 1.f;
    bool  m_draggingSlider = false;
    void  SetVolume(float v);

    // ── data ─────────────────────────────────────────────────
    // Stored by value so the popup owns its own copy
    std::unordered_map<std::wstring, std::wstring> m_devices;
    std::wstring                                   m_connectedId;
    std::function<void(const std::wstring&)>       m_fnConnect;
    std::function<void()>                          m_fnDisconnect;

    std::vector<std::wstring> m_otherIds;
    void                      RebuildOtherList();

    // ── Win32 ────────────────────────────────────────────────
    HWND m_hwnd     = nullptr;
    HWND m_hwndHost = nullptr;
    bool m_visible  = false;

    HitResult m_hover = { HitZone::None, -1 };

    float m_sliderY   = 0;
    float m_devicesY0 = 0;
    float m_footerY   = 0;
    float m_winH      = 0;

    // ── D2D resources ────────────────────────────────────────
    ComPtr<ID2D1Factory>          m_d2dFactory;  // matches D2D1CreateFactory() in .cpp
    ComPtr<IDWriteFactory3>       m_dwFactory;
    ComPtr<ID2D1HwndRenderTarget> m_rt;

    ComPtr<ID2D1SolidColorBrush>  m_brushBg;
    ComPtr<ID2D1SolidColorBrush>  m_brushSurface;
    ComPtr<ID2D1SolidColorBrush>  m_brushText;
    ComPtr<ID2D1SolidColorBrush>  m_brushSubtext;
    ComPtr<ID2D1SolidColorBrush>  m_brushAccent;
    ComPtr<ID2D1SolidColorBrush>  m_brushGreen;
    ComPtr<ID2D1SolidColorBrush>  m_brushHover;
    ComPtr<ID2D1SolidColorBrush>  m_brushSliderBg;
    ComPtr<ID2D1SolidColorBrush>  m_brushDivider;

    ComPtr<IDWriteTextFormat>     m_fmtTitle;
    ComPtr<IDWriteTextFormat>     m_fmtDevice;
    ComPtr<IDWriteTextFormat>     m_fmtSub;
    ComPtr<IDWriteTextFormat>     m_fmtLabel;
    ComPtr<IDWriteTextFormat>     m_fmtButton;
};
