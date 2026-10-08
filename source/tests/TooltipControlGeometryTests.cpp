#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include "../TooltipControlGeometry.h"

int main() {
    using namespace TooltipControls;
    Rect item(100, 100, 40, 40), panel(160, 60, 300, 180);
    assert(containsPoint(item, 100, 100));
    assert(!containsPoint(item, 140, 100));
    assert(!containsPoint(Rect(), 0, 0, 10));
    assert(containsPoint(panel, 158, 70, 3));
    assert(inBridge(item, panel, 150, 120));
    assert(inBridge(item, panel, 150, 175));
    assert(!inBridge(item, panel, 150, 230));
    assert(inBridge(panel, item, 150, 140));
    assert(inBridge(item, Rect(110, 170, 200, 180), 120, 155));
    assert(!inBridge(item, Rect(800, 100, 100, 100), 500, 120));
    assert(!inBridge(Rect(), panel, 150, 120));
    Rect lowerPanel(160, 60, 300, 600);
    assert(inBridge(item, lowerPanel, 150, 350));
    assert(!inBridge(item, lowerPanel, 150, 600));
    assert(inBridge(lowerPanel, item, 150, 350));
    assert(inBridge(Rect(160, 200, 40, 40), Rect(50, 50, 100, 100), 155, 175));
    assert(inBridge(item, item, 120, 120));
    assert(!inBridge(item, item, 160, 160));

    // Real, slow trajectories through a wide inventory, including long pauses
    // on empty cells. This deliberately differs from a teleported cursor.
    Rect origin(100, 100, 32, 64), target(700, 40, 210, 420);
    HoverTransition travel;
    travel.reset(origin);
    assert(!travel.hold(target, 120, 120, 10, false, Rect()));
    assert(inBridge(origin, target, 500, 150, 0.0));
    for (int x = 140; x < 700; x += 40)
        assert(travel.hold(target, x, 150, static_cast<unsigned long>(x * 100), false, Rect()));
    assert(travel.hold(target, 500, 150, 200000, false, Rect()));
    assert(travel.hold(target, 800, 420, 250000, false, Rect()));
    assert(!travel.hold(target, 500, 600, 250010, false, Rect()));

    // Crossing a new item briefly retains the original. A dwell yields once,
    // and validates the candidate again before requesting a native refresh.
    Rect other(280, 100, 60, 90), third(400, 100, 60, 90);
    travel.reset(origin);
    assert(travel.hold(target, 300, 145, 100, true, other));
    assert(travel.hold(target, 300, 145, 449, true, other));
    assert(!travel.hold(target, 300, 145, 450, true, other));
    assert(travel.takeRefresh(other));
    assert(!travel.takeRefresh(other));
    assert(!travel.hold(target, 300, 145, 1000, true, other));
    assert(!travel.takeRefresh(other));
    assert(travel.hold(target, 350, 145, 1010, false, Rect()));
    assert(travel.hold(target, 420, 145, 1020, true, third));
    assert(!travel.hold(target, 420, 145, 1370, true, third));
    assert(!travel.takeRefresh(other));
    assert(!travel.hold(target, 420, 145, 1380, true, third, true));
    assert(travel.hold(target, 500, 145, 1390, false, Rect()));

    // Returning to the source rearms normally; separate crossed icons do not
    // inherit the dwell timer. Unsigned elapsed time tolerates DWORD wrap.
    assert(!travel.hold(target, 110, 110, 1400, false, Rect()));
    assert(travel.hold(target, 300, 145, 1410, true, other));
    assert(travel.hold(target, 420, 145, 1700, true, third));
    assert(travel.hold(target, 420, 145, 2049, true, third));
    assert(!travel.hold(target, 420, 145, 2050, true, third));
    travel.reset(origin);
    const unsigned long nearWrap = ~static_cast<unsigned long>(0) - 100;
    assert(travel.hold(target, 300, 145, nearWrap, true, other));
    assert(!travel.hold(target, 300, 145, nearWrap + 350, true, other));
    assert(travel.takeRefresh(other));
    HoverTransition bentPath;
    bentPath.reset(Rect(100, 500, 32, 50));
    const Rect highPanel(700, 40, 210, 240);
    assert(bentPath.hold(highPanel, 300, 520, 1000, false, Rect()));
    assert(bentPath.hold(highPanel, 710, 520, 4000, false, Rect()));
    assert(bentPath.hold(highPanel, 710, 350, 8000, false, Rect()));
    assert(bentPath.hold(highPanel, 710, 150, 12000, false, Rect()));
    assert(!bentPath.hold(highPanel, 950, 520, 13000, false, Rect()));
    std::cout << "Tooltip geometry and real pointer trajectory tests passed.\n";
}
