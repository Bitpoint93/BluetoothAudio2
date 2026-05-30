# BT Audio Receiver

A fork of [AudioPlaybackConnector](https://github.com/ysc3839/AudioPlaybackConnector) by ysc3839 (MIT).

Turns your Windows 10 2004+ PC into a **Bluetooth A2DP Sink** — your phone connects to it and streams audio, just like a car stereo. All BT logic is identical to the original; only the UI has been replaced.

---

## What changed from the original

| Original | This fork |
|---|---|
| Uses `DevicePicker` WinRT XAML flyout | Custom Win32 tray popup rendered with Direct2D |
| Shows a full XAML overlay | Small ~300px popup that appears above the taskbar on click |
| No volume control | Volume slider per connected device |
| No connection status indicator | Green dot + "Connected" label |
| No "switch device" in UI | List of other paired devices you can click to switch |

---

## Popup UI layout

```
┌─────────────────────────────────┐
│  BT Audio Receiver              │  ← header
├─────────────────────────────────┤
│  ● Galaxy S24 Ultra             │  ← connected device (green dot)
│    Connected                    │
│  🔊 ──────●──────────────────── │  ← volume slider
├─────────────────────────────────┤
│  OTHER DEVICES                  │
│  🔵 MacBook Pro                 │  ← clickable — connects
│  🔵 iPad Air                    │
├─────────────────────────────────┤
│  Settings          Disconnect   │  ← footer buttons
└─────────────────────────────────┘
```

- **Left-click tray icon** → show/hide popup
- **Right-click tray icon** → context menu (Open / Bluetooth Settings / Exit)
- Click a device in "Other Devices" to switch the A2DP connection to it
- Volume slider controls Windows audio session volume for the connected device
- Popup auto-hides on focus loss
- Dark/light mode follows Windows system setting in real-time

---

## Requirements

- Windows 10 version 2004 (build 19041) or later
- Visual Studio 2022 with:
  - Desktop development with C++ workload
  - Windows 10 SDK 10.0.19041.0 or later
  - C++/WinRT tooling (included by default)

---

## Build

1. Clone this repo
2. Open `AudioPlaybackConnector.sln` in Visual Studio 2022
3. Right-click solution → **Restore NuGet Packages** (installs `wil`)
4. Build → **Release | x64**
5. Output: `x64/Release/BTAudioReceiver.exe`

> **Note:** Do not build until explicitly instructed — per project conventions.

---

## First-time setup

1. Go to **Settings → Bluetooth** and pair your phone to the PC
2. Run `BTAudioReceiver.exe` — it appears in the system tray
3. Click the tray icon and select your device
4. Play audio on your phone — it streams through your PC speakers

---

## Files

| File | Purpose |
|---|---|
| `AudioPlaybackConnector.cpp` | Main entry point, BT device watcher, tray icon |
| `PopupWindow.h / .cpp` | Custom Direct2D tray popup (replaces DevicePicker) |
| `pch.h / pch.cpp` | Precompiled header |
| `targetver.h` | Windows 10 2004 minimum |
| `resource.h` | Resource IDs |
| `AudioPlaybackConnector.rc` | Icon + manifest resources |
| `AudioPlaybackConnector.manifest` | DPI awareness, Win10 compat |
| `AudioPlaybackConnector.vcxproj` | VS2022 project |
| `AudioPlaybackConnector.sln` | Solution file |
| `packages.config` | NuGet: Windows Implementation Library (wil) |

---

## License

MIT — original copyright ysc3839. Fork UI additions same license.
