// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RACE_LIST_PAGE_NAVIGATION_H
#define RACE_LIST_PAGE_NAVIGATION_H
#include <cstddef>

namespace PageNavigation {
enum Action { Toggle, Previous, Next };

// Always navigate from the page actually displayed in the active pane. A
// comparison may have more pages, and the shared page can exceed this pane's
// total; neither must create invisible extra steps in its < and > buttons.
inline bool apply(Action action, bool& expanded, std::size_t& sharedPage,
    std::size_t& displayedPage, std::size_t localPageCount) {
    if (action == Toggle) {
        expanded = !expanded;
        sharedPage = displayedPage = 0;
        return true;
    }
    if (!expanded || localPageCount <= 1) return false;
    const std::size_t current = displayedPage < localPageCount
        ? displayedPage : localPageCount - 1;
    displayedPage = action == Next ? (current + 1) % localPageCount
        : (current ? current - 1 : localPageCount - 1);
    sharedPage = displayedPage;
    return true;
}
}
#endif
