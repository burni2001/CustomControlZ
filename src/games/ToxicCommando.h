#pragma once
#include "../GameProfiles.h"

// Binding indices for Toxic Commando
enum ToxicCommandoBinding {
    TC_KEY_SCROLL_TOGGLE = 0, // Key that cycles weapon each press (single in-game key)
    TC_KEY_MELEE         = 1, // Melee combo: tap = switch+click+switch back; hold = switch+hold+switch back on release
    TC_KEY_OVERRIDE      = 2, // Toggle: first press suspends all custom bindings; second press re-enables
    TC_BINDING_COUNT     = 3
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
    /* posterFile    */ L"assets\\toxic.png",
    /* theme */ {
        RGB(0, 0, 0),       // bg         - black
        RGB(255, 255, 255), // text        - white
        RGB(255, 255, 255), // accent      - white
        RGB(20, 20, 20),    // button      - near black
        RGB(50, 18, 18),    // selectBg    - soft dark red for game-select card
        RGB(200, 50, 50),   // border      - red
        RGB(10, 10, 10),    // rowBg       - near black
        RGB(180, 30, 30),   // separator   - red
        RGB(160, 20, 20),   // exitFill    - dark red
        RGB(220, 50, 50),   // exitBorder  - bright red
        RGB(255, 255, 255), // exitText    - white
        RGB(200, 50, 50),   // minimizeBorder - red
    },
    /* bindingCount */ TC_BINDING_COUNT,
    /* bindings */ {
        // TC_KEY_SCROLL_TOGGLE: KeyToggle — each press cycles the weapon using the single in-game weapon key.
        // Both outputVk and longOutputVk use the same key since the game has one weapon-cycle binding.
        { L"ScrollToggleKey", L"Custom key: Quickswap weapons", 'Q', 'Q',
          { BehaviorType::KeyToggle, /*outputVk=*/'2', /*longOutputVk=*/'1',
            /*thresholdMs=*/0, /*durationMs=*/50, /*wheelDelta=*/0, /*attackVk=*/0, /*returnDelayMs=*/0,
            /*outputVkLabel=*/L"Primary Weapon", /*longOutputVkLabel=*/L"Secondary Weapon" },
          /*isAppOnly=*/true },

        // TC_KEY_MELEE: MeleeBurst — repeated taps stay on melee; auto-switch back after idle
        //   tap:        switch to melee (held), wait, click LMB; melee key stays held for subsequent taps
        //   long press: switch to melee (held), hold LMB until V released, then switch back
        //   idle:       auto-release melee key and press main weapon key to return
        // outputVk = switch-to-melee key; longOutputVk = switch-back (main weapon) key; attackVk = LMB
        { L"MeleeKey", L"Custom key: Quick melee attack", 'V', 'V',
          { BehaviorType::MeleeBurst, /*outputVk=*/VK_OEM_5, /*longOutputVk=*/'2',
            /*thresholdMs=*/TC_MELEE_THRESHOLD_MS, /*durationMs=*/TC_WEAPON_SWITCH_MS,
            /*wheelDelta=*/0, /*attackVk=*/VK_LBUTTON, /*returnDelayMs=*/750,
            /*outputVkLabel=*/L"Melee", /*longOutputVkLabel=*/nullptr },
          /*isAppOnly=*/true, /*separatorAbove=*/true },

        // TC_KEY_OVERRIDE: GlobalSuspend — first press suspends all custom bindings (Return passes through to game);
        //   second press re-enables. Any held keys are released on entering suspend.
        { L"OverrideKey", L"Text Chat", 0, 0,
          { BehaviorType::GlobalSuspend, /*outputVk=*/0, /*longOutputVk=*/0,
            /*thresholdMs=*/0, /*durationMs=*/0, /*wheelDelta=*/0, /*attackVk=*/0, /*returnDelayMs=*/0,
            /*outputVkLabel=*/nullptr, /*longOutputVkLabel=*/nullptr },
          /*isAppOnly=*/false, /*separatorAbove=*/true },
    },
    /* logicFn */ GenericLogicThreadFn,
};
