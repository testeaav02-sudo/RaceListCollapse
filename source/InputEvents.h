// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RACE_LIST_INPUT_EVENTS_H
#define RACE_LIST_INPUT_EVENTS_H

#include <Windows.h>
#include <cstddef>

// The native InputHandler callbacks run on the UI input path. This helper only
// records presses; the inventory tooltip update performs the actual refresh.
// Call all methods on that same UI thread. Do not register a second OIS listener.
namespace InputEvents {

enum Action { Toggle = 0, PreviousPage = 1, NextPage = 2 };

// The INI uses Windows virtual-key codes; Kenshi's OIS callbacks use DIK scan
// codes. The E0 flag maps to the high bit in OIS (PgUp C9, PgDown D1).
inline unsigned virtualKeyToScan(int virtualKey) {
    if (virtualKey < 1 || virtualKey > 255) return 0;
    // Some keyboard layouts omit E0 in MapVirtualKeyEx for these VKs. Preserve
    // the OIS distinction between navigation keys and the numeric keypad.
    switch (virtualKey) {
        // Directional arrows belong to Kenshi's controls, including when an
        // old or manually edited INI attempts to assign one to this plugin.
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN: return 0;
        case VK_PAUSE: return 0xC5;
        case VK_NUMLOCK: return 0x45;
        case VK_HOME: return 0xC7;
        case VK_PRIOR: return 0xC9;
        case VK_END: return 0xCF;
        case VK_NEXT: return 0xD1;
        case VK_INSERT: return 0xD2;
        case VK_DELETE: return 0xD3;
        case VK_RCONTROL: return 0x9D;
        case VK_RMENU: return 0xB8;
        case VK_DIVIDE: return 0xB5;
        case VK_SNAPSHOT: return 0xB7;
        case VK_LWIN: return 0xDB;
        case VK_RWIN: return 0xDC;
        case VK_APPS: return 0xDD;
    }
    const UINT scan = MapVirtualKeyExA(static_cast<UINT>(virtualKey),
        MAPVK_VK_TO_VSC_EX, GetKeyboardLayout(0));
    if (!scan) return 0;
    const unsigned code = scan & 0xFF;
    if ((scan & 0xFF00) == 0xE000) return code | 0x80;
    if ((scan & 0xFF00) == 0xE100) return 0; // unsupported special key
    return code;
}

class Buffer {
    enum { BindingCount = 3, QueueCapacity = 32 };
    struct Event {
        Action action;
        void* tooltip;
        void* caller;
        Event() : action(Toggle), tooltip(NULL), caller(NULL) {}
    };
    bool held[256];
    int virtualKeys[BindingCount];
    unsigned scanCodes[BindingCount];
    Event events[QueueCapacity];
    std::size_t head;
    std::size_t count;

public:
    Buffer() : head(0), count(0) {
        for (unsigned i = 0; i < 256; ++i) held[i] = false;
        for (unsigned i = 0; i < BindingCount; ++i) { virtualKeys[i] = 0; scanCodes[i] = 0; }
    }

    bool configure(int toggle, int previous, int next) {
        // Use only the configured keys. Automatic arrow aliases interfere with
        // Kenshi's camera controls, so navigation has no additional bindings.
        const int requested[BindingCount] = { toggle, previous, next };
        unsigned mapped[BindingCount];
        for (unsigned i = 0; i < BindingCount; ++i) {
            mapped[i] = virtualKeyToScan(requested[i]);
            if (mapped[i] == 0 || mapped[i] >= 256) return false;
        }
        clearPending();
        for (unsigned i = 0; i < 256; ++i) held[i] = false;
        for (unsigned i = 0; i < BindingCount; ++i) {
            virtualKeys[i] = requested[i]; scanCodes[i] = mapped[i];
        }
        return true;
    }

    unsigned scanCode(Action action) const { return scanCodes[action]; }
    std::size_t pendingCount() const { return count; }

    void onKeyDown(unsigned code, bool eligibleNow, bool modifiersNow,
        void* primaryTooltip, void* caller) {
        // Kenshi reuses keyDownEvent for mouse buttons at codes >= 0x1000.
        if (code == 0 || code >= 256) return;
        const bool repeated = held[code];
        held[code] = true; // track even rejected/hidden presses until key-up
        if (repeated || !eligibleNow || modifiersNow || !primaryTooltip || !caller) return;
        for (unsigned i = 0; i < BindingCount; ++i) {
            if (code != scanCodes[i]) continue;
            if (count == QueueCapacity) return; // bounded; never allocate in input callback
            Event& event = events[(head + count) % QueueCapacity];
            event.action = static_cast<Action>(i);
            event.tooltip = primaryTooltip;
            event.caller = caller;
            ++count;
            return; // duplicate configured keys obey Toggle/Previous/Next priority
        }
    }

    void onKeyUp(unsigned code) {
        if (code < 256) held[code] = false;
    }

    // Only the primary tooltip should drain the queue. Comparison tooltips use
    // the shared render revision, so one physical press cannot toggle twice.
    bool pop(void* primaryTooltip, void* caller, bool eligibleNow, Action& action) {
        while (count) {
            const Event event = events[head];
            head = (head + 1) % QueueCapacity;
            --count;
            // Discard stale input after focus loss, hiding, or an item change.
            if (!eligibleNow || event.tooltip != primaryTooltip || event.caller != caller) continue;
            action = event.action;
            return true;
        }
        return false;
    }

    void clearPending() { head = 0; count = 0; }

    // A key-up may be routed to a text field or another app after focus changes.
    // High-bit polling only releases a latch; actual presses always come from
    // buffered callbacks. Never use GetAsyncKeyState's unreliable low bit.
    void syncReleased(SHORT (WINAPI* keyState)(int) = GetAsyncKeyState) {
        for (unsigned i = 0; i < BindingCount; ++i)
            if (scanCodes[i] && !(keyState(virtualKeys[i]) & 0x8000))
                held[scanCodes[i]] = false;
    }
};

} // namespace InputEvents
#endif
