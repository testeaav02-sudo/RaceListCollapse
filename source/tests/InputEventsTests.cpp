#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../InputEvents.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

using InputEvents::Buffer;
using InputEvents::Action;

static SHORT keyStates[256] = { 0 };
static SHORT WINAPI readKeyState(int key) { return keyStates[key]; }

int main() {
    Buffer input;
    assert(input.configure(VK_F8, VK_PRIOR, VK_NEXT));
    assert(input.scanCode(InputEvents::Toggle) == 0x42);
    assert(input.scanCode(InputEvents::PreviousPage) == 0xC9);
    assert(input.scanCode(InputEvents::NextPage) == 0xD1);
    assert(!input.configure(0, VK_PRIOR, VK_NEXT));
    assert(InputEvents::virtualKeyToScan(VK_NUMLOCK) == 0x45);
    assert(InputEvents::virtualKeyToScan(VK_PAUSE) == 0xC5);
    int tooltip, caller, otherTooltip, otherCaller;
    Action action;

    // Even down/up entirely between two UI frames produces one command.
    input.onKeyDown(0x42, true, false, &tooltip, &caller);
    input.onKeyUp(0x42);
    assert(input.pop(&tooltip, &caller, true, action) && action == InputEvents::Toggle);
    assert(!input.pop(&tooltip, &caller, true, action));

    // Holding a key / repeated native key-down never repeats the toggle.
    input.onKeyDown(0x42, true, false, &tooltip, &caller);
    input.onKeyDown(0x42, true, false, &tooltip, &caller);
    assert(input.pendingCount() == 1);
    assert(input.pop(&tooltip, &caller, true, action));
    input.onKeyDown(0x42, true, false, &tooltip, &caller);
    assert(!input.pop(&tooltip, &caller, true, action));
    input.onKeyUp(0x42);

    // Two actual taps within one frame remain two commands, unlike a bool flag.
    input.onKeyDown(0x42, true, false, &tooltip, &caller);
    input.onKeyUp(0x42);
    input.onKeyDown(0x42, true, false, &tooltip, &caller);
    input.onKeyUp(0x42);
    assert(input.pop(&tooltip, &caller, true, action));
    assert(input.pop(&tooltip, &caller, true, action));
    assert(!input.pop(&tooltip, &caller, true, action));

    // A press that started hidden/with a modifier stays rejected until release.
    input.onKeyDown(0x42, false, false, &tooltip, &caller);
    input.onKeyDown(0x42, true, false, &tooltip, &caller);
    assert(input.pendingCount() == 0);
    input.onKeyUp(0x42);
    input.onKeyDown(0x42, true, true, &tooltip, &caller);
    input.onKeyDown(0x42, true, false, &tooltip, &caller);
    assert(input.pendingCount() == 0);
    input.onKeyUp(0x42);

    // Commands are not replayed on another item or after foreground loss.
    input.onKeyDown(0xC9, true, false, &tooltip, &caller);
    input.onKeyUp(0xC9);
    assert(!input.pop(&tooltip, &otherCaller, true, action));
    input.onKeyDown(0xD1, true, false, &tooltip, &caller);
    input.onKeyUp(0xD1);
    assert(!input.pop(&otherTooltip, &caller, true, action));
    input.onKeyDown(0x42, true, false, &tooltip, &caller);
    input.onKeyUp(0x42);
    assert(!input.pop(&tooltip, &caller, false, action));

    // Mouse events, null targets, and unbound keys do not enter the queue.
    input.onKeyDown(0x1000, true, false, &tooltip, &caller);
    input.onKeyDown(0x42, true, false, NULL, &caller);
    input.onKeyUp(0x42);
    input.onKeyDown(0x42, true, false, &tooltip, NULL);
    input.onKeyUp(0x42);
    input.onKeyDown(0x41, true, false, &tooltip, &caller);
    input.onKeyUp(0x41);
    assert(input.pendingCount() == 0);

    // The callback queue is bounded even when no UI frame can drain it.
    for (int i = 0; i < 100; ++i) {
        input.onKeyDown(0x42, true, false, &tooltip, &caller);
        input.onKeyUp(0x42);
    }
    assert(input.pendingCount() == 32);
    input.clearPending();
    assert(input.pendingCount() == 0);

    // All four arrows remain available exclusively to Kenshi by default,
    // including repeated presses while a supported tooltip is visible.
    const unsigned arrows[] = { 0xCB, 0xCD, 0xC8, 0xD0 };
    for (unsigned i = 0; i < 4; ++i) {
        input.onKeyDown(arrows[i], true, false, &tooltip, &caller);
        input.onKeyDown(arrows[i], true, false, &tooltip, &caller);
        input.onKeyUp(arrows[i]);
        assert(!input.pop(&tooltip, &caller, true, action));
    }
    const unsigned pageKeys[] = { 0xC9, 0xD1 };
    const Action pageActions[] = { InputEvents::PreviousPage, InputEvents::NextPage };
    for (unsigned i = 0; i < 2; ++i) {
        input.onKeyDown(pageKeys[i], true, false, &tooltip, &caller);
        input.onKeyUp(pageKeys[i]);
        assert(input.pop(&tooltip, &caller, true, action) && action == pageActions[i]);
        assert(!input.pop(&tooltip, &caller, true, action));
    }

    // Page repeats stay latched until release. Separate taps can be queued in
    // one frame, and holding PageUp does not prevent a PageDown tap.
    input.onKeyDown(0xC9, true, false, &tooltip, &caller);
    input.onKeyDown(0xC9, true, false, &tooltip, &caller);
    input.onKeyDown(0xD1, true, false, &tooltip, &caller);
    input.onKeyUp(0xD1);
    input.onKeyDown(0xD1, true, false, &tooltip, &caller);
    input.onKeyUp(0xD1);
    assert(input.pendingCount() == 3);
    assert(input.pop(&tooltip, &caller, true, action) && action == InputEvents::PreviousPage);
    assert(input.pop(&tooltip, &caller, true, action) && action == InputEvents::NextPage);
    assert(input.pop(&tooltip, &caller, true, action) && action == InputEvents::NextPage);
    input.onKeyDown(0xC9, true, false, &tooltip, &caller);
    assert(!input.pop(&tooltip, &caller, true, action));
    input.onKeyUp(0xC9);
    input.onKeyDown(0xC9, true, false, &tooltip, &caller);
    input.onKeyUp(0xC9);
    assert(input.pop(&tooltip, &caller, true, action) && action == InputEvents::PreviousPage);

    // Modifier/hidden presses must not become page commands when a modifier is
    // released or a tooltip appears before the page key itself is released.
    for (unsigned i = 0; i < 2; ++i) {
        input.onKeyDown(pageKeys[i], true, true, &tooltip, &caller);
        input.onKeyDown(pageKeys[i], true, false, &tooltip, &caller);
        assert(input.pendingCount() == 0);
        input.onKeyUp(pageKeys[i]);
        input.onKeyDown(pageKeys[i], false, false, &tooltip, &caller);
        input.onKeyDown(pageKeys[i], true, false, &tooltip, &caller);
        assert(input.pendingCount() == 0);
        input.onKeyUp(pageKeys[i]);
        input.onKeyDown(pageKeys[i], true, false, &tooltip, &caller);
        input.onKeyUp(pageKeys[i]);
        assert(!input.pop(&tooltip, &caller, false, action));
        input.onKeyDown(pageKeys[i], true, false, &tooltip, &caller);
        input.onKeyUp(pageKeys[i]);
        assert(!input.pop(&tooltip, &otherCaller, true, action));
    }

    // A lost key-up after switching apps is recovered for both page keys. A held
    // high bit cannot release the latch; a low bit alone is treated as released
    // and never manufactures a command from polling.
    for (unsigned i = 0; i < 2; ++i) {
        const int virtualKey = i == 0 ? VK_PRIOR : VK_NEXT;
        keyStates[virtualKey] = -32767; // SHORT bit pattern 0x8001
        input.onKeyDown(pageKeys[i], true, false, &tooltip, &caller);
        assert(input.pop(&tooltip, &caller, true, action) && action == pageActions[i]);
        input.syncReleased(readKeyState);
        input.onKeyDown(pageKeys[i], true, false, &tooltip, &caller);
        assert(input.pendingCount() == 0);
        input.clearPending();
        keyStates[virtualKey] = 1;
        input.syncReleased(readKeyState);
        assert(input.pendingCount() == 0);
        input.onKeyDown(pageKeys[i], true, false, &tooltip, &caller);
        input.onKeyUp(pageKeys[i]);
        assert(input.pop(&tooltip, &caller, true, action) && action == pageActions[i]);
        keyStates[virtualKey] = 0;
    }

    // Directional arrows cannot be assigned to ANY action, even explicitly.
    // The caller can fall back to defaults, which still ignore all four arrows.
    const int arrowVirtualKeys[] = { VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN };
    for (unsigned i = 0; i < 4; ++i) {
        assert(InputEvents::virtualKeyToScan(arrowVirtualKeys[i]) == 0);
        assert(!input.configure(arrowVirtualKeys[i], VK_PRIOR, VK_NEXT));
        assert(!input.configure(VK_F8, arrowVirtualKeys[i], VK_NEXT));
        assert(!input.configure(VK_F8, VK_PRIOR, arrowVirtualKeys[i]));
        assert(input.configure(VK_F8, VK_PRIOR, VK_NEXT));
        input.onKeyDown(arrows[i], true, false, &tooltip, &caller);
        input.onKeyUp(arrows[i]);
        assert(!input.pop(&tooltip, &caller, true, action));
    }

    // Other custom bindings remain available. Duplicate configured keys emit
    // one command in Toggle/Previous/Next order.
    assert(input.configure(VK_F8, VK_NEXT, VK_NEXT));
    input.onKeyDown(0xD1, true, false, &tooltip, &caller);
    input.onKeyUp(0xD1);
    assert(input.pop(&tooltip, &caller, true, action) && action == InputEvents::PreviousPage);
    assert(!input.pop(&tooltip, &caller, true, action));
    assert(input.configure(VK_NEXT, VK_NEXT, VK_NEXT));
    input.onKeyDown(0xD1, true, false, &tooltip, &caller);
    input.onKeyUp(0xD1);
    assert(input.pop(&tooltip, &caller, true, action) && action == InputEvents::Toggle);
    assert(!input.pop(&tooltip, &caller, true, action));

    // Reconfiguration clears held keys and queued old actions. Custom ordinary
    // keys do not enable arrows; an invalid configuration preserves prior state.
    input.onKeyDown(0xD1, true, false, &tooltip, &caller);
    assert(input.configure(VK_F8, 'A', 'D'));
    assert(input.pendingCount() == 0);
    for (unsigned i = 0; i < 4; ++i) {
        input.onKeyDown(arrows[i], true, false, &tooltip, &caller);
        input.onKeyUp(arrows[i]);
        assert(!input.pop(&tooltip, &caller, true, action));
    }
    for (unsigned i = 0; i < 2; ++i) {
        const unsigned code = input.scanCode(pageActions[i]);
        input.onKeyDown(code, true, false, &tooltip, &caller);
        input.onKeyUp(code);
        assert(input.pop(&tooltip, &caller, true, action) && action == pageActions[i]);
        input.onKeyDown(pageKeys[i], true, false, &tooltip, &caller);
        input.onKeyUp(pageKeys[i]);
        assert(!input.pop(&tooltip, &caller, true, action));
    }
    const unsigned customNext = input.scanCode(InputEvents::NextPage);
    input.onKeyDown(customNext, true, false, &tooltip, &caller);
    assert(!input.configure(0, VK_PRIOR, VK_NEXT));
    input.onKeyDown(customNext, true, false, &tooltip, &caller);
    assert(input.pendingCount() == 1);
    input.onKeyUp(customNext);
    assert(input.pop(&tooltip, &caller, true, action) && action == InputEvents::NextPage);
    assert(!input.pop(&tooltip, &caller, true, action));

    std::cout << "InputEvents: 15 scenario groups passed.\n";
    return 0;
}
