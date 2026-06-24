#pragma once
// macOS shim: shadows the real <windows.h> when src/platform is first in the include path.
// BehaviorEngine.h and GameProfiles.h do #include <windows.h>; on macOS the compiler finds
// this file instead of the non-existent real one.
#include "win_types_compat.h"
