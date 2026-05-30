// pch.h — precompiled header for BTAudioReceiver fork
// Keeps all deps from the original AudioPlaybackConnector,
// adds dwrite_3.h for the custom popup UI.

#ifndef PCH_H
#define PCH_H

#include "targetver.h"

// Windows
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>       // GET_X_LPARAM / GET_Y_LPARAM
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl_core.h>
#include <d2d1_3.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <winreg.h>

// C++ stdlib
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

// wil
#ifndef _DEBUG
#define RESULT_DIAGNOSTICS_LEVEL 1
#endif
#include <wil/common.h>
#include <wil/result.h>
#include <wil/cppwinrt.h>

// C++/WinRT
#undef GetCurrentTime
#include <winrt/base.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Media.Audio.h>
#include <winrt/Windows.System.h>

// Note: We do NOT include XAML headers — the custom popup replaces all XAML UI.
// The original pch.h included:
//   winrt/Windows.UI.Xaml.Controls.h  ← removed
//   winrt/Windows.UI.Xaml.Hosting.h   ← removed
//   windows.ui.xaml.hosting.desktopwindowxamlsource.h ← removed

#endif // PCH_H
