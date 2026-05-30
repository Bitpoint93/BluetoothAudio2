// AudioPlaybackConnector.cpp
// Fork of https://github.com/ysc3839/AudioPlaybackConnector by ysc3839
// UI replaced: DevicePicker flyout → custom Win32 tray popup (Direct2D rendered)
// Original MIT license preserved.

#include "pch.h"
#include "resource.h"
#include "PopupWindow.h"

// ─────────────────────────────────────────────────────────────
//  WinRT namespaces
// ─────────────────────────────────────────────────────────────
using namespace winrt;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Media::Audio;

// ─────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────
static constexpr UINT WM_TRAYICON      = WM_APP + 1;
static constexpr UINT TRAY_UID         = 1;
static constexpr wchar_t kAppName[]    = L"BT Audio Receiver";
static constexpr wchar_t kWndClass[]   = L"BTAudioReceiverHost";

// A2DP sink selector — same as original
static constexpr wchar_t kA2DPSelector[] =
    L"System.Devices.Aep.ProtocolId:=\"{E0CBF06C-CD8B-4647-BB8A-263B43F0F974}\""
    L" AND System.Devices.Aep.IsPaired:=System.StructuredQueryType.Boolean#True";

// ─────────────────────────────────────────────────────────────
//  Globals
// ─────────────────────────────────────────────────────────────
static HWND              g_hwndHost     = nullptr;
static NOTIFYICONDATAW   g_nid          = {};
static PopupWindow*      g_popup        = nullptr;

// BT device watcher
static DeviceWatcher            g_watcher{ nullptr };
static winrt::event_token       g_tokAdded, g_tokRemoved, g_tokUpdated, g_tokEnumDone;

// Current connected device (empty = none)
static std::wstring g_connectedId;

// All known paired A2DP devices  { id → friendly name }
static std::unordered_map<std::wstring, std::wstring> g_devices;

// AudioPlaybackConnection for the active device
static AudioPlaybackConnection g_apc{ nullptr };

// ─────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────
void ConnectDevice(const std::wstring& deviceId);
void DisconnectCurrent();
void UpdateTrayTooltip();

// ─────────────────────────────────────────────────────────────
//  AudioPlaybackConnection helpers (same logic as original)
// ─────────────────────────────────────────────────────────────
static void OpenConnection(const std::wstring& deviceId)
{
    if (g_apc)
    {
        g_apc.Close();
        g_apc = nullptr;
    }

    try
    {
        g_apc = AudioPlaybackConnection::TryCreateFromId(deviceId);
        if (!g_apc)
        {
            MessageBoxW(nullptr, L"Could not create AudioPlaybackConnection.\n"
                        L"Make sure the device is paired and in range.", kAppName, MB_ICONERROR);
            return;
        }
        g_apc.StartAsync().get();
        g_connectedId = deviceId;
        UpdateTrayTooltip();

        if (g_popup)
            g_popup->OnConnectionChanged();
    }
    catch (const winrt::hresult_error& ex)
    {
        MessageBoxW(nullptr, ex.message().c_str(), kAppName, MB_ICONERROR);
    }
}

// ─────────────────────────────────────────────────────────────
//  Public: connect / disconnect
// ─────────────────────────────────────────────────────────────
void ConnectDevice(const std::wstring& deviceId)
{
    OpenConnection(deviceId);
}

void DisconnectCurrent()
{
    if (g_apc)
    {
        g_apc.Close();
        g_apc = nullptr;
    }
    g_connectedId.clear();
    UpdateTrayTooltip();

    if (g_popup)
        g_popup->OnConnectionChanged();
}

// ─────────────────────────────────────────────────────────────
//  Tray icon helpers
// ─────────────────────────────────────────────────────────────
void UpdateTrayTooltip()
{
    std::wstring tip = kAppName;
    if (!g_connectedId.empty())
    {
        auto it = g_devices.find(g_connectedId);
        if (it != g_devices.end())
            tip = it->second + L" — connected";
    }

    wcsncpy_s(g_nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void AddTrayIcon(HWND hwnd)
{
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = TRAY_UID;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIconW(GetModuleHandleW(nullptr),
                                       MAKEINTRESOURCEW(IDI_APPICON));
    wcscpy_s(g_nid.szTip, kAppName);
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Use NOTIFYICON_VERSION_4 for TaskbarCreated re-add support
    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);
}

// ─────────────────────────────────────────────────────────────
//  Device watcher callbacks
// ─────────────────────────────────────────────────────────────
static void OnDeviceAdded(const DeviceWatcher&, const DeviceInformation& di)
{
    auto id   = std::wstring(di.Id());
    auto name = std::wstring(di.Name());
    PostMessageW(g_hwndHost, WM_APP + 10,
                 reinterpret_cast<WPARAM>(new std::wstring(id)),
                 reinterpret_cast<LPARAM>(new std::wstring(name)));
}

static void OnDeviceRemoved(const DeviceWatcher&, const DeviceInformationUpdate& diu)
{
    auto id = std::wstring(diu.Id());
    PostMessageW(g_hwndHost, WM_APP + 11,
                 reinterpret_cast<WPARAM>(new std::wstring(id)), 0);
}

static void OnDeviceUpdated(const DeviceWatcher&, const DeviceInformationUpdate& diu)
{
    // Name changes are rare; just signal a refresh
    PostMessageW(g_hwndHost, WM_APP + 12, 0, 0);
}

static void OnEnumerationCompleted(const DeviceWatcher&, const winrt::Windows::Foundation::IInspectable&)
{
    PostMessageW(g_hwndHost, WM_APP + 13, 0, 0);
}

// ─────────────────────────────────────────────────────────────
//  Host window procedure (message-only, invisible)
// ─────────────────────────────────────────────────────────────
static LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static UINT s_taskbarCreatedMsg = 0;

    switch (msg)
    {
    case WM_CREATE:
        s_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
        return 0;

    // ── Tray icon ──────────────────────────────────────────
    case WM_TRAYICON:
    {
        if (LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == NIN_SELECT)
        {
            if (g_popup)
                g_popup->Toggle();
        }
        else if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
        {
            // Right-click context menu
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1, L"Open");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 2, L"Bluetooth Settings");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 3, L"Exit");

            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                     pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);

            if (cmd == 1 && g_popup) g_popup->Show();
            else if (cmd == 2)
                ShellExecuteW(nullptr, L"open",
                              L"ms-settings:bluetooth", nullptr, nullptr, SW_SHOWNORMAL);
            else if (cmd == 3)
                PostQuitMessage(0);
        }
        return 0;
    }

    // ── Device watcher messages ────────────────────────────
    case WM_APP + 10: // device added
    {
        auto* pId   = reinterpret_cast<std::wstring*>(wp);
        auto* pName = reinterpret_cast<std::wstring*>(lp);
        g_devices[*pId] = *pName;
        delete pId; delete pName;
        if (g_popup) g_popup->OnDeviceListChanged();
        return 0;
    }
    case WM_APP + 11: // device removed
    {
        auto* pId = reinterpret_cast<std::wstring*>(wp);
        if (*pId == g_connectedId)
            DisconnectCurrent();
        g_devices.erase(*pId);
        delete pId;
        if (g_popup) g_popup->OnDeviceListChanged();
        return 0;
    }
    case WM_APP + 12: // device updated
    case WM_APP + 13: // enumeration done
        if (g_popup) g_popup->OnDeviceListChanged();
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;

    default:
        if (msg == s_taskbarCreatedMsg)
            AddTrayIcon(hwnd);           // re-add icon if Explorer restarted
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ─────────────────────────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────────────────────────
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    // Single instance guard
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"BTAudioReceiverMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr, L"BT Audio Receiver is already running.",
                    kAppName, MB_ICONINFORMATION);
        return 0;
    }

    // Init COM / WinRT
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Register host window class (invisible message pump window)
    WNDCLASSEXW wcx   = {};
    wcx.cbSize        = sizeof(wcx);
    wcx.lpfnWndProc   = HostWndProc;
    wcx.hInstance     = hInst;
    wcx.lpszClassName = kWndClass;
    RegisterClassExW(&wcx);

    g_hwndHost = CreateWindowExW(0, kWndClass, kAppName, 0,
                                 0, 0, 0, 0,
                                 HWND_MESSAGE, nullptr, hInst, nullptr);
    if (!g_hwndHost)
        return 1;

    // Tray icon
    AddTrayIcon(g_hwndHost);

    // Create popup window
    g_popup = new PopupWindow(hInst, g_hwndHost,
                              g_devices, g_connectedId,
                              ConnectDevice, DisconnectCurrent);

    // Start BT device watcher
    {
        auto props = winrt::single_threaded_vector<winrt::hstring>(
            { L"System.Devices.Aep.DeviceAddress",
              L"System.Devices.Aep.IsConnected" });

        g_watcher = DeviceInformation::CreateWatcher(
            kA2DPSelector, props,
            DeviceInformationKind::AssociationEndpoint);

        g_tokAdded    = g_watcher.Added(OnDeviceAdded);
        g_tokRemoved  = g_watcher.Removed(OnDeviceRemoved);
        g_tokUpdated  = g_watcher.Updated(OnDeviceUpdated);
        g_tokEnumDone = g_watcher.EnumerationCompleted(OnEnumerationCompleted);

        g_watcher.Start();
    }

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    g_watcher.Stop();
    g_watcher.Added(g_tokAdded);
    g_watcher.Removed(g_tokRemoved);
    g_watcher.Updated(g_tokUpdated);
    g_watcher.EnumerationCompleted(g_tokEnumDone);

    if (g_apc)
        g_apc.Close();

    delete g_popup;
    CloseHandle(hMutex);
    return static_cast<int>(msg.wParam);
}
