#pragma once
#include "../GameProfiles.h"

// Binding indices for Toxic Commando
enum ToxicCommandoBinding {
    TC_KEY_SCROLL_TOGGLE = 0, // Key that alternates scroll wheel up/down each press
    TC_KEY_MELEE         = 1, // Melee combo: tap = switch+click+switch back; hold = switch+hold+switch back on release
    TC_BINDING_COUNT     = 2
};

constexpr DWORD TC_SCROLL_DELTA        = 120;       // Base delta; WheelToggle alternates sign each press
constexpr int   TC_WEAPON_SWITCH_MS    = 350;        // Delay after weapon-switch key before firing attack (ms)
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
        // TC_KEY_SCROLL_TOGGLE: KeyToggle — each rising edge alternates between pressing '1' and '2'
        { L"ScrollToggleKey", L"Cycle Weapon (Key Toggle)", VK_PRIOR, VK_PRIOR,
          { BehaviorType::KeyToggle, /*outputVk=*/'1', /*longOutputVk=*/'2',
            /*thresholdMs=*/0, /*durationMs=*/50 } },

        // TC_KEY_MELEE: MeleeBurst — repeated taps stay on melee; auto-switch back after 500ms idle
        //   tap:       switch to melee (^, held), wait, click LMB; ^ stays held for subsequent taps
        //   long press: switch to melee (^, held), hold LMB until V released, then switch back
        //   idle 500ms: auto-release ^ and press 2 to return to main weapon
        // outputVk = switch-to-melee key; longOutputVk = switch-back key; attackVk = LMB
        { L"MeleeKey", L"Melee Attack (Combo Key)", 'V', 'V',
          { BehaviorType::MeleeBurst, /*outputVk=*/VK_OEM_5, /*longOutputVk=*/'2',
            /*thresholdMs=*/TC_MELEE_THRESHOLD_MS, /*durationMs=*/TC_WEAPON_SWITCH_MS,
            /*wheelDelta=*/0, /*attackVk=*/VK_LBUTTON, /*returnDelayMs=*/750 } },
    },
    /* logicFn */ GenericLogicThreadFn,
};
