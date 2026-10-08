// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef RACE_LIST_TOOLTIP_CONTROLS_H
#define RACE_LIST_TOOLTIP_CONTROLS_H

#include <Windows.h>
#include <kenshi/gui/ToolTip.h>
#include <kenshi/gui/InventoryGUI.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_Delegate.h>
#include <map>
#include <deque>
#include <sstream>
#include "TooltipControlGeometry.h"

namespace TooltipControls {
enum Action { Toggle, PreviousPage, NextPage };

// All methods run on Kenshi's GUI thread. Call remove() before the native
// ToolTipInventory destructor. A native clearLines() leaves these popup widgets
// intact because they belong to panel, not to a ToolTipLine.
class Manager {
    struct Entry {
        ToolTipInventory* owner;
        MyGUI::Widget* caller;
        MyGUI::Widget* bar;
        MyGUI::Button* toggle;
        MyGUI::Button* previous;
        MyGUI::Button* next;
        bool available;
        bool failed;
        bool cacheValid;
        bool cachedExpanded;
        size_t cachedPage, cachedPageCount;
        Rect cachedBar;
        void* sourceInventory;
        HoverTransition transition;
        std::deque<Action> pending;

        Entry(ToolTipInventory* value) : owner(value), caller(NULL), bar(NULL),
            toggle(NULL), previous(NULL), next(NULL), available(false), failed(false),
            cacheValid(false), cachedExpanded(false), cachedPage(0), cachedPageCount(0), sourceInventory(NULL) {}

        void click(MyGUI::Widget* sender) {
            if (!available || !bar || !bar->getInheritedVisible()
                || !owner->caller || owner->caller != caller
                || !caller->getInheritedVisible()) return;
            DWORD process = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &process);
            if (process != GetCurrentProcessId()) return;
            MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
            if (!input || input->isModalAny()) return;
            Action action;
            if (sender == toggle) action = Toggle;
            else if (sender == previous && previous->getVisible()) action = PreviousPage;
            else if (sender == next && next->getVisible()) action = NextPage;
            else return;
            // A click delegate must never leak an allocation exception into
            // MyGUI's input dispatcher. A missed click under OOM is harmless.
            try { if (pending.size() < 16) pending.push_back(action); }
            catch (...) {}
        }

        void create() {
            std::ostringstream id;
            id << "RaceListControls_" << static_cast<void*>(owner);
            // The game's ToolTip layer has Pick=false. A popup child on Top is
            // pickable while remaining tied to the native panel's visibility.
            bar = owner->panel->createWidget<MyGUI::Widget>(MyGUI::WidgetStyle::Popup,
                "PanelEmpty", MyGUI::IntCoord(0, 0, 1, 1), MyGUI::Align::Default,
                "Top", id.str());
            bar->setVisible(false);
            bar->setNeedKeyFocus(false);
            bar->setNeedMouseFocus(false);
            bar->setInheritsPick(true);
            bar->setNeedToolTip(false);
            toggle = button(id.str() + "_Toggle");
            previous = button(id.str() + "_Previous");
            next = button(id.str() + "_Next");
            previous->setCaption("<");
            next->setCaption(">");
            cacheValid = false;
        }

        MyGUI::Button* button(const std::string& name) {
            MyGUI::Button* result = bar->createWidget<MyGUI::Button>("Kenshi_Button1",
                MyGUI::IntCoord(0, 0, 1, 1), MyGUI::Align::Default, name);
            result->setNeedKeyFocus(false);
            result->setNeedMouseFocus(true);
            result->setNeedToolTip(false);
            result->eventMouseButtonClick += MyGUI::newDelegate(this, &Entry::click);
            return result;
        }

        void hide() {
            available = false;
            pending.clear();
            if (bar && bar->getVisible()) bar->setVisible(false);
        }

        void destroy() {
            hide();
            if (bar) MyGUI::Gui::getInstance().destroyWidget(bar);
            bar = NULL; toggle = previous = next = NULL;
        }
    };

    typedef std::map<ToolTip*, Entry*> Entries;
    Entries entries_;
    Manager(const Manager&);
    Manager& operator=(const Manager&);

    static Rect rect(MyGUI::Widget* widget) {
        const MyGUI::IntCoord value = widget->getAbsoluteCoord();
        return Rect(value.left, value.top, value.width, value.height);
    }

    struct HoveredSource {
        void* inventory;
        MyGUI::Widget* section;
        Rect item;
        bool hasItem;
        HoveredSource() : inventory(NULL), section(NULL), hasItem(false) {}
    };

    // Resolve only live widget data, as the native tooltip does. Keep copied
    // rectangles and an opaque inventory identity, never an Item/Icon pointer.
    static HoveredSource inspectSection(MyGUI::Widget* widget, const MyGUI::IntPoint& mouse) {
        HoveredSource result;
        if (!widget || !widget->getInheritedVisible()
            || !containsPoint(rect(widget), mouse.left, mouse.top)
            || !widget->isUserString("section")) return result;
        InventoryGUI** data = widget->getUserData<InventoryGUI*>(false);
        if (!data || !*data) return result;
        InventoryGUI* inventory = *data;
        const std::string& name = widget->getUserString("section");
        // Avoid the native map lookup for unrelated or stale user-string data.
        const auto found = inventory->inventorySections.find(name);
        if (found == inventory->inventorySections.end() || !found->second) return result;
        result.inventory = inventory;
        result.section = widget;
        // Native getSelectedItem clamps outside coordinates onto an edge cell.
        // The rectangle check above is therefore required before calling it.
        Item* selected = inventory->getSelectedItem(name);
        result.hasItem = selected != NULL;
        if (!selected) return result;
        result.item = Rect(mouse.left - 4, mouse.top - 4, 8, 8);
        const auto& icons = found->second->itemsIcons;
        for (size_t i = 0; i < icons.size(); ++i) {
            if (!icons[i] || icons[i]->item != selected) continue;
            MyGUI::Widget* icon = icons[i]->getWidget();
            if (icon && icon->getInheritedVisible()) {
                const Rect bounds = rect(icon);
                if (containsPoint(bounds, mouse.left, mouse.top)) result.item = bounds;
            }
            break;
        }
        return result;
    }

    static HoveredSource currentSource(const Entry* entry, const MyGUI::IntPoint& mouse,
        MyGUI::Widget* preferred = NULL) {
        if (preferred) {
            const HoveredSource result = inspectSection(preferred, mouse);
            if (result.section) return result;
        }
        MyGUI::Widget* focused = MyGUI::InputManager::getInstance().getMouseFocusWidget();
        for (unsigned depth = 0; focused && depth < 32; ++depth, focused = focused->getParent()) {
            const HoveredSource result = inspectSection(focused, mouse);
            if (result.section) return result;
        }
        return inspectSection(entry->caller, mouse);
    }

    Entry* ensureEntry(ToolTipInventory* owner) {
        Entries::iterator found = entries_.find(owner);
        if (found != entries_.end()) return found->second;
        Entry* entry = new Entry(owner);
        try { entries_.insert(std::make_pair(static_cast<ToolTip*>(owner), entry)); }
        catch (...) { delete entry; throw; }
        return entry;
    }

    static bool usable(const Entry* entry, bool foreground) {
        if (!foreground || !entry || !entry->available || !entry->owner->panel
            || !entry->owner->caller || entry->owner->caller != entry->caller
            || !entry->owner->panel->getInheritedVisible()
            || !entry->caller->getInheritedVisible()) return false;
        // Viewing tooltip pages does not act on the source inventory widget.
        // A visible disabled source can still show a tooltip and accept F8;
        // do not silently discard its otherwise valid mouse controls.
        MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
        return input && !input->isModalAny();
    }

public:
    Manager() {}
    // Native tooltip destruction owns cleanup. Do not touch GUI singletons
    // during process/DLL static teardown, when MyGUI may already be shut down.
    ~Manager() {}

    // Call after a supported native setContent, including when the same grid
    // caller now represents another item. Cached pagination must not call this.
    void recordSource(ToolTipInventory* owner, MyGUI::Widget* caller) {
        if (!owner || !caller) return;
        Entry* entry = ensureEntry(owner);
        const MyGUI::IntPoint mouse = MyGUI::InputManager::getInstance().getMousePosition();
        const HoveredSource source = inspectSection(caller, mouse);
        if (entry->caller != caller) entry->failed = false;
        entry->caller = caller;
        entry->sourceInventory = source.inventory;
        entry->transition.reset(source.hasItem ? source.item : Rect(mouse.left - 4, mouse.top - 4, 8, 8));
        entry->pending.clear();
    }

    // Invoke at the END of setContent, after all replay/reflow. The final row
    // must be RaceSections' dedicated keyboard hint. Native setPosition sizes
    // its panel from that row's bottom, so the toolbar gets real layout space.
    static void reserveFooter(ToolTipInventory* owner, bool hasSections) {
        if (!hasSections || !owner || owner->lines.empty()) return;
        ToolTip::ToolTipLine* line = owner->lines.back();
        if (!line || !line->content) return;
        int height = line->content->getHeight();
        int desired = line->leftBox ? line->leftBox->getFontHeight() + 12 : 30;
        if (desired < 30) desired = 30;
        if (height < desired) line->content->setSize(line->content->getWidth(), desired);
    }

    // Invoke after native update/show. It is safe to call every frame; widgets
    // are reused, and are hidden whenever the native tooltip is unavailable.
    void sync(ToolTipInventory* owner, bool hasSections, bool expanded,
        size_t page, size_t pageCount, bool foreground) {
        Entries::iterator found = entries_.find(owner);
        if (!hasSections || !owner || !owner->panel || !owner->caller
            || !owner->panel->getInheritedVisible() || !owner->caller->getInheritedVisible()
            || !foreground || MyGUI::InputManager::getInstance().isModalAny()
            || owner->lines.empty()) {
            if (found != entries_.end()) found->second->hide();
            return;
        }
        Entry* entry = ensureEntry(owner);
        if (entry->caller != owner->caller) {
            entry->pending.clear();
            entry->caller = owner->caller;
            entry->failed = false;
            recordSource(owner, owner->caller);
        }
        if (!entry->transition.valid()) recordSource(owner, owner->caller);
        if (entry->failed) return;
        if (!entry->bar) {
            try { entry->create(); }
            catch (...) { entry->destroy(); throw; }
        }
        ToolTip::ToolTipLine* footer = owner->lines.back();
        const Rect panel = rect(owner->panel), line = rect(footer->content);
        const int width = line.width > 4 ? line.width - 4 : 1;
        const int height = line.height > 4 ? line.height - 4 : 1;
        // Popup coordinates are absolute screen coordinates in Kenshi's
        // MyGUI build, even though the widget remains owned by panel.
        const Rect bar(line.left + 2, line.top + 2, width, height);
        const bool pages = expanded && pageCount > 1;
        const bool cachedPages = entry->cachedExpanded && entry->cachedPageCount > 1;
        if (!entry->cacheValid || bar.left != entry->cachedBar.left || bar.top != entry->cachedBar.top
            || width != entry->cachedBar.width || height != entry->cachedBar.height || pages != cachedPages) {
            const int smallWidth = pages ? width / 5 : 0;
            const int toggleWidth = width - smallWidth * 2 - (pages ? 8 : 0);
            entry->bar->setCoord(bar.left, bar.top, width, height);
            entry->toggle->setCoord(0, 0, toggleWidth, height);
            entry->previous->setCoord(toggleWidth + 4, 0, smallWidth, height);
            entry->next->setCoord(toggleWidth + smallWidth + 8, 0, smallWidth, height);
            entry->cachedBar = bar;
        }
        if (!entry->cacheValid || expanded != entry->cachedExpanded || page != entry->cachedPage
            || pageCount != entry->cachedPageCount) {
            std::ostringstream caption;
            caption << (expanded ? "Collapse" : "Expand races");
            if (pages) caption << " (" << page + 1 << '/' << pageCount << ')';
            entry->toggle->setCaption(caption.str());
        }
        if (!entry->cacheValid || pages != cachedPages) {
            entry->previous->setVisible(pages);
            entry->next->setVisible(pages);
        }
        entry->cachedExpanded = expanded;
        entry->cachedPage = page; entry->cachedPageCount = pageCount;
        entry->cacheValid = true;
        if (footer->leftBox && footer->leftBox->getVisible()) footer->leftBox->setVisible(false);
        // The footer has an empty right caption. Native ToolTipLine leaves
        // rightBox UNINITIALIZED in that case, rather than assigning NULL.
        // Never read it: even a null check can see arbitrary heap contents.
        entry->available = true;
        if (!entry->bar->getVisible()) entry->bar->setVisible(true);
    }

    // Gate ONLY tooltip mouse-leave notifications and native inventory update.
    // Explicit native hide/clear/destruction must remain unconditional.
    bool shouldHold(ToolTip* owner, bool foreground) {
        return shouldHoldShow(owner, NULL, foreground);
    }

    // Native inventory mouseMoved calls show directly on every pixel. This
    // guard is needed in addition to update and tooltip Hide notifications.
    bool shouldHoldShow(ToolTip* owner, MyGUI::Widget* sender, bool foreground) {
        Entries::iterator found = entries_.find(owner);
        if (found == entries_.end() || !usable(found->second, foreground)) return false;
        Entry* entry = found->second;
        const MyGUI::IntPoint mouse = MyGUI::InputManager::getInstance().getMousePosition();
        const Rect panel = rect(owner->panel);
        const DWORD now = GetTickCount();
        if (containsPoint(panel, mouse.left, mouse.top, 3)) {
            entry->transition.arrived();
            return true;
        }
        // Keep the main tooltip alive while the mouse uses its comparison pane.
        for (Entries::iterator it = entries_.begin(); it != entries_.end(); ++it) {
            Entry* other = it->second;
            if (other == entry || !usable(other, foreground)) continue;
            if (entry->owner->compareTooltip != other->owner
                && other->owner->compareTooltip != entry->owner) continue;
            if (containsPoint(rect(other->owner->panel), mouse.left, mouse.top, 3)) {
                entry->transition.arrived();
                return true;
            }
        }
        if (entry->transition.inSource(mouse.left, mouse.top)
            || !entry->transition.inCorridor(panel, mouse.left, mouse.top)) {
            entry->transition.cancel(); return false;
        }
        const HoveredSource hovered = currentSource(entry, mouse, sender);
        const bool foreign = hovered.inventory && hovered.inventory != entry->sourceInventory;
        // A different caller is allowed only after proving it belongs to the
        // same InventoryGUI (e.g. crossing equipment slots toward the panel).
        const bool unknownSender = sender && sender != entry->caller
            && (!hovered.inventory || hovered.inventory != entry->sourceInventory);
        return entry->transition.hold(panel, mouse.left, mouse.top, now,
            hovered.hasItem, hovered.item, foreign || unknownSender);
    }

    // Dwell may finish without another mouseMoved event. The update hook can
    // then call its original native show with this freshly validated section.
    // No candidate Widget pointer is retained between frames.
    MyGUI::Widget* takeNativeRefresh(ToolTipInventory* owner) {
        Entries::iterator found = entries_.find(owner);
        if (found == entries_.end() || !usable(found->second, true)) return NULL;
        Entry* entry = found->second;
        const MyGUI::IntPoint mouse = MyGUI::InputManager::getInstance().getMousePosition();
        const HoveredSource hovered = currentSource(entry, mouse);
        if (!hovered.section || !hovered.hasItem || !hovered.inventory
            || hovered.inventory != entry->sourceInventory) return NULL;
        return entry->transition.takeRefresh(hovered.item) ? hovered.section : NULL;
    }

    bool pop(ToolTipInventory* owner, MyGUI::Widget* caller, Action& action) {
        Entries::iterator found = entries_.find(owner);
        if (found == entries_.end()) return false;
        Entry* entry = found->second;
        if (!usable(entry, true) || entry->caller != caller) {
            entry->pending.clear(); return false;
        }
        if (entry->pending.empty()) return false;
        action = entry->pending.front(); entry->pending.pop_front();
        return true;
    }

    void hide(ToolTip* owner) {
        Entries::iterator it = entries_.find(owner);
        if (it != entries_.end()) it->second->hide();
    }

    // Recovery for a caught sync exception. Keep the keyboard hint available
    // and do not retry a broken widget/skin every frame on this same item.
    // The caller can log the original exception once before invoking this.
    void disable(ToolTipInventory* owner) {
        Entries::iterator it = entries_.find(owner);
        if (it == entries_.end()) return;
        it->second->failed = true;
        try { it->second->hide(); } catch (...) {}
        try {
            if (!owner->lines.empty()) {
                ToolTip::ToolTipLine* footer = owner->lines.back();
                if (footer->leftBox) footer->leftBox->setVisible(true);
            }
        } catch (...) {}
    }

    void remove(ToolTip* owner) {
        Entries::iterator it = entries_.find(owner);
        if (it == entries_.end()) return;
        Entry* entry = it->second;
        entries_.erase(it);
        entry->destroy();
        delete entry;
    }
};
}
#endif
