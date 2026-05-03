#pragma once
#include "../GameProfiles.h"

enum DarkSoulsIIIBinding {
    DS3_KEY_WALK      = 0,  // WalkModifier: in-game walk key (injected alongside directions)
    DS3_KEY_RUN_FWD   = 1,  // WalkRunSwap: forward direction
    DS3_KEY_RUN_BACK  = 2,  // WalkRunSwap: backward direction
    DS3_KEY_RUN_LEFT  = 3,  // WalkRunSwap: left direction
    DS3_KEY_RUN_RIGHT = 4,  // WalkRunSwap: right direction
    DS3_KEY_SPRINT    = 5,  // SprintKey: hold to run instead of walk
    DS3_BINDING_COUNT = 6
};

static GameProfile g_DarkSoulsIIIProfile = {
    /* id            */ L"DarkSoulsIII",
    /* displayName   */ L"DARK SOULS™ III",
    /* iniSection    */ L"DarkSoulsIII",
    /* processName1  */ L"DarkSoulsIII.exe",
    /* processName2  */ nullptr,
    /* tipActive     */ L"CustomControlZ - Dark Souls III: ACTIVE",
    /* tipIdle       */ L"CustomControlZ: Waiting...",
    /* settingsTitle */ L"DARK SOULS™ III - Key Bindings",
    /* posterFile    */ L"assets\\darksoulsiii.png",
    /* theme */ {
        RGB(30, 28, 26),    // bg              — dark warm stone (settings window)
        RGB(225, 220, 210), // text            — warm off-white
        RGB(200, 190, 165), // accent          — muted stone-gold
        RGB(52, 49, 44),    // button          — dark stone
        RGB(10, 10, 10),    // selectBg        — near-black (game-select card)
        RGB(85, 80, 70),    // border          — mid stone-gray
        RGB(40, 38, 34),    // rowBg           — slightly lighter dark stone
        RGB(105, 98, 85),   // separator       — stone line
        RGB(120, 35, 35),   // exitFill        — dark crimson
        RGB(155, 55, 55),   // exitBorder
        RGB(240, 225, 225), // exitText
        RGB(80, 76, 68),    // minimizeBorder
    },
    /* bindingCount */ DS3_BINDING_COUNT,
    /* bindings */ {
        // DS3_KEY_WALK: WalkModifier — configurable in-game walk key; injected by WalkRunSwap when no sprint held
        { L"WalkKey",      L"Walk",
          VK_LSHIFT, VK_LSHIFT,
          { BehaviorType::WalkModifier },
          /*isAppOnly=*/false },

        // DS3_KEY_RUN_FWD/BACK/LEFT/RIGHT: WalkRunSwap — direction alone = walk; direction + Sprint = run
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

        // DS3_KEY_SPRINT: SprintKey — hold while moving to run instead of walk
        { L"SprintKey",   L"Custom Key: Sprint",
          VK_CAPITAL, VK_CAPITAL,
          { BehaviorType::SprintKey },
          /*isAppOnly=*/true, /*separatorAbove=*/true },
    },
    /* logicFn */ GenericLogicThreadFn,
};
