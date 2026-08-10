#pragma once
#include "../GameProfiles.h"

// Binding indices for Elden Ring
enum EldenRingBinding {
    ER_KEY_INGAME    = 0, // The actual in-game dodge/dash key (remapped output) — no-op descriptor
    ER_KEY_DODGE     = 1, // Custom dodge trigger key — EdgeTrigger (app-only)
    ER_KEY_SPRINT    = 2, // Sprint hold key — HoldToToggle (app-only)
    ER_KEY_TRIGGER   = 3, // Long-press trigger key — LongPress (tap=Escape, hold=Q)
    ER_BINDING_COUNT = 4
};

// Action key fired on long-hold of ER_KEY_TRIGGER
constexpr WORD ER_ACTION_VK          = 'Q';

// Timing constants used as descriptor parameters
constexpr int  ER_LONG_PRESS_DELAY_MS = 400;
constexpr int  ER_DODGE_DURATION_MS   = 50;
// Elden Ring's F key is dual-purpose in-game: a quick tap rolls/dodges, a sustained
// hold sprints. SprintKey must therefore withhold its press until the input has been
// held past this delay — otherwise a quick tap of the custom sprint key itself reads
// to the game as a dodge tap.
constexpr int  ER_SPRINT_HOLD_DELAY_MS = 75;

static GameProfile g_EldenRingProfile = {
    /* id            */ L"EldenRing",
    /* displayName   */ L"ELDEN RING",
    /* iniSection    */ L"EldenRing",
    /* processName1  */ L"eldenring.exe",
    /* processName2  */ L"ersc_launcher.exe",
    /* tipActive     */ L"CustomControlZ - Elden Ring: ACTIVE",
    /* tipIdle       */ L"CustomControlZ: Waiting...",
    /* settingsTitle */ L"ELDEN RING - Key Bindings",
    /* theme */ {
        RGB(25, 20, 15),    // bg
        RGB(220, 195, 145), // text
        RGB(255, 215, 120), // accent
        RGB(40, 35, 25),    // button
        RGB(40, 35, 25),    // selectBg    - same as button for game-select card
        RGB(120, 100, 60),  // border
        RGB(45, 38, 28),    // rowBg
        RGB(140, 115, 70),  // separator
        RGB(180, 50, 50),   // exitFill
        RGB(200, 80, 80),   // exitBorder
        RGB(255, 220, 220), // exitText
        RGB(80, 70, 55),    // minimizeBorder
    },
    /* bindingCount */ ER_BINDING_COUNT,
    /* bindings */ {
        // ER_KEY_INGAME: no-op — user rebinds this to the in-game dodge/dash key.
        // outputVk=0 means HoldToToggle does nothing; currentVk is used as outputVk
        // by SPRINT and DODGE (hardcoded in descriptors below).
        { L"IngameKey",  L"Backstep, Dodge Roll, Dash",      'F',        'F',
          { BehaviorType::HoldToToggle, /*outputVk=*/0 },
          /*isAppOnly=*/false },

        // ER_KEY_DODGE: EdgeTrigger — rising edge fires the in-game key for 50ms
        { L"DodgeKey",   L"Custom key: Dodge roll",          'C',        'C',
          { BehaviorType::EdgeTrigger, /*outputVk=*/'F', /*longOutputVk=*/0,
            /*thresholdMs=*/ER_LONG_PRESS_DELAY_MS, /*durationMs=*/ER_DODGE_DURATION_MS },
          /*isAppOnly=*/true },

        // ER_KEY_SPRINT: HoldToToggle — holds the in-game key while input key is held.
        // holdDelayMs prevents a quick tap from reading as a dodge (see comment above).
        // holdDelayLabel exposes it as a tunable field in the settings UI.
        { L"SprintKey",  L"Custom Key: Dash, Sprint",        VK_CAPITAL, VK_CAPITAL,
          { .type = BehaviorType::HoldToToggle, .outputVk = 'F', .holdDelayMs = ER_SPRINT_HOLD_DELAY_MS,
            .holdDelayLabel = L"Sprint engage delay (ms):", .requireMovementKey = true },
          /*isAppOnly=*/true },

        // ER_KEY_TRIGGER: LongPress — tap fires Escape (close/back), hold fires Q (action)
        { L"TriggerKey", L"Custom Key: Close, Back (Long press)", VK_ESCAPE,  VK_ESCAPE,
          { BehaviorType::LongPress, /*outputVk=*/VK_ESCAPE, /*longOutputVk=*/ER_ACTION_VK,
            /*thresholdMs=*/ER_LONG_PRESS_DELAY_MS },
          /*isAppOnly=*/true, /*separatorAbove=*/true },
    },
    /* logicFn */ GenericLogicThreadFn,
};
