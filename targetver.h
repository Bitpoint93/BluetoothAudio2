// targetver.h
// Minimum Windows version: Windows 10 version 2004 (20H1), build 19041
// Required for AudioPlaybackConnection WinRT API.

#pragma once
#include <sdkddkver.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00   // Windows 10
#endif

// AudioPlaybackConnection was added in Windows 10 2004 (build 19041)
// NTDDI_WIN10_VB = 0x0A000008
#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10_VB
#endif
