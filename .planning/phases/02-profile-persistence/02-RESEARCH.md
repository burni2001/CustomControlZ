# Phase 2: Profile Persistence - Research

**Researched:** 2026-03-12
**Domain:** JSON file I/O in C++20/Win32, filesystem discovery, GameProfile hydration, settings.ini migration
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PROF-01 | Game profiles stored as JSON files in a `profiles\` folder next to the exe | Win32 `GetModuleFileNameW` gives exe path; append `\profiles\` to resolve folder at runtime with no hardcoded absolute path |
| PROF-02 | App auto-discovers and loads all `.json` profile files in `profiles\` at startup — no hardcoded list | `FindFirstFileW` / `FindNextFileW` with pattern `profiles\*.json` scans the folder; any file found becomes a profile. `g_gameProfiles` / `g_gameProfileCount` are populated at startup, not compile time |
| PROF-03 | Elden Ring and Toxic Commando rebuilt as JSON profiles; hardcoded C++ implementations removed | `games/EldenRing.h` and `games/ToxicCommando.h` replaced by `profiles/EldenRing.json` and `profiles/ToxicCommando.json`; `#include` lines removed from CustomControlZ.cpp; `GameProfile` objects allocated on heap from parsed JSON |
| PROF-04 | User's saved key bindings migrated from `settings.ini` into the corresponding JSON profile files | Each JSON profile's `bindings[i].currentVk` is written to the JSON file on `SaveConfig`; `settings.ini` is read once on first load to seed defaults if JSON has no saved value, then ignored |
</phase_requirements>

---

## Summary

Phase 2 replaces the compile-time `#include "games/EldenRing.h"` and `#include "games/ToxicCommando.h"` pattern with runtime JSON file loading. On startup the app scans a `profiles\` folder next to the exe, parses every `.json` file it finds, and builds a heap-allocated `GameProfile` array that the rest of the app uses identically to before. The `GameProfile` struct itself does not change; the JSON parser is responsible for populating it.

The constraint of "C++20, Win32 API only — no new runtime dependencies" eliminates every popular JSON library (nlohmann/json, RapidJSON, etc.) because they all require adding a header-only or source file dependency to the project. The only viable zero-dependency approach on Windows is a hand-written or project-local JSON parser. Given the schema is intentionally small and well-defined, a bespoke recursive-descent parser for the exact schema is the right choice — not because hand-rolling is generally good, but because the alternative (a 20,000-line header library) violates the explicit project constraint. The schema is simple enough (JSON objects, arrays of objects, string/int/hex fields) that a targeted parser is approximately 150 lines and carries no edge-case risk for inputs that never come from an adversary.

The settings.ini migration (PROF-04) is a one-time "read if exists, write to JSON on save" pattern. The `LoadConfig` function already reads from `settings.ini` keyed by `iniSection` + `iniKey`; in Phase 2, the JSON file becomes the primary store, and `settings.ini` serves only as a fallback seed for previously-saved VK values if a fresh JSON file has no saved binding values.

**Primary recommendation:** Write a `ProfileLoader.h` header with a single `LoadProfilesFromFolder` function that calls `FindFirstFileW`/`FindNextFileW`, parses each `.json` file using a purpose-built minimal parser, heap-allocates `GameProfile` objects, and populates `g_gameProfiles` / `g_gameProfileCount`. Delete `games/EldenRing.h` and `games/ToxicCommando.h` from the project, ship `profiles/EldenRing.json` and `profiles/ToxicCommando.json` next to the exe.

---

## Standard Stack

### Core (all existing or Win32 built-in — no new dependencies)

| API / Component | Version | Purpose | Why Standard |
|----------------|---------|---------|--------------|
| `GetModuleFileNameW` | Win32 (all versions) | Get absolute path of the running exe | Only reliable way to find exe-relative paths on Windows; works regardless of working directory |
| `FindFirstFileW` / `FindNextFileW` | Win32 (all versions) | Enumerate `profiles\*.json` files | The standard Win32 filesystem enumeration API; no CRT or COM init required |
| `CreateFileW` + `ReadFile` | Win32 | Read the full content of each JSON file into a buffer | Simple, reliable, no buffering surprise; `GetFileSizeEx` gives exact size for single read |
| `WriteFile` | Win32 | Write updated JSON back when bindings change | Consistent with `ReadFile`; atomic via create-temp + `MoveFileExW` is optional for this phase |
| Hand-written JSON parser (bespoke) | C++20 | Parse the profile schema | Project constraint forbids new dependencies; schema is fixed and small |
| `new` / `delete[]` (C++20) | C++20 stdlib | Heap-allocate `GameProfile` objects | Static `GameProfile` arrays in headers will be deleted; dynamic allocation required |
| `WritePrivateProfileString` (legacy) | Win32 | Continue writing font preference to settings.ini | Font is a UI preference, not a profile field; keep in settings.ini unchanged |

### No External Libraries

The constraint from PROJECT.md is absolute: "C++20, Win32 API only — no new runtime dependencies." This rules out:
- nlohmann/json (header-only, but large and adds to source tree)
- RapidJSON (similar)
- Windows JSON (WinRT `JsonObject`) — requires COM + WinRT, adds a significant dependency surface
- `std::filesystem` — available in C++20 stdlib, but `FindFirstFileW` is already used in the project and works fine; `std::filesystem` is the acceptable alternative if the team wants it, but it adds no capability here

**Decision point:** `std::filesystem::directory_iterator` is a clean C++20 alternative to `FindFirstFileW`. It requires no new runtime dependency (it is part of the C++ standard library already linked). Either is acceptable. Given the project already uses Win32 patterns throughout, `FindFirstFileW` is the consistent choice; however, `std::filesystem` is not wrong.

### Supporting

| Component | Purpose | When to Use |
|-----------|---------|-------------|
| `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` | Atomic JSON save (write temp, rename) | If user reports corruption on crash-during-save; optional for Phase 2 |
| `GetPrivateProfileInt` (legacy) | Seed currentVk from settings.ini on first load | Only needed during PROF-04 migration; remove from LoadConfig once migration window is over |

---

## JSON Schema

The profile JSON schema must be fully defined before writing the parser. Based on the existing `GameProfile` struct fields:

```json
{
  "id":            "EldenRing",
  "displayName":   "Elden Ring",
  "processName1":  "eldenring.exe",
  "processName2":  "ersc_launcher.exe",
  "tipActive":     "CustomControlZ - Elden Ring: ACTIVE",
  "tipIdle":       "CustomControlZ: Waiting...",
  "settingsTitle": "Elden Ring - Key Bindings",
  "theme": {
    "bg":             "0x19140F",
    "text":           "0xDCC391",
    "accent":         "0xFFD778",
    "button":         "0x282319",
    "border":         "0x78643C",
    "rowBg":          "0x2D261C",
    "separator":      "0x8C7346",
    "exitFill":       "0xB43232",
    "exitBorder":     "0xC85050",
    "exitText":       "0xFFDCDC",
    "minimizeBorder": "0x504637"
  },
  "bindings": [
    {
      "iniKey":       "IngameKey",
      "label":        "Backstep, Dodge Roll, Dash",
      "defaultVk":    70,
      "currentVk":    70,
      "behaviorType": "HoldToToggle",
      "outputVk":     0
    },
    {
      "iniKey":       "DodgeKey",
      "label":        "Dodge Roll (Custom Key)",
      "defaultVk":    67,
      "currentVk":    67,
      "behaviorType": "EdgeTrigger",
      "outputVk":     70,
      "durationMs":   50
    },
    {
      "iniKey":       "SprintKey",
      "label":        "Dash, Sprint (Custom Key)",
      "defaultVk":    20,
      "currentVk":    20,
      "behaviorType": "HoldToToggle",
      "outputVk":     70
    },
    {
      "iniKey":       "TriggerKey",
      "label":        "Close, Back (Long Press Trigger)",
      "defaultVk":    27,
      "currentVk":    27,
      "behaviorType": "LongPress",
      "outputVk":     27,
      "longOutputVk": 81,
      "thresholdMs":  400
    }
  ]
}
```

**Design decisions for the schema:**

1. **Color values as hex strings (`"0x19140F"`)** — Matches COLORREF byte layout (BGR). Human-readable in the file. Parser uses `wcstoul(val, nullptr, 16)`.

2. **VK values as decimal integers** — Simpler than hex for keys; `_wtoi` reads them. The `currentVk` field is the user-saved binding value; `defaultVk` is the factory default.

3. **`behaviorType` as string enum** — Maps to `BehaviorType` enum. Set `{"HoldToToggle", "EdgeTrigger", "LongPress", "WheelToKey"}`.

4. **Optional fields with defaults** — `durationMs` defaults to 50 if absent; `thresholdMs` defaults to 400; `longOutputVk` defaults to 0; `wheelDelta` defaults to 120. Parser applies defaults, so existing JSON files without optional fields remain valid.

5. **`processName2` is nullable** — JSON: omit the key or set `"processName2": null`. Parser treats missing or null as `nullptr`.

6. **`iniKey` and `iniSection`** — `iniSection` is NOT a JSON field (it equals `id`); the parser derives `iniSection` from `id`. `iniKey` remains a per-binding field for settings.ini migration lookup.

7. **`currentVk` in JSON** — This is the user-saved value. On save, the app rewrites the JSON file with updated `currentVk` values. No settings.ini needed once the JSON file exists.

---

## Architecture Patterns

### Recommended File Structure (after Phase 2)

```
CustomControlZ/
├── BehaviorEngine.h       UNCHANGED from Phase 1
├── GameProfiles.h         UNCHANGED from Phase 1
├── ProfileLoader.h        NEW — LoadProfilesFromFolder, SaveProfileBindings
├── CustomControlZ.cpp     MODIFY — remove #include games/*.h, call LoadProfilesFromFolder
│                                   on startup; update SaveConfig/LoadConfig to use JSON
├── games/
│   ├── EldenRing.h        DELETE — replaced by profiles/EldenRing.json
│   └── ToxicCommando.h    DELETE — replaced by profiles/ToxicCommando.json
└── (exe output folder)/
    └── profiles/
        ├── EldenRing.json
        └── ToxicCommando.json
```

**Note on project file:** `games/EldenRing.h` and `games/ToxicCommando.h` must be removed from `CustomControlZ.vcxproj` (the `<ClInclude>` entries) in addition to being deleted from disk.

### Pattern 1: Exe-Relative Profiles Folder

```cpp
// ProfileLoader.h — resolves "profiles\" next to the exe at runtime
// Source: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew

std::wstring GetProfilesFolder() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    // Strip filename: find last backslash
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';  // keep trailing backslash
    return std::wstring(exePath) + L"profiles\\";
}
```

### Pattern 2: Folder Enumeration

```cpp
// List all *.json files in the profiles folder
// Source: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew

std::vector<std::wstring> EnumerateProfileFiles(const std::wstring& folder) {
    std::vector<std::wstring> result;
    WIN32_FIND_DATAW fd;
    std::wstring pattern = folder + L"*.json";
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return result;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            result.push_back(folder + fd.cFileName);
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return result;
}
```

### Pattern 3: File Read into Buffer

```cpp
// Read entire file content into a wstring (assuming UTF-8, convert to wide)
// Source: Win32 ReadFile docs — https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile

std::string ReadFileToString(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    std::string buf(static_cast<size_t>(size.QuadPart), '\0');
    DWORD bytesRead = 0;
    ReadFile(hFile, buf.data(), static_cast<DWORD>(size.QuadPart), &bytesRead, nullptr);
    CloseHandle(hFile);
    return buf;
}
```

### Pattern 4: Minimal JSON Parser for the Profile Schema

The JSON schema is a single flat object with one nested object (`theme`) and one array (`bindings`). A purpose-built parser needs to handle:
- String values (narrow UTF-8 → wide char via `MultiByteToWideChar`)
- Integer values (decimal VK codes)
- Hex string values (COLORREF as `"0xRRGGBB"`)
- Null values (`processName2: null`)
- One-level nested object (`theme`)
- Array of objects (`bindings`)

A hand-written parser of ~150 lines (a simple `ParseJsonObject` that returns a `std::map<std::string, std::string>` for top-level fields, plus a `ParseTheme` and `ParseBindings` helper) covers the full schema. No recursive descent needed — the schema depth is exactly 2.

```cpp
// Approach: tokenise by scanning for '"key"' patterns, extract values
// For objects: scan between '{' and matching '}'; for arrays: scan between '[' and ']'
// No general-purpose parser needed — schema is known and fixed

struct ParsedBinding {
    std::wstring iniKey;
    std::wstring label;
    WORD         defaultVk   = 0;
    WORD         currentVk   = 0;
    std::string  behaviorType;
    WORD         outputVk    = 0;
    WORD         longOutputVk = 0;
    int          thresholdMs  = 400;
    int          durationMs   = 50;
    DWORD        wheelDelta   = 120;
};

struct ParsedProfile {
    std::wstring id;
    std::wstring displayName;
    std::wstring processName1;
    std::wstring processName2;  // empty = nullptr
    std::wstring tipActive;
    std::wstring tipIdle;
    std::wstring settingsTitle;
    Theme        theme;
    std::vector<ParsedBinding> bindings;
};
```

**Parser implementation strategy:**
1. `TrimWhitespace(str)` — remove leading/trailing whitespace
2. `FindStringValue(json, key)` → returns string value for `"key": "value"` or `"key": null`
3. `FindIntValue(json, key, default)` → returns int for `"key": 123`
4. `FindHexColorValue(json, key, default)` → parses `"0xRRGGBB"` hex string to COLORREF
5. `ExtractObject(json, key)` → returns the `{...}` content of `"key": {...}`
6. `ExtractArray(json, key)` → returns vector of `{...}` strings from `"key": [...]`
7. `ParseBinding(objectJson)` → returns `ParsedBinding`
8. `ParseProfile(fileContent)` → returns `ParsedProfile`

**Wide string handling:** JSON file is written as UTF-8. String values are narrow `char`. Use `MultiByteToWideChar(CP_UTF8, ...)` to convert string values to `wchar_t` for storage in `GameProfile` fields. Heap-allocate with `new wchar_t[len+1]` — these strings live for the app lifetime.

**Persistence of parsed strings:** `GameProfile` fields (`id`, `displayName`, etc.) are `const wchar_t*`. After parsing, these must be heap-allocated and kept alive for the app lifetime. A `ProfileLoader` that owns a `std::vector<std::unique_ptr<GameProfile>>` plus associated string buffers is the safest ownership model.

### Pattern 5: SaveConfig / LoadConfig Replacement

Current `SaveConfig` writes to `settings.ini` via `WritePrivateProfileString`. In Phase 2:

- **LoadConfig (JSON path):** Read the JSON file, extract `currentVk` for each binding. If `currentVk` is zero or missing, fall back to reading the `settings.ini` value (one-time migration). Write the seeded value back to JSON immediately to complete migration.
- **SaveConfig (JSON path):** Read the existing JSON file, replace all `currentVk` values for the current profile, write back to disk. The simplest approach: re-serialize the entire profile to JSON and overwrite the file.

```cpp
// SaveProfileBindings — rewrites the JSON file with updated currentVk values
// Strategy: build the JSON string from the heap-allocated ParsedProfile + current VK values
void SaveProfileBindings(const std::wstring& jsonPath, GameProfile* profile);
```

**Alternative to full re-serialization:** Replace only the `currentVk` values in the raw JSON string (string search + replace for `"currentVk": NNN`). This avoids a full serializer and preserves user formatting/comments. Simpler for this phase.

### Anti-Patterns to Avoid

- **Stack-allocating `wchar_t*` strings returned as `const wchar_t*` fields of `GameProfile`:** The profile struct stores raw `const wchar_t*` pointers; these must point to heap memory that outlives the loader call. `std::wstring` local variables going out of scope cause dangling pointers.
- **Forgetting `FindClose`:** Every `FindFirstFileW` call that succeeds must be paired with `FindClose`. Resource leak on early return.
- **Using `atoi`/`_wtoi` on hex strings without checking prefix:** `"0xFF"` fails `_wtoi` — use `wcstoul(val, nullptr, 16)` for COLORREF values.
- **Loading profiles before `g_gameProfiles` is used:** Load must complete before `CreateGameSelectionWindow` because that window iterates `g_gameProfiles`.
- **Writing `currentVk` back to `settings.ini`:** Once Phase 2 is complete, do not write to settings.ini for binding values. Write only to JSON. Leave font preference in settings.ini.
- **Accessing null processName2:** After loading, if `processName2` field was absent or null in JSON, the `GameProfile.processName2` pointer must be `nullptr` (not an empty string). `IsGameRunning` and `IsProcessRunningElevated` both check `profile->processName2` for null before using it.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Find exe directory | Custom registry/environment walk | `GetModuleFileNameW(nullptr, ...)` | Only reliable exe-path API; environment vars and working directory are wrong |
| File enumeration | `FindFirstFileW` reinvented | `FindFirstFileW` / `FindNextFileW` directly | Already the Win32 standard; no wrapper needed |
| Full JSON parser | Custom general-purpose recursive JSON parser | Purpose-built 150-line schema-specific parser | General parser is overkill for a fixed 2-level schema; schema-specific parser has zero untargeted attack surface |
| Dynamic string pooling | Custom string allocator | `new wchar_t[]` + lifetime tied to profile vector | Sufficient for a small fixed number of profiles; no allocator complexity needed |
| Atomic file write | fsync + rename sequence | `WriteFile` to temp path + `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` | Standard Windows atomic-rename idiom; prevents corruption on crash-during-write |

---

## Common Pitfalls

### Pitfall 1: Wide String Ownership / Dangling Pointers
**What goes wrong:** `GameProfile` fields are `const wchar_t*`. If the wstring used to parse them goes out of scope, the pointer dangles silently.
**Why it happens:** C++ automatic storage duration; the pointer is still non-null but the memory is freed.
**How to avoid:** Heap-allocate all parsed strings with `new wchar_t[len+1]` and store the raw pointers in a companion vector owned by the loader. Keep the loader alive (or store in a static/global `ProfileStorage` object) for the full app lifetime.
**Warning signs:** Garbled display names in the game selection window, or crashes in `_wcsicmp` inside `IsGameRunning`.

### Pitfall 2: `g_gameProfiles` Array is Compile-Time Static
**What goes wrong:** Current code has `GameProfile* g_gameProfiles[] = { &g_EldenRingProfile, &g_ToxicCommandoProfile };` as a static compile-time array. A dynamically-loaded profile list requires a `std::vector<GameProfile*>` or a separately heap-allocated `GameProfile*[]`.
**Why it happens:** The hardcoded array has a compile-time size; discovered profiles have a runtime size.
**How to avoid:** Replace the static array with a `std::vector<GameProfile*> g_gameProfiles` (or a heap-allocated pointer array) populated at startup. Update all uses of `g_gameProfileCount` to use `g_gameProfiles.size()` or a separate `int`.
**Warning signs:** Compiler errors on the array declaration, or the game selection window only showing a fixed 2 profiles regardless of how many JSON files are present.

### Pitfall 3: JSON File Not Found at Startup
**What goes wrong:** App launches, `profiles\` folder is missing or empty, `g_gameProfiles` is empty, game selection window shows no buttons.
**Why it happens:** User forgot to deploy the `profiles\` folder alongside the exe, or the exe-relative path resolution is wrong.
**How to avoid:** If `FindFirstFileW` returns `INVALID_HANDLE_VALUE` (folder missing) or finds no `.json` files, show a `MessageBox` explaining that no profile files were found and where to put them. Do not silently continue with an empty profile list.
**Warning signs:** Game selection window shows only an Exit button.

### Pitfall 4: COLORREF Byte Order
**What goes wrong:** COLORREF is `0x00BBGGRR` (Blue in high byte, Red in low byte). If colors are stored as `0xRRGGBB` HTML-style strings in JSON, the parser must reverse the bytes.
**Why it happens:** `RGB(r, g, b)` macro expands to `r | (g << 8) | (b << 16)` = `0x00BBGGRR`. The existing C++ code uses `RGB(25, 20, 15)` notation. When converting to a JSON hex string for human readability, the current code should write `0x0F1419` (the COLORREF value directly) not `0x19140F`.
**How to avoid:** Store COLORREF values as plain decimal integers in JSON, or as the raw COLORREF hex without byte-swapping. Decide at schema definition time and be consistent. Recommended: store as decimal integer (e.g., `"bg": 987151`) — unambiguous, no byte-order concern, `wcstoul` parses it with base 10.
**Warning signs:** Wrong colors in settings window after loading from JSON (green themes appear red, etc.).

### Pitfall 5: processName2 Null vs Empty String
**What goes wrong:** `IsGameRunning` and `IsProcessRunningElevated` check `if (profile->processName2)` before using it. If the JSON parser sets an empty `wchar_t[1]` buffer (with `L'\0'`) instead of `nullptr`, both functions will call `_wcsicmp` on an empty string — no crash, but the second process check always fails.
**Why it happens:** JSON `null` is typically parsed as an absent value; a naive parser that allocates `L""` for any missing field doesn't distinguish "absent/null" from "empty string".
**How to avoid:** In the parser, if `processName2` is absent or has a JSON `null` value, set `profile->processName2 = nullptr` explicitly.
**Warning signs:** Elden Ring profile works but `ersc_launcher.exe` is never detected as the game (secondary process name broken).

### Pitfall 6: `iniSection` Derivation
**What goes wrong:** `GameProfile.iniSection` is used in `LoadConfig` and `SaveConfig` to read/write `settings.ini`. If Phase 2 derives `iniSection` from `id` (which it should, to avoid a redundant JSON field), the derived value must be a stable heap-allocated `wchar_t*`, not the `id` pointer itself (which is fine if `id` is already heap-allocated and identical in content).
**Why it happens:** `iniSection` was a separate field in the C++ headers, set identically to `id`. In JSON it is redundant. If the parser simply sets `profile->iniSection = profile->id`, they point to the same buffer — this is safe, but the intent must be explicit.
**How to avoid:** Set `profile->iniSection = profile->id` in the parser. This is safe because the settings.ini key is `[EldenRing]` which equals the profile `id`. Document this decision.

### Pitfall 7: SaveConfig Must Not Corrupt the JSON File
**What goes wrong:** `SaveConfig` is called on every key-bind change. If it writes a partial JSON file (crash during write, disk full), the profile is corrupted and unreadable on next launch.
**Why it happens:** Simple `WriteFile` overwrites the file in place; if the process dies mid-write, the file is truncated.
**How to avoid:** Write to a temp file (`profiles\EldenRing.json.tmp`), then `MoveFileExW(tmp, target, MOVEFILE_REPLACE_EXISTING)`. This is atomic on NTFS for files on the same volume.

---

## Code Examples

### Exe-Relative Path Resolution
```cpp
// Source: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew
std::wstring GetExeFolder() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* last = wcsrchr(path, L'\\');
    if (last) *(last + 1) = L'\0';
    return path;
}
// Usage: std::wstring profilesDir = GetExeFolder() + L"profiles\\";
```

### File Enumeration
```cpp
// Source: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew
WIN32_FIND_DATAW fd;
HANDLE hFind = FindFirstFileW((profilesDir + L"*.json").c_str(), &fd);
if (hFind != INVALID_HANDLE_VALUE) {
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            // process fd.cFileName
            ;
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}
```

### UTF-8 File Content → Wide String
```cpp
// JSON files are UTF-8; GameProfile strings are wchar_t*
// Source: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), len);
    result.resize(len - 1);  // strip null terminator from std::wstring
    return result;
}
```

### Atomic JSON Save (Write-Temp + Rename)
```cpp
// Source: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexa
// MOVEFILE_REPLACE_EXISTING is atomic on same NTFS volume
std::wstring tmpPath = jsonPath + L".tmp";
HANDLE hTmp = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
if (hTmp != INVALID_HANDLE_VALUE) {
    DWORD written;
    WriteFile(hTmp, content.data(), (DWORD)content.size(), &written, nullptr);
    CloseHandle(hTmp);
    MoveFileExW(tmpPath.c_str(), jsonPath.c_str(), MOVEFILE_REPLACE_EXISTING);
}
```

### BehaviorType String → Enum
```cpp
BehaviorType ParseBehaviorType(const std::string& s) {
    if (s == "EdgeTrigger")  return BehaviorType::EdgeTrigger;
    if (s == "LongPress")    return BehaviorType::LongPress;
    if (s == "WheelToKey")   return BehaviorType::WheelToKey;
    return BehaviorType::HoldToToggle;  // default / "HoldToToggle"
}
```

### Dynamic g_gameProfiles Array
```cpp
// Replace static array in CustomControlZ.cpp:
//   GameProfile* g_gameProfiles[] = { ... };   <-- DELETE
//   constexpr int g_gameProfileCount = ...;    <-- DELETE
// With:
GameProfile** g_gameProfiles     = nullptr;
int           g_gameProfileCount = 0;
// Populated by LoadProfilesFromFolder() during wWinMain startup.
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `#include "games/EldenRing.h"` in CustomControlZ.cpp | `LoadProfilesFromFolder()` reads `profiles/*.json` at startup | Phase 2 | New profiles require no recompilation |
| Static `GameProfile* g_gameProfiles[]` compile-time array | Runtime `GameProfile**` pointer + `g_gameProfileCount` | Phase 2 | Array size determined by file count |
| `SaveConfig` → `WritePrivateProfileString` (settings.ini) | `SaveProfileBindings` → rewrite `.json` file with updated `currentVk` | Phase 2 | Bindings stored in per-game JSON files |
| `LoadConfig` → `GetPrivateProfileInt` (settings.ini) | `LoadProfileFromJson` reads `currentVk` from JSON; falls back to settings.ini for migration | Phase 2 | Single source of truth for binding values |
| Hardcoded `iniSection` string in C++ header | Derived from JSON `id` field (set `iniSection = id`) | Phase 2 | Eliminates redundant field in JSON schema |

**Deprecated/outdated after Phase 2:**
- `games/EldenRing.h` and `games/ToxicCommando.h`: deleted from source tree and project file.
- `#include "games/EldenRing.h"` and `#include "games/ToxicCommando.h"` in CustomControlZ.cpp: removed.
- Per-profile `#include` pattern: no longer used; profiles are discovered at runtime.

---

## Open Questions

1. **Toxic Commando actual process name**
   - What we know: `ToxicCommando.exe` is a placeholder. The comment in ToxicCommando.h says "UPDATE with real process name before release."
   - What's unclear: The actual Windows process name of the Toxic Commando game executable. This must be confirmed by the developer before shipping the JSON file.
   - Recommendation: Create the JSON file with `"processName1": "ToxicCommando.exe"` and a comment (or a placeholder note in the JSON, or in the PLAN documentation). The developer confirms and updates the JSON file before smoke testing.

2. **JSON string format for special characters in labels**
   - What we know: Binding labels like `"Backstep, Dodge Roll, Dash"` contain only ASCII. The settings window title `"Elden Ring - Key Bindings"` is also ASCII.
   - What's unclear: If any label or string needs non-ASCII characters (e.g., German umlauts for future profiles), the UTF-8 JSON parser must handle them correctly via `MultiByteToWideChar(CP_UTF8, ...)`.
   - Recommendation: Use `MultiByteToWideChar(CP_UTF8, ...)` for all string values from the JSON file — handles both ASCII and non-ASCII correctly.

3. **COLORREF encoding in JSON**
   - What we know: COLORREF is `0x00BBGGRR`. Human-readable hex for a color is typically `0xRRGGBB`. These are byte-swapped.
   - What's unclear: Whether to store COLORREF as a raw decimal int, as `0xBBGGRR`, or as a swapped `0xRRGGBB` with parsing adjustment.
   - Recommendation: Store as plain decimal integer (e.g., `"bg": 987151`). No byte-swap ambiguity. The existing `RGB(25, 20, 15)` macro result `== 987151` == `0x0F1419`. Conversion: `RGB(r,g,b) = r + (g<<8) + (b<<16)`. When writing the JSON files, compute decimal from existing RGB() calls in the headers.

4. **Font preference migration**
   - What we know: Font name is currently stored in settings.ini under `[UI] FontName`.
   - What's unclear: Whether to keep it in settings.ini or move it to a global app config JSON.
   - Recommendation: Keep font in settings.ini for Phase 2 (no change). Moving it is a Phase 3+ concern.

---

## Validation Architecture

`nyquist_validation` is enabled in `.planning/config.json`.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | None — Win32 binary, manual smoke tests only (same as Phase 1) |
| Config file | None |
| Quick run command | Manual: launch app, verify profile loads and game select shows correct names |
| Full suite command | Manual: run all 4 Phase 2 success criteria scenarios |

### Phase Requirements → Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PROF-01 | `profiles\` folder next to exe contains `.json` files; removing a file removes that game on next launch | manual-smoke | (1) Create `profiles\` with 2 JSON files, launch — see 2 games. (2) Remove one JSON, relaunch — see 1 game. | N/A — Wave 0 |
| PROF-02 | Auto-discovery: adding a new `.json` to `profiles\` and restarting causes new game to appear | manual-smoke | Copy a third JSON file to `profiles\`, relaunch — see 3 games in select window | N/A — Wave 0 |
| PROF-03 | Elden Ring and TC load from JSON and behave identically to Phase 1 hardcoded implementations | manual-smoke | Launch app, select Elden Ring, verify sprint/dodge/longpress; select TC, verify scroll-wheel | N/A — Wave 0 |
| PROF-04 | App starts with no settings.ini binding data — binding values come from JSON files | manual-smoke | Delete settings.ini, launch app, select Elden Ring, open settings — bindings show JSON `currentVk` values | N/A — Wave 0 |

**Justification for manual-only:** Same rationale as Phase 1 — Win32 system-tray app with no automated test infrastructure. File I/O behavior is observable by inspecting the game selection window and settings window. PROF-04 migration is verified by deleting settings.ini and confirming defaults load from JSON.

### Wave 0 Gaps

- [ ] `profiles/EldenRing.json` — covers PROF-01, PROF-02, PROF-03, PROF-04 for Elden Ring profile
- [ ] `profiles/ToxicCommando.json` — covers PROF-01, PROF-02, PROF-03, PROF-04 for Toxic Commando profile
- [ ] `CustomControlZ/ProfileLoader.h` — the profile loading/saving module (new file)

*(No automated test framework needed — manual smoke testing is the established validation approach for this project.)*

---

## Sources

### Primary (HIGH confidence)

- [GetModuleFileNameW — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew) — exe-relative path resolution
- [FindFirstFileW — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew) — file enumeration pattern
- [ReadFile — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile) — binary file read pattern
- [MoveFileExW — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexa) — atomic rename for safe JSON save
- [MultiByteToWideChar — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar) — UTF-8 → wchar_t conversion
- `CustomControlZ/GameProfiles.h` — `GameProfile` struct, `KeyBinding` struct, `Theme` struct (fields to be populated by parser)
- `CustomControlZ/BehaviorEngine.h` — `BehaviorType` enum, `BehaviorDescriptor` struct (field names for JSON mapping)
- `CustomControlZ/games/EldenRing.h` — reference data for EldenRing.json content
- `CustomControlZ/games/ToxicCommando.h` — reference data for ToxicCommando.json content
- `CustomControlZ/CustomControlZ.cpp` — `SaveConfig`, `LoadConfig`, `g_gameProfiles` array patterns to replace

### Secondary (MEDIUM confidence)

- JSON spec (RFC 8259) — JSON number, string, null, array, object grammar used to design parser strategy
- Win32 NTFS atomicity guarantee: `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` is atomic when source and destination are on the same NTFS volume — verified by Raymond Chen's Old New Thing blog and Windows internals documentation

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all APIs are existing Win32 primitives; no new dependencies; constraint confirmed from PROJECT.md
- JSON schema: HIGH — derived directly from existing `GameProfile` struct fields; no guessing
- Architecture patterns: HIGH — based on reading actual CustomControlZ.cpp source code and existing patterns
- Parser strategy: MEDIUM — bespoke parser is the right call given constraints, but exact implementation details depend on final schema decisions (COLORREF encoding, optional field handling)
- Pitfalls: HIGH — all identified from direct code inspection (dangling pointers, null processName2, COLORREF byte order)

**Research date:** 2026-03-12
**Valid until:** 2026-09-12 (Win32 APIs are stable; JSON spec is final)
