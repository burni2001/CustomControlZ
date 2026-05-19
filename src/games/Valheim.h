#pragma once
#include "../GameProfiles.h"

enum ValheimBinding {
    VH_KEY_CROUCH       = 0,  // InGameKey: Crouch; also the hold target for VH_KEY_DODGE_OR_CROUCH
    VH_KEY_DODGE_OR_CROUCH = 1, // TapComboOrHold: tap = dodge roll (Block+Jump), hold = Crouch
    VH_KEY_HAMMER_8     = 2,  // InGameKey: item slot 8 / Hammer; also the tap target for Hammer Time
    VH_KEY_HAMMER_TIME  = 3,  // LongPress: tap = Item slot 8, hold = Crouch
    VH_BINDING_COUNT    = 4
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
        // VH_KEY_CROUCH: InGameKey -- looked up at runtime by TapComboOrHold (hold) and LongPress (hold)
        { L"CrouchKey", L"Crouch",
          'C', 'C',
          { BehaviorType::InGameKey },
          /*isAppOnly=*/false },

        // VH_KEY_DODGE_OR_CROUCH: TapComboOrHold -- tap = dodge roll, hold = crouch
        // Hold target resolved at runtime from nearest preceding InGameKey (Crouch above)
        { L"DodgeOrCrouchKey", L"Custom Key: Dodge / Crouch",
          'C', 'C',
          { .type = BehaviorType::TapComboOrHold,
            .outputVk = VK_XBUTTON2,
            .longOutputVk = VK_SPACE,
            .thresholdMs = 400,
            .durationMs = 15,
            .outputVkLabel = L"Block key",
            .longOutputVkLabel = L"Jump key" },
          /*isAppOnly=*/true },

        // VH_KEY_HAMMER_8: InGameKey -- looked up at runtime by LongPress (tap)
        { L"HammerKey", L"Item slot 8 (Hammer)",
          '8', '8',
          { BehaviorType::InGameKey },
          /*isAppOnly=*/false, /*separatorAbove=*/true },

        // VH_KEY_HAMMER_TIME: LongPress -- tap = nearest preceding InGameKey (Hammer/8), hold = second (Crouch/C)
        { L"HammerTimeKey", L"Custom Key: Hammer Time",
          'B', 'B',
          { .type = BehaviorType::LongPress,
            .outputVk = 0,
            .longOutputVk = 0,
            .thresholdMs = 400 },
          /*isAppOnly=*/true },
    },
    /* logicFn */ GenericLogicThreadFn,
};
