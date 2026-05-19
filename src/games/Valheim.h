#pragma once
#include "../GameProfiles.h"

enum ValheimBinding {
    VH_KEY_DODGE_ROLL   = 0,  // HoldAndPulse: hold Block + pulse Jump = dodge roll
    VH_KEY_HAMMER_8     = 1,  // InGameKey: item slot 8 / Hammer
    VH_KEY_HAMMER_TIME  = 2,  // SimulateKey: custom key that presses item slot 8
    VH_BINDING_COUNT    = 3
};

static GameProfile g_ValheimProfile = {
    /* id            */ L"Valheim",
    /* displayName   */ L"VALHEIM",
    /* iniSection    */ L"Valheim",
    /* processName1  */ L"valheim.exe",
    /* processName2  */ nullptr,
    /* tipActive     */ L"CustomControlZ - Valheim: ACTIVE",
    /* tipIdle       */ L"CustomControlZ: Waiting...",
    /* settingsTitle */ L"VALHEIM - Key Bindings",
    /* posterFile    */ L"assets\\valheim.png",
    /* theme */ {
        RGB(48, 40, 55),    // bg              -- purple-grey (settings window)
        RGB(230, 165, 40),  // text            -- yellow-orange
        RGB(255, 135, 20),  // accent          -- fiery orange
        RGB(65, 54, 74),    // button          -- darker purple-grey
        RGB(12, 8, 5),      // selectBg        -- near-black (game-select card)
        RGB(200, 75, 10),   // border          -- fiery orange-red
        RGB(58, 49, 66),    // rowBg           -- slightly lighter than bg
        RGB(175, 115, 35),  // separator       -- amber-gold line
        RGB(105, 28, 28),   // exitFill        -- dark crimson
        RGB(150, 48, 48),   // exitBorder
        RGB(240, 225, 220), // exitText
        RGB(90, 78, 102),   // minimizeBorder
    },
    /* bindingCount */ VH_BINDING_COUNT,
    /* bindings */ {
        // VH_KEY_DODGE_ROLL: HoldAndPulse -- hold Block + pulse Jump = dodge roll
        { L"DodgeRollKey", L"Custom Key: Dodge Roll",
          'C', 'C',
          { .type = BehaviorType::HoldAndPulse,
            .outputVk = VK_XBUTTON2,
            .longOutputVk = VK_SPACE,
            .durationMs = 50,
            .outputVkLabel = L"Block key",
            .longOutputVkLabel = L"Jump key" },
          /*isAppOnly=*/true },

        // VH_KEY_HAMMER_8: InGameKey -- item slot 8 / Hammer
        { L"HammerKey", L"Item slot 8 (Hammer)",
          '8', '8',
          { BehaviorType::InGameKey },
          /*isAppOnly=*/false, /*separatorAbove=*/true },

        // VH_KEY_HAMMER_TIME: SimulateKey -- custom key that presses item slot 8
        { L"HammerTimeKey", L"Custom Key: Hammer Time",
          'H', 'H',
          { .type = BehaviorType::SimulateKey },
          /*isAppOnly=*/true },
    },
    /* logicFn */ GenericLogicThreadFn,
};
