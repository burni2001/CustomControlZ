# Stack Research

**Domain:** Data-driven profile/configuration system for C++20 Win32 desktop application
**Researched:** 2026-03-11
**Confidence:** HIGH (core format and library recommendations), MEDIUM (compile-time impact estimates)

## Context

This research targets one specific question: how to add a data-driven game profile system to an existing C++20/Win32 single-exe app (CustomControlZ) without adding any runtime dependencies. The existing stack is MSVC v143, MSBuild, `settings.ini` via `WritePrivateProfileString`. Everything recommended here must drop into that context as a header-only addition.

---

## Recommended Stack

### Core Technologies (Unchanged — Already in Place)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| C++20 | MSVC v143 | Core implementation language | Already in use; C++20 structured bindings and `std::string_view` help with config parsing code |
| Win32 API | Windows 10+ | File I/O, path resolution, UI | `GetModuleFileNameW` + `PathCchRemoveFileSpec` give exe-relative profile paths without any additional library |
| MSBuild | Visual Studio 2022 | Build system | Already in use; header-only additions require only `#include` — no .vcxproj changes |

### Profile Format: JSON via nlohmann/json

**Recommendation: JSON + nlohmann/json v3.12.0**

JSON is the right format for game profiles. TOML is more human-readable for flat config, but game profiles need arrays (multiple key bindings per profile, multiple behavior parameters per binding) and JSON handles nested structure naturally. Users won't hand-edit profiles directly — the in-app UI editor writes them — so human editability is secondary.

nlohmann/json is the recommendation over alternatives because:
- Single-file drop-in: copy `json.hpp` (~1 MB source) into the project, add to `#include` path, done. No CMake, no vcpkg, no `.vcxproj` additions beyond the include path.
- C++20 compatible: fixes and full `<ranges>` support added in recent versions; no conflicts with the existing `/std:c++20` flag.
- Dominant ecosystem choice: the de facto standard for C++ JSON; any future contributor will recognize it immediately.
- MIT license: no restrictions.
- Version 3.12.0 (April 2025) is current and actively maintained.

**Confidence: HIGH** — verified via official GitHub releases page and nlohmann.me docs.

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| nlohmann/json | 3.12.0 | Profile file parsing and serialization | All profile read/write operations |
| Win32 `WritePrivateProfileString` / `GetPrivateProfileInt` | (built-in) | Keep for settings.ini (app-level settings, font name) | Retain for non-profile user preferences that already use INI |

### Development Tools (Unchanged)

| Tool | Purpose | Notes |
|------|---------|-------|
| Visual Studio 2022 | IDE + MSVC compiler | No changes needed for header-only addition |
| MSBuild | Build execution | Add `$(ProjectDir)vendor\` to `AdditionalIncludeDirectories` for json.hpp |

---

## Profile File Layout (Data Architecture Decision)

**Recommendation: One JSON file per game profile, stored next to the .exe**

```
CustomControlz.exe
profiles\
    EldenRing.json
    ToxicCommando.json
    MyNewGame.json
settings.ini          ← keep for app-level prefs (font, last selected game)
```

**Why one file per profile, not one master file:**
- Adding a new game = creating a new file. No risk of corrupting another profile's JSON.
- The UI editor writes only the active profile's file. No parse-and-reserialize of the whole registry on each save.
- Discovery is `FindFirstFileW("profiles\\*.json")` — no hardcoded list of profiles anywhere in C++.

**Why `profiles\` subdirectory, not flat next to the .exe:**
- Keeps the exe directory clean.
- Matches the existing pattern: `settings.ini` is already flat; profiles are a new concept and deserve a container.

**Path resolution pattern (Win32, no new deps):**

```cpp
// Get directory of running .exe, then append relative path
wchar_t exePath[MAX_PATH];
GetModuleFileNameW(nullptr, exePath, MAX_PATH);
PathCchRemoveFileSpec(exePath, MAX_PATH);  // requires pathcch.h, links pathcch.lib
// exePath is now the exe's directory; append L"\\profiles\\"
```

`pathcch.lib` is a Windows 8+ system library — no new external dependency. Link via `#pragma comment(lib, "pathcch.lib")` consistent with existing `uxtheme.lib` and `dwmapi.lib` patterns.

**Confidence: HIGH** — `GetModuleFileNameW` + `PathCchRemoveFileSpec` is the documented Win32 pattern; `pathcch.lib` ships with the Windows SDK already referenced by the project.

---

## Profile JSON Schema (Recommended Structure)

```json
{
  "id": "EldenRing",
  "displayName": "Elden Ring",
  "processNames": ["eldenring.exe"],
  "iniSection": "EldenRing",
  "theme": {
    "background": "#1A1A2E",
    "accent": "#E8B86D"
  },
  "bindings": [
    {
      "id": "sprint",
      "label": "Hold to Sprint",
      "defaultVk": 16,
      "currentVk": 16,
      "behavior": {
        "type": "hold_to_toggle",
        "outputVk": 17,
        "holdThresholdMs": 0
      }
    }
  ]
}
```

This maps directly to the existing `GameProfile` and `KeyBinding` structs. The `behavior.type` field is the discriminator that replaces the hardcoded `logicFn` pointer — the generic engine reads it at runtime and dispatches to the appropriate preset handler.

---

## Installation

No package manager. Drop the file directly into the source tree:

```
CustomControlz/
    vendor/
        nlohmann/
            json.hpp        ← download from github.com/nlohmann/json/releases/tag/v3.12.0
    CustomControlz.cpp
    GameProfiles.h
```

In `CustomControlZ.vcxproj`, add to `AdditionalIncludeDirectories`:
```
$(ProjectDir)vendor\
```

Then include:
```cpp
#include <nlohmann/json.hpp>
```

No other `.vcxproj` edits required. No linker changes. The existing `MultiThreaded` static runtime setting is compatible.

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| JSON (nlohmann/json) | TOML (toml++) | If users will hand-edit profile files frequently AND nested arrays aren't needed. TOML is more ergonomic for flat key-value config, but awkward for arrays of objects (bindings). Not the right fit here. |
| JSON (nlohmann/json) | Custom INI extension | Never. The existing INI is adequate for simple key=value pairs but cannot represent the behavior parameter block (type + threshold + outputVk) without invented syntax. Adding custom INI sections for behaviors is a parsing maintenance burden with no upside. |
| JSON (nlohmann/json) | RapidJSON | Only if parse performance is a bottleneck. Profile files are <5 KB and parsed once at profile load. RapidJSON's SAX API is significantly more verbose and offers no practical benefit at this scale. |
| JSON (nlohmann/json) | XML | Never for this project. Win32 has `IXMLDOMDocument` but it requires COM initialization overhead. No meaningful advantage over JSON for this data shape. |
| JSON (nlohmann/json) | Hardcoded C++ structs (current approach) | Explicitly ruled out — the whole point of the milestone is to remove this. |
| `profiles\*.json` discovery | Profiles listed in settings.ini | Adding a game via the UI should not require editing a secondary index file. Dynamic discovery is more robust. |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| YAML | No mature single-header C++17 YAML parser exists. yaml-cpp requires building a static library, which violates the no-new-runtime-dependencies constraint. | JSON with nlohmann/json |
| toml++ for profile format | The `[[array of tables]]` TOML syntax for repeated sections (multiple bindings per profile) is syntactically awkward and requires non-obvious escaping. TOML is ideal for flat config, poor for structured lists. | JSON |
| `GetPrivateProfileString` extensions for behavior params | INI has no typed values, no arrays, no nesting. Inventing `[EldenRing.Bindings.Sprint]` sub-sections is a proprietary format with no tooling. | JSON |
| `std::filesystem` for path operations | Adds a dependency on the MSVC runtime's filesystem support and requires `/std:c++17` or later — already satisfied — but `GetModuleFileNameW` + `PathCchRemoveFileSpec` is lower-overhead and already idiomatic in this Win32 codebase. | `GetModuleFileNameW` + `PathCchRemoveFileSpec` |
| Registering profiles in the Windows Registry | Breaks portability. The app currently requires zero registry writes and runs from any directory. | JSON files in `profiles\` next to the exe |

---

## Stack Patterns by Variant

**If the user wants profiles to be hand-editable (power user scenario):**
- JSON is still the right format; it is more human-readable than binary alternatives
- Add a comment field (`"_comment": "..."`) as a convention since JSON has no native comments
- Consider JSON with Comments (JSONC) support: nlohmann/json does not support JSONC natively, but this is easily handled by stripping `//` lines before parsing if needed

**If compile times become a concern after adding json.hpp:**
- Isolate all nlohmann/json usage to a single `ProfileLoader.cpp` translation unit
- Pre-compiled headers (already configured in the MSVC project) will amortize the cost
- json.hpp is ~1 MB uncompressed; compile time impact on a single TU is measurable but not prohibitive on modern hardware

**If more than ~20 concurrent profiles are expected:**
- The current "scan `profiles\*.json` on startup" approach remains valid up to hundreds of files
- No index or caching needed for the foreseeable scale of this app

---

## Version Compatibility

| Component | Compatible With | Notes |
|-----------|-----------------|-------|
| nlohmann/json 3.12.0 | C++17, C++20 | Confirmed compatible with MSVC v143 `/std:c++20`; no breaking changes from 3.11.x |
| pathcch.lib | Windows 8 / SDK 10.0+ | Already required by the project's Windows 10 minimum target; `PathCchRemoveFileSpec` is the modern replacement for deprecated `PathRemoveFileSpec` |
| `FindFirstFileW` / `FindNextFileW` | All Windows versions | Used for `profiles\*.json` discovery; no minimum version concern |

---

## Sources

- [nlohmann/json GitHub — v3.12.0 release](https://github.com/nlohmann/json/releases/tag/v3.12.0) — confirmed single-header, MIT, C++20 compatible, actively maintained (HIGH confidence)
- [nlohmann/json integration docs](https://json.nlohmann.me/integration/) — confirmed single-file drop-in pattern (HIGH confidence)
- [tomlplusplus GitHub](https://github.com/marzer/tomlplusplus) — evaluated and rejected for this use case; requires C++17, has MSVC lambda processor workarounds (MEDIUM confidence on rejection rationale)
- [GetModuleFileNameA — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamea) — official Win32 pattern for exe-relative path resolution (HIGH confidence)
- WebSearch: JSON vs TOML vs INI for desktop config — corroborated TOML preference for flat config, JSON preference for structured data (MEDIUM confidence, multiple sources agree)
- WebSearch: RapidJSON header-only MSVC — confirmed header-only, but verbose SAX API not justified at this scale (MEDIUM confidence)

---

*Stack research for: CustomControlZ data-driven profile system*
*Researched: 2026-03-11*
