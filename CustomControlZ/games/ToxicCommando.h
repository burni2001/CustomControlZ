#pragma once
#include "../GameProfiles.h"

// Binding indices for Toxic Commando
enum ToxicCommandoBinding {
    TC_KEY_SCROLL_UP   = 0, // Key that simulates scroll wheel up (next weapon)
    TC_KEY_SCROLL_DOWN = 1, // Key that simulates scroll wheel down (prev weapon)
    TC_BINDING_COUNT   = 2
};

constexpr DWORD TC_SCROLL_DELTA_UP   = 120;
constexpr DWORD TC_SCROLL_DELTA_DOWN = static_cast<DWORD>(-120);

static GameProfile g_ToxicCommandoProfile = {
    /* id            */ L"ToxicCommando",
    /* displayName   */ L"Toxic Commando",
    /* iniSection    */ L"ToxicCommando",
    /* processName1  */ L"ToxicCommando.exe",  // UPDATE with real process name before release
    /* processName2  */ nullptr,
    /* tipActive     */ L"CustomControlZ - Toxic Commando: ACTIVE",
    /* tipIdle       */ L"CustomControlZ: Waiting...",
    /* settingsTitle */ L"Toxic Commando - Key Bindings",
    /* theme */ {
        RGB(30, 50, 20),    // bg         - dark olive
        RGB(180, 200, 140), // text        - light olive/sage
        RGB(100, 180, 50),  // accent      - bright military green
        RGB(45, 65, 30),    // button
        RGB(70, 110, 40),   // border
        RGB(40, 60, 25),    // rowBg
        RGB(85, 130, 50),   // separator
        RGB(160, 40, 40),   // exitFill
        RGB(190, 70, 70),   // exitBorder
        RGB(255, 210, 210), // exitText
        RGB(60, 90, 40),    // minimizeBorder
    },
    /* bindingCount */ TC_BINDING_COUNT,
    /* bindings */ {
        // TC_KEY_SCROLL_UP: WheelToKey — rising edge fires mouse wheel up (next weapon)
        { L"ScrollUpKey",   L"Next Weapon (Scroll Up Key)",   VK_PRIOR, VK_PRIOR,
          { BehaviorType::WheelToKey, /*outputVk=*/0, /*longOutputVk=*/0,
            /*thresholdMs=*/400, /*durationMs=*/50, /*wheelDelta=*/TC_SCROLL_DELTA_UP } },

        // TC_KEY_SCROLL_DOWN: WheelToKey — rising edge fires mouse wheel down (prev weapon)
        { L"ScrollDownKey", L"Prev Weapon (Scroll Down Key)", VK_NEXT,  VK_NEXT,
          { BehaviorType::WheelToKey, /*outputVk=*/0, /*longOutputVk=*/0,
            /*thresholdMs=*/400, /*durationMs=*/50, /*wheelDelta=*/TC_SCROLL_DELTA_DOWN } },
    },
    /* logicFn */ GenericLogicThreadFn,
};
