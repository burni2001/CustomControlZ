// Objective-C++ implementation of the Platform interface for macOS.
// Requires Accessibility permission: System Settings → Privacy & Security → Accessibility.

#include "platform.h"
#include "win_types_compat.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include <array>
#include <atomic>
#include <functional>
#include <string>
#include <mutex>
#include <cstring>
#include <cwchar>

// ---------------------------------------------------------------------------
// Globals declared in platform.h / win_types_compat.h
// ---------------------------------------------------------------------------
SendInputFnPtr g_sendInputImpl = nullptr;
Platform*      g_platform      = nullptr;

// ---------------------------------------------------------------------------
// VK ↔ CGKeyCode lookup tables
// 256-entry arrays; 0xFF = no mapping
// ---------------------------------------------------------------------------
static constexpr uint8_t kNone = 0xFF;

// Windows VK code → macOS CGKeyCode
// Initialised with designated initialiser syntax so all gaps default to kNone.
static uint8_t vkToCGKey[256];
static int     cgKeyToVk[256];  // reverse map

static void initKeyMaps() {
    memset(vkToCGKey, kNone, sizeof(vkToCGKey));
    memset(cgKeyToVk, -1,    sizeof(cgKeyToVk));

    // Helper lambda
    auto add = [](int vk, uint8_t cg) {
        vkToCGKey[vk] = cg;
        cgKeyToVk[cg] = vk;
    };

    // Control keys
    add(0x08, 0x33); // VK_BACK      → kVK_Delete
    add(0x09, 0x30); // VK_TAB       → kVK_Tab
    add(0x0D, 0x24); // VK_RETURN    → kVK_Return
    add(0x10, 0x38); // VK_SHIFT     → kVK_Shift
    add(0x11, 0x3B); // VK_CONTROL   → kVK_Control
    add(0x12, 0x3A); // VK_MENU/Alt  → kVK_Option
    add(0x14, 0x39); // VK_CAPITAL   → kVK_CapsLock
    add(0x1B, 0x35); // VK_ESCAPE    → kVK_Escape
    add(0x20, 0x31); // VK_SPACE     → kVK_Space
    // Navigation
    add(0x21, 0x74); // VK_PRIOR PgUp
    add(0x22, 0x79); // VK_NEXT  PgDn
    add(0x23, 0x77); // VK_END
    add(0x24, 0x73); // VK_HOME
    add(0x25, 0x7B); // VK_LEFT
    add(0x26, 0x7E); // VK_UP
    add(0x27, 0x7C); // VK_RIGHT
    add(0x28, 0x7D); // VK_DOWN
    add(0x2E, 0x75); // VK_DELETE → kVK_ForwardDelete
    // Digits
    add('0', 0x1D); add('1', 0x12); add('2', 0x13);
    add('3', 0x14); add('4', 0x15); add('5', 0x17);
    add('6', 0x16); add('7', 0x1A); add('8', 0x1C);
    add('9', 0x19);
    // Letters A-Z
    add('A', 0x00); add('B', 0x0B); add('C', 0x08);
    add('D', 0x02); add('E', 0x0E); add('F', 0x03);
    add('G', 0x05); add('H', 0x04); add('I', 0x22);
    add('J', 0x26); add('K', 0x28); add('L', 0x25);
    add('M', 0x2E); add('N', 0x2D); add('O', 0x1F);
    add('P', 0x23); add('Q', 0x0C); add('R', 0x0F);
    add('S', 0x01); add('T', 0x11); add('U', 0x20);
    add('V', 0x09); add('W', 0x0D); add('X', 0x07);
    add('Y', 0x10); add('Z', 0x06);
    // Function keys
    add(0x70, 0x7A); // F1
    add(0x71, 0x78); // F2
    add(0x72, 0x63); // F3
    add(0x73, 0x76); // F4
    add(0x74, 0x60); // F5
    add(0x75, 0x61); // F6
    add(0x76, 0x62); // F7
    add(0x77, 0x64); // F8
    add(0x78, 0x65); // F9
    add(0x79, 0x6D); // F10
    add(0x7A, 0x67); // F11
    add(0x7B, 0x6F); // F12
    // Modifier alternates
    add(0xA0, 0x38); // VK_LSHIFT
    add(0xA1, 0x3C); // VK_RSHIFT
    add(0xA2, 0x3B); // VK_LCONTROL
    add(0xA3, 0x3E); // VK_RCONTROL
    add(0xA4, 0x3A); // VK_LMENU
    add(0xA5, 0x3D); // VK_RMENU
    // OEM punctuation
    add(0xBA, 0x29); // ;:
    add(0xBB, 0x18); // =+
    add(0xBC, 0x2B); // ,<
    add(0xBD, 0x1B); // -_
    add(0xBE, 0x2F); // .>
    add(0xBF, 0x2C); // /?
    add(0xC0, 0x32); // `~
    add(0xDB, 0x21); // [{
    add(0xDC, 0x2A); // \|
    add(0xDD, 0x1E); // ]}
    add(0xDE, 0x27); // '"
}

// ---------------------------------------------------------------------------
// MacPlatform
// ---------------------------------------------------------------------------
class MacPlatform : public Platform {
public:
    MacPlatform();
    ~MacPlatform() override;

    void startKeyboardHook(std::function<bool(MacKeyEvent)> cb) override;
    void stopKeyboardHook() override;

    void injectKey(int vk, bool down) override;
    void injectMouseButton(int vk, bool down) override;
    void injectMouseWheel(int delta) override;

    bool isKeyDown(int vk) const override;
    bool isKeyPhysicallyDown(int vk) const override;

    bool isProcessRunning(const wchar_t* processName) const override;
    bool isAppInForeground(const wchar_t* processName) const override;

    std::string configFilePath() const override;

    void notifyTrayState(bool active) override;
    void showWarning(const wchar_t* title, const wchar_t* text) override;

    // Called from the CGEventTap callback
    CGEventRef handleEvent(CGEventType type, CGEventRef event);

    // Called from win_types_compat.h SendInput stub
    static UINT sendInputImpl(UINT n, INPUT* inputs, int cbSize);

    // Set from TrayIcon to receive state change notifications
    std::function<void(bool)> onTrayStateChange;

private:
    std::function<bool(MacKeyEvent)> m_hookCallback;
    CFMachPortRef      m_eventTap    = nullptr;
    CFRunLoopSourceRef m_runLoopSrc  = nullptr;

    // Physical keyboard state indexed by CGKeyCode (0-255)
    mutable std::mutex m_stateMutex;
    bool m_physDown[256] = {};  // set by event tap from hardware events

    static CGEventRef eventTapCB(CGEventTapProxy, CGEventType, CGEventRef, void*);

    static NSString* processToMatch(const wchar_t* processName);
};

// ---------------------------------------------------------------------------
// Event tap callback
// We use kCGHIDEventTap which only sees physical hardware events — our own
// CGEventPost injections are not captured here, so we need no injection marker.
// ---------------------------------------------------------------------------
CGEventRef MacPlatform::eventTapCB(CGEventTapProxy, CGEventType type,
                                    CGEventRef event, void* refcon) {
    return static_cast<MacPlatform*>(refcon)->handleEvent(type, event);
}

CGEventRef MacPlatform::handleEvent(CGEventType type, CGEventRef event) {
    // Re-enable tap if it gets disabled by the system
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        if (m_eventTap) CGEventTapEnable(m_eventTap, true);
        return event;
    }
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) return event;

    CGKeyCode cgKey = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
    bool      down  = (type == kCGEventKeyDown);

    if (cgKey < 256) {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        m_physDown[cgKey] = down;
    }

    if (!m_hookCallback) return event;

    int vk = (cgKey < 256) ? cgKeyToVk[cgKey] : -1;
    if (vk < 0) return event;   // unmapped key — pass through

    MacKeyEvent ev{ vk, down, false };
    return m_hookCallback(ev) ? nullptr : event;  // nullptr = suppress
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------
MacPlatform::MacPlatform() {
    initKeyMaps();
    memset(m_physDown, 0, sizeof(m_physDown));

    // Hook up the win_types_compat.h SendInput stub so wheel/keyboard injection
    // from GenericLogicThreadFn reaches us.
    g_sendInputImpl = [](UINT n, INPUT* inputs, int sz) -> UINT {
        return MacPlatform::sendInputImpl(n, inputs, sz);
    };
}

MacPlatform::~MacPlatform() {
    stopKeyboardHook();
}

// ---------------------------------------------------------------------------
// Keyboard hook (CGEventTap at HID level — physical keys only)
// ---------------------------------------------------------------------------
void MacPlatform::startKeyboardHook(std::function<bool(MacKeyEvent)> cb) {
    m_hookCallback = std::move(cb);

    if (!AXIsProcessTrusted()) {
        NSDictionary* opts = @{ (__bridge NSString*)kAXTrustedCheckOptionPrompt : @YES };
        AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)opts);
        NSLog(@"[CCZ] Accessibility permission not granted — keyboard hook disabled. "
              @"Grant permission in System Settings → Privacy & Security → Accessibility, "
              @"then relaunch the app.");
        return;
    }

    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);

    // kCGHIDEventTap: captures physical hardware events before software injection
    m_eventTap = CGEventTapCreate(
        kCGHIDEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,   // can suppress events
        mask,
        &MacPlatform::eventTapCB,
        this);

    if (!m_eventTap) {
        NSLog(@"[CCZ] Failed to create CGEventTap — check Accessibility permission");
        return;
    }

    m_runLoopSrc = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), m_runLoopSrc, kCFRunLoopCommonModes);
    CGEventTapEnable(m_eventTap, true);
    NSLog(@"[CCZ] Keyboard hook installed");
}

void MacPlatform::stopKeyboardHook() {
    if (m_eventTap) {
        CGEventTapEnable(m_eventTap, false);
        if (m_runLoopSrc) {
            CFRunLoopRemoveSource(CFRunLoopGetMain(), m_runLoopSrc, kCFRunLoopCommonModes);
            CFRelease(m_runLoopSrc);
            m_runLoopSrc = nullptr;
        }
        CFRelease(m_eventTap);
        m_eventTap = nullptr;
    }
    m_hookCallback = nullptr;
}

// ---------------------------------------------------------------------------
// Key injection
// ---------------------------------------------------------------------------
void MacPlatform::injectKey(int vk, bool down) {
    if (vk <= 0 || vk >= 256) return;
    uint8_t cgKey = vkToCGKey[vk];
    if (cgKey == kNone) return;

    // Use kCGEventSourceStateHIDSystemState so injected events appear to apps
    // as regular keystrokes.
    CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    CGEventRef ev = CGEventCreateKeyboardEvent(src, (CGKeyCode)cgKey, (Boolean)down);
    // Post to the annotated session tap — bypasses our kCGHIDEventTap listener,
    // so injected events don't feed back into IsKeyDown as physical presses.
    CGEventPost(kCGAnnotatedSessionEventTap, ev);
    CFRelease(ev);
    if (src) CFRelease(src);
}

void MacPlatform::injectMouseButton(int vk, bool down) {
    CGMouseButton btn;
    CGEventType   typeDown, typeUp;
    switch (vk) {
        case 0x01: btn = kCGMouseButtonLeft;   typeDown = kCGEventLeftMouseDown;   typeUp = kCGEventLeftMouseUp;   break;
        case 0x02: btn = kCGMouseButtonRight;  typeDown = kCGEventRightMouseDown;  typeUp = kCGEventRightMouseUp;  break;
        case 0x04: btn = kCGMouseButtonCenter; typeDown = kCGEventOtherMouseDown;  typeUp = kCGEventOtherMouseUp;  break;
        case 0x05: btn = (CGMouseButton)3;     typeDown = kCGEventOtherMouseDown;  typeUp = kCGEventOtherMouseUp;  break;
        case 0x06: btn = (CGMouseButton)4;     typeDown = kCGEventOtherMouseDown;  typeUp = kCGEventOtherMouseUp;  break;
        default: return;
    }

    CGEventRef pos = CGEventCreate(nullptr);
    CGPoint    loc = pos ? CGEventGetLocation(pos) : CGPointMake(0, 0);
    if (pos) CFRelease(pos);

    CGEventRef ev = CGEventCreateMouseEvent(nullptr, down ? typeDown : typeUp, loc, btn);
    if (vk >= 0x05) CGEventSetIntegerValueField(ev, kCGMouseEventButtonNumber, (int64_t)btn);
    CGEventPost(kCGAnnotatedSessionEventTap, ev);
    CFRelease(ev);
}

void MacPlatform::injectMouseWheel(int delta) {
    // delta: Windows convention — positive = scroll up (wheel forward)
    // CGScrollEventUnit: kCGScrollEventUnitLine gives consistent speed
    int32_t lines = delta / 120;
    if (lines == 0) lines = (delta > 0) ? 1 : -1;
    CGEventRef ev = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 1, lines);
    CGEventPost(kCGAnnotatedSessionEventTap, ev);
    CFRelease(ev);
}

// ---------------------------------------------------------------------------
// Key state queries
// ---------------------------------------------------------------------------
bool MacPlatform::isKeyDown(int vk) const {
    return isKeyPhysicallyDown(vk);  // for macOS: no distinction needed (we don't track injected state separately)
}

bool MacPlatform::isKeyPhysicallyDown(int vk) const {
    if (vk <= 0 || vk >= 256) return false;
    // Mouse buttons via NSEvent
    switch (vk) {
        case 0x01: return ([NSEvent pressedMouseButtons] & (1 << 0)) != 0;
        case 0x02: return ([NSEvent pressedMouseButtons] & (1 << 1)) != 0;
        case 0x04: return ([NSEvent pressedMouseButtons] & (1 << 2)) != 0;
        case 0x05: return ([NSEvent pressedMouseButtons] & (1 << 3)) != 0;
        case 0x06: return ([NSEvent pressedMouseButtons] & (1 << 4)) != 0;
    }
    uint8_t cgKey = vkToCGKey[vk];
    if (cgKey == kNone) return false;
    std::lock_guard<std::mutex> lk(m_stateMutex);
    return m_physDown[cgKey];
}

// ---------------------------------------------------------------------------
// Process / foreground detection
// ---------------------------------------------------------------------------
NSString* MacPlatform::processToMatch(const wchar_t* processName) {
    if (!processName || processName[0] == L'\0') return nil;
    size_t len = wcslen(processName);
    NSString* s = [[NSString alloc] initWithBytes:processName
                                           length:len * sizeof(wchar_t)
                                         encoding:NSUTF32LittleEndianStringEncoding];
    // Strip ".exe" (Windows games run via Crossover/Parallels keep the .exe name)
    if ([s.lowercaseString hasSuffix:@".exe"])
        s = [s substringToIndex:s.length - 4];
    return s.lowercaseString;
}

bool MacPlatform::isProcessRunning(const wchar_t* processName) const {
    NSString* target = processToMatch(processName);
    if (!target) return false;
    for (NSRunningApplication* app in [NSWorkspace sharedWorkspace].runningApplications) {
        if ([app.localizedName.lowercaseString isEqualToString:target]) return true;
        if ([app.bundleIdentifier.lowercaseString containsString:target])  return true;
        if ([app.executableURL.lastPathComponent.lowercaseString isEqualToString:target]) return true;
    }
    return false;
}

bool MacPlatform::isAppInForeground(const wchar_t* processName) const {
    NSString* target = processToMatch(processName);
    if (!target) return false;
    NSRunningApplication* front = [NSWorkspace sharedWorkspace].frontmostApplication;
    if (!front) return false;
    return [front.localizedName.lowercaseString isEqualToString:target] ||
           [front.bundleIdentifier.lowercaseString containsString:target]  ||
           [front.executableURL.lastPathComponent.lowercaseString isEqualToString:target];
}

// ---------------------------------------------------------------------------
// Config path  (~/.config equivalent on macOS)
// ---------------------------------------------------------------------------
std::string MacPlatform::configFilePath() const {
    NSString* support = NSSearchPathForDirectoriesInDomains(
                            NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject;
    NSString* dir  = [support stringByAppendingPathComponent:@"CustomControlZ"];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir
                              withIntermediateDirectories:YES attributes:nil error:nil];
    return [[dir stringByAppendingPathComponent:@"settings.ini"] UTF8String];
}

// ---------------------------------------------------------------------------
// Tray state  (dispatched to main thread so Qt can update the icon safely)
// ---------------------------------------------------------------------------
void MacPlatform::notifyTrayState(bool active) {
    if (!onTrayStateChange) return;
    auto cb = onTrayStateChange;
    dispatch_async(dispatch_get_main_queue(), ^{ cb(active); });
}

void MacPlatform::showWarning(const wchar_t* title, const wchar_t* text) {
    if (!title || !text) return;
    NSString* t = [[NSString alloc] initWithBytes:title length:wcslen(title)*sizeof(wchar_t)
                                         encoding:NSUTF32LittleEndianStringEncoding];
    NSString* m = [[NSString alloc] initWithBytes:text  length:wcslen(text) *sizeof(wchar_t)
                                         encoding:NSUTF32LittleEndianStringEncoding];
    dispatch_async(dispatch_get_main_queue(), ^{
        NSAlert* alert   = [[NSAlert alloc] init];
        alert.alertStyle = NSAlertStyleWarning;
        alert.messageText    = t;
        alert.informativeText = m;
        [alert runModal];
    });
}

// ---------------------------------------------------------------------------
// SendInput bridge (called via g_sendInputImpl from win_types_compat.h)
// ---------------------------------------------------------------------------
UINT MacPlatform::sendInputImpl(UINT n, INPUT* inputs, int /*cbSize*/) {
    auto* self = static_cast<MacPlatform*>(g_platform);
    if (!self) return 0;
    for (UINT i = 0; i < n; i++) {
        if (inputs[i].type == INPUT_MOUSE) {
            if (inputs[i].mi.dwFlags & MOUSEEVENTF_WHEEL)
                self->injectMouseWheel(static_cast<int>(static_cast<LONG>(inputs[i].mi.mouseData)));
        } else if (inputs[i].type == INPUT_KEYBOARD) {
            bool up = (inputs[i].ki.dwFlags & KEYEVENTF_KEYUP) != 0;
            self->injectKey(inputs[i].ki.wVk, !up);
        }
    }
    return n;
}

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
Platform* createPlatform() {
    auto* p = new MacPlatform();
    g_platform = p;
    return p;
}
