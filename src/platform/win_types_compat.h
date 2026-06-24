#pragma once
// macOS compatibility shim: provides Windows types and stubs used by BehaviorEngine.h
// and GameProfiles.h. Included only when NOT building for Windows.
#ifndef _WIN32

#include <stdint.h>
#include <stddef.h>
#include <string.h>   // memset
#include <unistd.h>   // usleep
#include <chrono>
#include <cwchar>     // wchar_t, wcschr

// ---------------------------------------------------------------------------
// Core Windows integer types
// ---------------------------------------------------------------------------
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned long       DWORD;
typedef unsigned long long  ULONGLONG;
typedef long long           LONGLONG;
typedef unsigned int        UINT;
typedef void*               HWND;
typedef void*               HANDLE;
typedef void*               HMODULE;
typedef int                 INT;
typedef long                LONG;
// BOOL is defined by Objective-C headers as 'bool'; don't redefine it here.
// Pure C++ callers that need it can use 'bool' directly — our game profile
// headers only use 'bool', never BOOL.

// ---------------------------------------------------------------------------
// Color
// ---------------------------------------------------------------------------
typedef DWORD COLORREF;
#define RGB(r,g,b) ((COLORREF)( \
    (unsigned char)(r)          | \
    ((unsigned short)((unsigned char)(g)) << 8) | \
    (((DWORD)(unsigned char)(b)) << 16) ))

// ---------------------------------------------------------------------------
// Boolean
// ---------------------------------------------------------------------------
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

// ---------------------------------------------------------------------------
// Virtual key codes  (subset used across BehaviorEngine + game profiles)
// ---------------------------------------------------------------------------
#define VK_LBUTTON   0x01
#define VK_RBUTTON   0x02
#define VK_MBUTTON   0x04
#define VK_XBUTTON1  0x05
#define VK_XBUTTON2  0x06
#define VK_BACK      0x08
#define VK_TAB       0x09
#define VK_RETURN    0x0D
#define VK_SHIFT     0x10
#define VK_CONTROL   0x11
#define VK_MENU      0x12
#define VK_CAPITAL   0x14
#define VK_ESCAPE    0x1B
#define VK_SPACE     0x20
#define VK_LEFT      0x25
#define VK_UP        0x26
#define VK_RIGHT     0x27
#define VK_DOWN      0x28
#define VK_DELETE    0x2E
#define VK_F1        0x70
#define VK_F2        0x71
#define VK_F3        0x72
#define VK_F4        0x73
#define VK_F5        0x74
#define VK_F6        0x75
#define VK_F7        0x76
#define VK_F8        0x77
#define VK_F9        0x78
#define VK_F10       0x79
#define VK_F11       0x7A
#define VK_F12       0x7B
#define VK_LSHIFT    0xA0
#define VK_RSHIFT    0xA1
#define VK_LCONTROL  0xA2
#define VK_RCONTROL  0xA3
#define VK_LMENU     0xA4
#define VK_RMENU     0xA5
#define VK_OEM_1     0xBA  // ;:
#define VK_OEM_PLUS  0xBB  // =+
#define VK_OEM_COMMA 0xBC  // ,<
#define VK_OEM_MINUS 0xBD  // -_
#define VK_OEM_PERIOD 0xBE // .>
#define VK_OEM_2     0xBF  // /?
#define VK_OEM_3     0xC0  // `~
#define VK_OEM_4     0xDB  // [{
#define VK_OEM_5     0xDC  // \|
#define VK_OEM_6     0xDD  // ]}
#define VK_OEM_7     0xDE  // '"

// ---------------------------------------------------------------------------
// Sleep / timing
// ---------------------------------------------------------------------------
inline void Sleep(unsigned int ms) { usleep(static_cast<useconds_t>(ms) * 1000u); }

inline ULONGLONG GetTickCount64() {
    using namespace std::chrono;
    return static_cast<ULONGLONG>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// ---------------------------------------------------------------------------
// MessageBox stub (logs to stderr; avoids blocking the logic thread on macOS)
// ---------------------------------------------------------------------------
#define MB_OK           0
#define MB_ICONWARNING  0
#include <cstdio>
inline int MessageBox(HWND, const wchar_t* text, const wchar_t* title, UINT) {
    fwprintf(stderr, L"[Warning] %ls: %ls\n", title ? title : L"", text ? text : L"");
    return 0;
}

// ---------------------------------------------------------------------------
// INPUT struct stubs  (used by WheelToKey / WheelToggle in GenericLogicThreadFn)
// Instead of Win32 SendInput, route wheel events through a global callback.
// ---------------------------------------------------------------------------
#define INPUT_KEYBOARD      1
#define INPUT_MOUSE         0
#define MOUSEEVENTF_WHEEL   0x0800
#define MOUSEEVENTF_XDOWN   0x0080
#define MOUSEEVENTF_XUP     0x0100
#define KEYEVENTF_KEYUP     0x0002
#define KEYEVENTF_SCANCODE  0x0008

struct MOUSEINPUT {
    LONG      dx;
    LONG      dy;
    DWORD     mouseData;
    DWORD     dwFlags;
    DWORD     time;
    ULONGLONG dwExtraInfo;
};

struct KEYBDINPUT {
    WORD      wVk;
    WORD      wScan;
    DWORD     dwFlags;
    DWORD     time;
    ULONGLONG dwExtraInfo;
};

struct INPUT {
    DWORD type;
    union {
        MOUSEINPUT mi;
        KEYBDINPUT ki;
    };
};

// Global hook: platform_macos.mm sets this so SendInput can reach the platform
typedef UINT (*SendInputFnPtr)(UINT, INPUT*, int);
extern SendInputFnPtr g_sendInputImpl;
inline UINT SendInput(UINT n, INPUT* inputs, int cbSize) {
    if (g_sendInputImpl) return g_sendInputImpl(n, inputs, cbSize);
    return 0;
}

// MapVirtualKey stub (unused on macOS path but referenced as a type)
#define MAPVK_VK_TO_CHAR 2
inline UINT MapVirtualKey(UINT, UINT) { return 0; }

// CharUpperW stub
inline wchar_t* CharUpperW(wchar_t* s) { return s; }

// ARRAYSIZE macro
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

#endif // !_WIN32
