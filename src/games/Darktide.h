#pragma once
#include "../GameProfiles.h"

// Binding indices for Darktide
enum DarktideBinding {
    DT_KEY_MELEE    = 0, // Melee combo: tap = switch+click+switch back; hold = switch+hold+switch back on release
    DT_KEY_OVERRIDE = 1, // Toggle: first press suspends all custom bindings (Enter passes to game); second press re-enables
    DT_BINDING_COUNT = 2
};

constexpr int DT_WEAPON_SWITCH_MS   = 350;  // Delay after weapon-switch key before firing attack (ms)
constexpr int DT_MELEE_THRESHOLD_MS = 400;  // Hold duration to trigger long-press melee mode (ms)

static GameProfile g_DarktideProfile = {
    /* id            */ L"Darktide",
    /* displayName   */ L"Darktide",
    /* iniSection    */ L"Darktide",
    /* processName1  */ L"Darktide.exe",
    /* processName2  */ nullptr,
    /* tipActive     */ L"CustomControlZ - Darktide: ACTIVE",
    /* tipIdle       */ L"CustomControlZ: Waiting...",
    /* settingsTitle */ L"Warhammer 40,000: Darktide - Key Bindings",
    /* posterFile    */ L"assets\\darktide.png",
    /* theme */ {
        RGB(18,  28,  18),  // bg              - dark grey-green (settings background)
        RGB(140, 235, 140), // text             - light green (settings font)
        RGB(220,  80,  80), // accent           - light red (game-card font, select screen)
        RGB(25,  38,  25),  // button           - dark green button fill
        RGB(35,  50,  35),  // selectBg         - very faded dark black-green (game-card background, select screen)
        RGB(60,  120,  60), // border           - medium green
        RGB(28,  40,  28),  // rowBg            - slightly lighter row highlight
        RGB(60,  120,  60), // separator        - green separator lines
        RGB(120,  25,  25), // exitFill         - dark red exit button
        RGB(200,  60,  60), // exitBorder       - light red exit border
        RGB(140, 235, 140), // exitText         - light green exit text
        RGB(60,  120,  60), // minimizeBorder   - green minimize border
        true,               // scanlines        - CRT scanline overlay on settings background
    },
    /* bindingCount */ DT_BINDING_COUNT,
    /* bindings */ {
        // DT_KEY_MELEE: MeleeBurst — repeated taps stay on melee; auto-switch back after idle
        //   tap:        switch to melee weapon (held), wait, click LMB; melee key stays held for subsequent taps
        //   long press: switch to melee weapon (held), hold LMB until key released, then switch back
        //   idle:       auto-release melee key and press ranged weapon key to return
        // outputVk = switch-to-melee key (slot 3); longOutputVk = switch-back (slot 1); attackVk = LMB
        { L"MeleeKey", L"Custom key: Quick melee attack", 'V', 'V',
          { BehaviorType::MeleeBurst, /*outputVk=*/'3', /*longOutputVk=*/'1',
            /*thresholdMs=*/DT_MELEE_THRESHOLD_MS, /*durationMs=*/DT_WEAPON_SWITCH_MS,
            /*wheelDelta=*/0, /*attackVk=*/VK_LBUTTON, /*returnDelayMs=*/750,
            /*outputVkLabel=*/L"Melee", /*longOutputVkLabel=*/nullptr },
          /*isAppOnly=*/true },

        // DT_KEY_OVERRIDE: GlobalSuspend — first press suspends all custom bindings (Enter passes through to game for chat);
        //   second press re-enables. Any held keys are released on entering suspend.
        { L"OverrideKey", L"Enter for Chat", VK_RETURN, VK_RETURN,
          { BehaviorType::GlobalSuspend, /*outputVk=*/0, /*longOutputVk=*/0,
            /*thresholdMs=*/0, /*durationMs=*/0, /*wheelDelta=*/0, /*attackVk=*/0, /*returnDelayMs=*/0,
            /*outputVkLabel=*/nullptr, /*longOutputVkLabel=*/nullptr },
          /*isAppOnly=*/false, /*separatorAbove=*/true },
    },
    /* logicFn */ GenericLogicThreadFn,
};
