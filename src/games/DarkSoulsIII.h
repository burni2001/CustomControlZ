#pragma once
#include "../GameProfiles.h"

enum DarkSoulsIIIBinding {
    DS3_KEY_WALK      = 0,  // WalkModifier: in-game walk key (injected alongside directions)
    DS3_KEY_RUN_FWD   = 1,  // WalkRunSwap: forward direction
    DS3_KEY_RUN_BACK  = 2,  // WalkRunSwap: backward direction
    DS3_KEY_RUN_LEFT  = 3,  // WalkRunSwap: left direction
    DS3_KEY_RUN_RIGHT = 4,  // WalkRunSwap: right direction
    DS3_KEY_DASH      = 5,  // DashKey: in-game Dash/Backstep/Roll key (also held by Sprint)
    DS3_KEY_SPRINT        = 6,  // SprintHoldDash: hold to hold Dash key (= sprinting in-game)
    DS3_KEY_ATTACK_L      = 7,  // InGameKey: Attack (left hand) in-game key
    DS3_KEY_ATTACK_L_ALT  = 8,  // SimulateKey: custom key that mirrors Attack (left hand)
    DS3_KEY_MENU          = 9,  // InGameKey: Menu in-game key
    DS3_KEY_MENU_ALT      = 10, // SimulateKey: custom key that mirrors Menu
    DS3_BINDING_COUNT     = 11
};

static GameProfile g_DarkSoulsIIIProfile = {
    /* id            */ L"DarkSoulsIII",
    /* displayName   */ L"DARK SOULS\u2122 III",
    /* iniSection    */ L"DarkSoulsIII",
    /* processName1  */ L"DarkSoulsIII.exe",
    /* processName2  */ nullptr,
    /* tipActive     */ L"CustomControlZ - Dark Souls III: ACTIVE",
    /* tipIdle       */ L"CustomControlZ: Waiting...",
    /* settingsTitle */ L"DARK SOULS\u2122 III - Key Bindings",
    /* posterFile    */ L"assets\\darksoulsiii.png",
    /* theme */ {
        RGB(12, 11, 10),    // bg              -- near-black (settings window)
        RGB(225, 220, 210), // text            -- warm off-white
        RGB(200, 190, 165), // accent          -- muted stone-gold
        RGB(52, 49, 44),    // button          -- dark stone
        RGB(10, 10, 10),    // selectBg        -- near-black (game-select card)
        RGB(85, 80, 70),    // border          -- mid stone-gray
        RGB(24, 22, 20),    // rowBg           -- slightly lighter than bg
        RGB(105, 98, 85),   // separator       -- stone line
        RGB(120, 35, 35),   // exitFill        -- dark crimson
        RGB(155, 55, 55),   // exitBorder
        RGB(240, 225, 225), // exitText
        RGB(80, 76, 68),    // minimizeBorder
    },
    /* bindingCount */ DS3_BINDING_COUNT,
    /* bindings */ {
        // DS3_KEY_WALK: WalkModifier -- injected alongside each direction key when Always Walk is on
        { L"WalkKey", L"Walk",
          VK_LSHIFT, VK_LSHIFT,
          { .type = BehaviorType::WalkModifier, .checkboxLabel = L"Always Walk" },
          /*isAppOnly=*/false },

        // DS3_KEY_RUN_FWD/BACK/LEFT/RIGHT: WalkRunSwap -- direction alone = walk (if Always Walk on); direction + Sprint = run
        { L"RunFwdKey",   L"Run (forward)",
          'W', 'W',
          { BehaviorType::WalkRunSwap },
          /*isAppOnly=*/false },

        { L"RunBackKey",  L"Run (backward)",
          'S', 'S',
          { BehaviorType::WalkRunSwap },
          /*isAppOnly=*/false },

        { L"RunLeftKey",  L"Run (left)",
          'A', 'A',
          { BehaviorType::WalkRunSwap },
          /*isAppOnly=*/false },

        { L"RunRightKey", L"Run (right)",
          'D', 'D',
          { BehaviorType::WalkRunSwap },
          /*isAppOnly=*/false },

        // DS3_KEY_DASH: DashKey -- in-game Dodge/Backstep/Roll; also held by Sprint
        { L"DashKey", L"Dash/Backstep/Roll",
          'C', 'C',
          { BehaviorType::DashKey },
          /*isAppOnly=*/false, /*separatorAbove=*/true },

        // DS3_KEY_SPRINT: SprintHoldDash -- hold to hold Dash key (sprinting in-game)
        { L"SprintKey", L"Custom Key: Sprint",
          VK_CAPITAL, VK_CAPITAL,
          { .type = BehaviorType::SprintHoldDash },
          /*isAppOnly=*/true },

        // DS3_KEY_ATTACK_L: InGameKey -- in-game Attack (left hand)
        { L"AttackLKey", L"Attack (left hand)",
          VK_XBUTTON1, VK_XBUTTON1,
          { BehaviorType::InGameKey },
          /*isAppOnly=*/false, /*separatorAbove=*/true },

        // DS3_KEY_ATTACK_L_ALT: SimulateKey -- custom key that mirrors Attack (left hand)
        { L"AttackLAltKey", L"Custom Key: Attack (left hand) Alternative",
          VK_RBUTTON, VK_RBUTTON,
          { .type = BehaviorType::SimulateKey },
          /*isAppOnly=*/true },

        // DS3_KEY_MENU: InGameKey -- in-game Menu key
        { L"MenuKey", L"Menu",
          VK_ESCAPE, VK_ESCAPE,
          { BehaviorType::InGameKey },
          /*isAppOnly=*/false, /*separatorAbove=*/true },

        // DS3_KEY_MENU_ALT: SimulateKey -- custom key that mirrors Menu
        { L"MenuAltKey", L"Custom Key: Menu Alternative",
          VK_XBUTTON2, VK_XBUTTON2,
          { .type = BehaviorType::SimulateKey },
          /*isAppOnly=*/true },
    },
    /* logicFn */ GenericLogicThreadFn,
};
