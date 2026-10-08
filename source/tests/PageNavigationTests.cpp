// SPDX-License-Identifier: GPL-3.0-or-later
#include "../PageNavigation.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

using PageNavigation::apply;
using PageNavigation::Next;
using PageNavigation::Previous;
using PageNavigation::Toggle;

int main() {
    bool expanded = true;
    std::size_t sharedPage = 4; // Comparison pane is on page 5 of 5.
    std::size_t displayedPage = 1; // Main pane displays its last page, 2 of 2.

    // One click on the main pane must immediately wrap its visible page. A
    // longer comparison list must never consume clicks on invisible pages.
    assert(apply(Next, expanded, sharedPage, displayedPage, 2));
    assert(expanded && displayedPage == 0 && sharedPage == 0);

    // Multiple commands queued before the next render each advance from the
    // result of the preceding command, rather than from stale rendered state.
    const std::size_t expectedNext[] = { 1, 0, 1, 0, 1 };
    for (unsigned i = 0; i < 5; ++i) {
        assert(apply(Next, expanded, sharedPage, displayedPage, 2));
        assert(displayedPage == expectedNext[i] && sharedPage == expectedNext[i]);
    }

    // Previous wraps within the clicked pane too, independently of the shared
    // state left by a comparison with more pages.
    sharedPage = 4;
    displayedPage = 0;
    assert(apply(Previous, expanded, sharedPage, displayedPage, 2));
    assert(displayedPage == 1 && sharedPage == 1);
    assert(apply(Previous, expanded, sharedPage, displayedPage, 2));
    assert(displayedPage == 0 && sharedPage == 0);

    // The comparison pane can still navigate through all five of its own pages.
    sharedPage = displayedPage = 1;
    const std::size_t expectedComparison[] = { 2, 3, 4, 0, 1 };
    for (unsigned i = 0; i < 5; ++i) {
        assert(apply(Next, expanded, sharedPage, displayedPage, 5));
        assert(displayedPage == expectedComparison[i]);
        assert(sharedPage == displayedPage);
    }
    assert(apply(Previous, expanded, sharedPage, displayedPage, 5));
    assert(displayedPage == 0 && sharedPage == 0);
    assert(apply(Previous, expanded, sharedPage, displayedPage, 5));
    assert(displayedPage == 4 && sharedPage == 4);

    // No page controls exist for one-page or empty sections. A stray queued
    // command must be a no-op and must not overwrite another pane's selection.
    for (std::size_t pageCount = 0; pageCount <= 1; ++pageCount) {
        sharedPage = 4;
        displayedPage = 0;
        assert(!apply(Next, expanded, sharedPage, displayedPage, pageCount));
        assert(!apply(Previous, expanded, sharedPage, displayedPage, pageCount));
        assert(expanded && sharedPage == 4 && displayedPage == 0);
    }

    // A collapsed list also ignores queued page commands without changing state.
    expanded = false;
    sharedPage = 4;
    displayedPage = 1;
    assert(!apply(Next, expanded, sharedPage, displayedPage, 2));
    assert(!apply(Previous, expanded, sharedPage, displayedPage, 2));
    assert(!expanded && sharedPage == 4 && displayedPage == 1);

    // Expand/collapse always starts at page one, including empty sections and
    // a repeated toggle arriving in the same batch of input events.
    assert(apply(Toggle, expanded, sharedPage, displayedPage, 2));
    assert(expanded && sharedPage == 0 && displayedPage == 0);
    sharedPage = 4;
    displayedPage = 1;
    assert(apply(Toggle, expanded, sharedPage, displayedPage, 2));
    assert(!expanded && sharedPage == 0 && displayedPage == 0);
    assert(apply(Toggle, expanded, sharedPage, displayedPage, 0));
    assert(expanded && sharedPage == 0 && displayedPage == 0);

    // If the active list shrinks, its previously displayed index is clamped
    // before applying a command, keeping the resulting index within this pane.
    sharedPage = displayedPage = 4;
    assert(apply(Next, expanded, sharedPage, displayedPage, 2));
    assert(sharedPage == 0 && displayedPage == 0);
    sharedPage = displayedPage = 4;
    assert(apply(Previous, expanded, sharedPage, displayedPage, 2));
    assert(sharedPage == 0 && displayedPage == 0);

    std::cout << "PageNavigation: 8 scenario groups passed.\n";
    return 0;
}
