// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RACE_LIST_TOOLTIP_CONTROL_GEOMETRY_H
#define RACE_LIST_TOOLTIP_CONTROL_GEOMETRY_H

#include <algorithm>

namespace TooltipControls {
struct Rect {
    int left, top, width, height;
    Rect(int x = 0, int y = 0, int w = 0, int h = 0)
        : left(x), top(y), width(w), height(h) {}
};

inline bool containsPoint(const Rect& r, int x, int y, int padding = 0) {
    return r.width > 0 && r.height > 0 && x >= r.left - padding
        && y >= r.top - padding && x < r.left + r.width + padding
        && y < r.top + r.height + padding;
}

struct BridgePoint {
    double x, y;
    BridgePoint(double left = 0, double top = 0) : x(left), y(top) {}
    bool operator<(const BridgePoint& other) const {
        return x < other.x || (x == other.x && y < other.y);
    }
};

inline double bridgeCross(const BridgePoint& a, const BridgePoint& b, const BridgePoint& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// The convex hull includes a straight path from anywhere on the item to
// anywhere on the panel, including a footer far below the item. It excludes
// the unrelated corners of their bounding box. This region never creates a
// widget or intercepts inventory input. A zero maxGap removes the distance cap
// for a corridor anchored to a real item, which may span a wide inventory.
inline bool inBridge(const Rect& a, const Rect& b, int x, int y, double maxGap = 240.0) {
    if (a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0) return false;
    const int ar = a.left + a.width, ab = a.top + a.height;
    const int br = b.left + b.width, bb = b.top + b.height;
    const double dx = ar < b.left ? b.left - ar : (br < a.left ? a.left - br : 0);
    const double dy = ab < b.top ? b.top - ab : (bb < a.top ? a.top - bb : 0);
    if (maxGap > 0.0 && dx * dx + dy * dy > maxGap * maxGap) return false;
    BridgePoint points[8] = {
        BridgePoint(a.left, a.top), BridgePoint(ar, a.top),
        BridgePoint(ar, ab), BridgePoint(a.left, ab),
        BridgePoint(b.left, b.top), BridgePoint(br, b.top),
        BridgePoint(br, bb), BridgePoint(b.left, bb)
    };
    std::sort(points, points + 8);
    BridgePoint hull[16];
    int count = 0;
    for (int i = 0; i < 8; ++i) {
        while (count >= 2 && bridgeCross(hull[count - 2], hull[count - 1], points[i]) <= 0) --count;
        hull[count++] = points[i];
    }
    const int lower = count;
    for (int i = 6; i >= 0; --i) {
        while (count > lower && bridgeCross(hull[count - 2], hull[count - 1], points[i]) <= 0) --count;
        hull[count++] = points[i];
    }
    const BridgePoint mouse(x, y);
    for (int i = 0; i + 1 < count; ++i)
        if (bridgeCross(hull[i], hull[i + 1], mouse) < 0) return false;
    return count >= 4;
}

inline bool sameRect(const Rect& a, const Rect& b) {
    return a.left == b.left && a.top == b.top && a.width == b.width && a.height == b.height;
}

// A submenu-style pointer corridor. Empty space has no time limit, allowing
// slow movement all the way to the tooltip. Crossing another item is allowed
// briefly; dwelling on it yields to the native tooltip after 350 ms.
class HoverTransition {
    Rect source_;
    Rect candidate_;
    bool valid_;
    bool dwelling_;
    bool released_;
    bool refreshPending_;
    unsigned long enteredAt_;

    void clearCandidate() {
        dwelling_ = released_ = refreshPending_ = false;
    }

public:
    HoverTransition() : valid_(false), dwelling_(false), released_(false),
        refreshPending_(false), enteredAt_(0) {}

    void reset(const Rect& source) {
        source_ = source;
        valid_ = source.width > 0 && source.height > 0;
        clearCandidate();
    }

    bool valid() const { return valid_; }
    bool inSource(int x, int y) const { return valid_ && containsPoint(source_, x, y); }
    bool inCorridor(const Rect& panel, int x, int y) const {
        if (!valid_ || panel.width <= 0 || panel.height <= 0) return false;
        // The bounded rectangle also accepts an ordinary horizontal-then-
        // vertical path. It holds tooltip text only; it never captures clicks.
        const int left = (std::min)(source_.left, panel.left);
        const int top = (std::min)(source_.top, panel.top);
        const int right = (std::max)(source_.left + source_.width, panel.left + panel.width);
        const int bottom = (std::max)(source_.top + source_.height, panel.top + panel.height);
        return containsPoint(Rect(left, top, right - left, bottom - top), x, y, 12);
    }
    void arrived() { clearCandidate(); }
    void cancel() { clearCandidate(); }

    bool hold(const Rect& panel, int x, int y, unsigned long now,
        bool overAnotherItem, const Rect& item, bool foreignInventory = false) {
        if (!valid_ || foreignInventory) { clearCandidate(); return false; }
        if (containsPoint(panel, x, y, 3)) { clearCandidate(); return true; }
        if (inSource(x, y) || !inCorridor(panel, x, y)) {
            clearCandidate(); return false;
        }
        if (!overAnotherItem) { clearCandidate(); return true; }
        if (!dwelling_ || !sameRect(candidate_, item)) {
            candidate_ = item;
            enteredAt_ = now;
            dwelling_ = true;
            released_ = refreshPending_ = false;
        }
        if (static_cast<unsigned long>(now - enteredAt_) < 350) return true;
        if (!released_) { released_ = true; refreshPending_ = true; }
        return false;
    }

    bool takeRefresh(const Rect& currentItem) {
        if (!refreshPending_) return false;
        refreshPending_ = false;
        return sameRect(candidate_, currentItem);
    }
};
}
#endif
