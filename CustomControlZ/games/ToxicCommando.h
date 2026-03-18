#pragma once
#include "../GameProfiles.h"

// Binding indices for Toxic Commando
enum ToxicCommandoBinding {
    TC_KEY_SCROLL_TOGGLE = 0, // Key that alternates scroll wheel up/down each press
    TC_KEY_MELEE         = 1, // Melee combo: tap = switch+click+switch back; hold = switch+hold+switch back on release
    TC_BINDING_COUNT     = 2
};

constexpr DWORD TC_SCROLL_DELTA        = 120;       // Base delta; WheelToggle alternates sign each press
constexpr int   TC_WEAPON_SWITCH_MS    = 100;        // Delay after weapon-switch key before firing attack (ms)
constexpr int   TC_MELEE_THRESHOLD_MS  = 400;        // Hold duration to trigger long-press melee mode (ms)

static GameProfile g_ToxicCommandoProfile = {
    /* id            */ L"ToxicCommando",
    /* displayName   */ L"Toxic Commando",
    /* iniSection    */ L"ToxicCommando",
    /* processName1  */ L"John Carpenter's Toxic Commando.exe",
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
        // TC_KEY_SCROLL_TOGGLE: WheelToggle — each rising edge alternates scroll up / scroll down
        { L"ScrollToggleKey", L"Cycle Weapon (Scroll Toggle Key)", VK_PRIOR, VK_PRIOR,
          { BehaviorType::WheelToggle, /*outputVk=*/0, /*longOutputVk=*/0,
            /*thresholdMs=*/400, /*durationMs=*/50, /*wheelDelta=*/TC_SCROLL_DELTA } },

        // TC_KEY_MELEE: WeaponCombo — tap: switch to melee (^), click LMB, switch back (2)
        //                             hold: switch to melee, hold LMB until released, switch back
        // outputVk = switch-to-melee key; longOutputVk = switch-back key; attackVk = LMB
        // Default keys are configurable in the settings window.
        { L"MeleeKey", L"Melee Attack (Combo Key)", 'V', 'V',
          { BehaviorType::WeaponCombo, /*outputVk=*/VK_OEM_5, /*longOutputVk=*/'2',
            /*thresholdMs=*/TC_MELEE_THRESHOLD_MS, /*durationMs=*/TC_WEAPON_SWITCH_MS,
            /*wheelDelta=*/0, /*attackVk=*/VK_LBUTTON } },
    },
    /* logicFn */ GenericLogicThreadFn,
};
