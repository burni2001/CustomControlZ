# Code Conventions

**Analysis Date:** 2026-03-11

## Language & Style

- **C++20** throughout (`LanguageStandard=stdcpp20` in `.vcxproj`)
- **Wide strings** (`wchar_t*`, `std::wstring`-free — uses raw `wchar_t` buffers with `StringCchCopy`/`StringCchPrintf`)
- **Win32 API style** — HWND, HBRUSH, COLORREF, WM_* message handling
- No STL containers; only `std::thread`, `std::atomic`, `std::mutex`, `std::function`
- `constexpr` for all numeric constants (never magic numbers in logic)
- `inline` on small utility functions (`PressKey`, `ReleaseKey`, `IsKeyDown`, `IsValidKey`)

## Naming

| Scope | Convention | Example |
|---|---|---|
| Globals | `g_` prefix + camelCase | `g_activeProfile`, `g_logicRunning` |
| Global handles | `g_h` prefix | `g_hMainWindow`, `g_hFontTitle` |
| Constants/macros | ALL_CAPS with underscores | `BTN_BIND_BASE`, `MAX_BINDINGS` |
| Layout constants | `LAYOUT_` or `SELECT_` prefix | `LAYOUT_LEFT_MARGIN`, `SELECT_WIN_WIDTH` |
| Timing constants | `_MS` or `_SEC` suffix | `ER_LOGIC_TICK_MS`, `SHELL_TIMEOUT_SEC` |
| Buffer constants | `_BUFFER` suffix | `KEY_NAME_BUFFER`, `FONT_NAME_BUFFER` |
| Static profile vars | `g_` prefix + PascalCase | `g_EldenRingProfile` |
| Binding enums | `XX_KEY_NAME` pattern | `ER_KEY_DODGE`, `TC_KEY_SCROLL_UP` |
| Functions | PascalCase | `EnableDarkTitleBar`, `SaveConfig` |
| Local variables | camelCase | `localIngame`, `sprintActive` |
| Struct fields | camelCase | `bindingCount`, `currentVk`, `logicFn` |

## Patterns

### WM_DRAWITEM for custom-styled buttons
All buttons use `BS_OWNERDRAW` and handle `WM_DRAWITEM` for consistent dark-themed rendering. Pattern in `SettingsProc`:
```cpp
case WM_DRAWITEM: {
    LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
    ButtonStyle style = GetButtonStyle(pDIS->CtlID);
    FillRect(hdc, &rc, style.brush);
    // draw border, text...
}
```

### Mutex-protected config snapshot in logic thread
Logic threads snapshot bindings under lock, then use snapshot values for polling:
```cpp
WORD localIngame, localDodge;
{
    std::lock_guard<std::mutex> lock(g_configMutex);
    localIngame = profile->bindings[ER_KEY_INGAME].currentVk;
    localDodge  = profile->bindings[ER_KEY_DODGE].currentVk;
}
// use localIngame, localDodge without holding lock
```

### Edge-triggered key detection
Key actions fire once per press using a boolean state tracker:
```cpp
if (IsKeyDown(key)) {
    if (!keyWasDown) {
        keyWasDown = true;
        // fire action once
    }
} else {
    keyWasDown = false;
}
```

### Game profile as data-only plugin
Each game in `games/` provides a single `static GameProfile` struct with all metadata and a `static void LogicThread(...)` function assigned as `logicFn`. No virtual dispatch.

### Resource cleanup pattern
GDI objects cleaned up with null-guard pattern:
```cpp
if (g_hFontTitle) { DeleteObject(g_hFontTitle); g_hFontTitle = nullptr; }
```

### Safe string operations
All string copies use `StringCchCopy`/`StringCchPrintf` (never `wcscpy`/`swprintf`).

## Error Handling

- **No exceptions** — Win32 returns BOOL/HANDLE, checked inline
- **Silent fallbacks** — if custom icons fail to load, falls back to system icons
- **No assert** — invalid states handled by early return or no-op
- **Modal MessageBox** for user-facing errors (invalid key binding)
- Logic thread and UI thread failures are not logged; they silently stop

## Threading Model

- **Main thread:** Windows message pump only
- **Logic thread:** one at a time; stopped/joined before starting new one
- **Shared state protection:** `g_configMutex` for font name and all `currentVk` values; `g_waitingForBindID` atomic for pause signaling

---

*Conventions analysis: 2026-03-11*
