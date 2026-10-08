// SPDX-License-Identifier: GPL-3.0-or-later
// Race List Collapse: display-only inventory tooltip plugin for RE_Kenshi.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Debug.h>
#include <core/Functions.h>
#include <kenshi/Kenshi.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Gear.h>
#include <kenshi/gui/ToolTip.h>
#include <map>
#include <sstream>
#include <algorithm>
#include "RaceSections.h"
#include "NativeCatalog.h"
#include "InputEvents.h"
#include "PageNavigation.h"
#include "TooltipControls.h"
#include "WearableRaces.h"

namespace {
HMODULE moduleHandle = NULL;
bool enabled = false, expanded = false, debugLogging = false;
int toggleKey = VK_F8, previousKey = VK_PRIOR, nextKey = VK_NEXT;
size_t pageSize = 6, page = 0;
unsigned long revision = 1;
InputEvents::Buffer inputEvents;
TooltipControls::Manager controls;
RaceSections::Labels labels;
NativeCatalog catalog;
std::string gameDirectory;
GameWorld* namesWorld = NULL;
size_t cachedRaceCount = 0, cachedGroupCount = 0;

struct TooltipState {
    bool hasSections;
    bool renderedExpanded;
    size_t pageCount;
    size_t page;
    unsigned long revision;
    MyGUI::Widget* caller;
    std::vector<RaceSections::Row> sourceRows;
    TooltipState() : hasSections(false), renderedExpanded(false), pageCount(1), page(0), revision(0), caller(NULL) {}
};
std::map<ToolTipInventory*, TooltipState> states;
struct Capture {
    ToolTipInventory* tooltip;
    std::vector<RaceSections::Row> rows;
    Capture* previous;
    bool failed;
    Capture(ToolTipInventory* t) : tooltip(t), previous(NULL), failed(false) {}
};
__declspec(thread) Capture* capture = NULL;

void (*originalAddLine)(ToolTip*, const std::string&, const std::string&) = NULL;
void (*originalSetContent)(ToolTipInventory*, MyGUI::Widget*) = NULL;
void (*originalShow)(ToolTipInventory*, MyGUI::Widget*, const MyGUI::IntPoint&) = NULL;
void (*originalUpdate)(ToolTipInventory*) = NULL;
void (*originalDestroy)(ToolTip*) = NULL;
void (*originalKeyDown)(InputHandler*, OIS::KeyCode) = NULL;
void (*originalKeyUp)(InputHandler*, OIS::KeyCode) = NULL;
void (*originalNotify)(ToolTip*, MyGUI::Widget*, const MyGUI::ToolTipInfo&) = NULL;
void (*originalArmourData)(Armour*, Ogre::vector<StringPair>::type&) = NULL;

void log(const std::string& message) { DebugLog("RaceListCollapse: " + message); }

void syncControls(ToolTipInventory* self, bool hasSections, size_t currentPage, size_t pageCount, bool foreground) {
    try { controls.sync(self, hasSections, expanded, currentPage, pageCount, foreground); }
    catch (const std::exception& e) { log(std::string("Could not update tooltip controls: ") + e.what()); try { controls.disable(self); } catch (...) {} }
    catch (...) { log("Could not update tooltip controls."); try { controls.disable(self); } catch (...) {} }
}

void armourDataHook(Armour* self, Ogre::vector<StringPair>::type& rows) {
    originalArmourData(self, rows);
    if (!enabled) return;
    const size_t originalSize = rows.size();
    try {
        if (WearableRaces::append(self, rows) && debugLogging) {
            const WearableRaces::Result result = WearableRaces::get(self);
            std::ostringstream msg;
            msg << "wearable item=" << self->getName() << " entries=" << result.names.size()
                << " considered=" << result.considered << " unavailableSlot=" << result.rejectedSlot
                << " restricted=" << result.rejectedLimiter;
            log(msg.str());
        }
    } catch (const std::exception& e) {
        rows.resize(originalSize);
        log(std::string("Keeping original armour tooltip: ") + e.what());
    } catch (...) {
        rows.resize(originalSize);
        log("Keeping original armour tooltip after an equipment lookup error.");
    }
}

std::string directoryOf(HMODULE module) {
    char path[32768];
    DWORD size = GetModuleFileNameA(module, path, sizeof(path));
    if (!size || size >= sizeof(path)) return std::string();
    std::string result(path, size);
    return result.substr(0, result.find_last_of("\\/"));
}

std::string languageFromSettings() {
    std::ifstream file((gameDirectory + "\\settings.cfg").c_str());
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 9, "language=") == 0) {
            std::string lang = line.substr(9);
            while (!lang.empty() && (lang[lang.size()-1] == '\r' || lang[lang.size()-1] == ' ')) lang.erase(lang.size()-1);
            if (lang.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos)
                return lang;
        }
    }
    return "en_GB";
}

void addNative(std::vector<std::string>& list, const char* key) {
    list.push_back(key);
    std::string translated = catalog.get(key);
    if (translated != key) list.push_back(translated);
}

std::string keyName(int key) {
    std::ostringstream name;
    if (key >= VK_F1 && key <= VK_F24) name << 'F' << key - VK_F1 + 1;
    else if (key == VK_PRIOR) return "PgUp";
    else if (key == VK_NEXT) return "PgDn";
    else if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) name << char(key);
    else name << "Key " << key;
    return name.str();
}

void refreshNames() {
    if (!ou) return;
    // Data comes from the loaded game, so additional races need no per-mod patch.
    // Read the existing maps. Kenshi's lektor has no owning destructor, so do
    // not allocate a temporary list via getDataOfType on every tooltip.
    const auto& categories = ou->gamedata.gamedataCatSID;
    const auto races = categories.find(RACE);
    const auto groups = categories.find(RACE_GROUP);
    const size_t raceCount = races == categories.end() ? 0 : races->second.size();
    const size_t groupCount = groups == categories.end() ? 0 : groups->second.size();
    if (namesWorld == ou && cachedRaceCount == raceCount && cachedGroupCount == groupCount) return;
    labels.raceNames.clear();
    if (races != categories.end())
        for (auto it = races->second.begin(); it != races->second.end(); ++it)
            if (it->second && !it->second->name.empty()) labels.raceNames.push_back(it->second->name);
    if (groups != categories.end())
        for (auto it = groups->second.begin(); it != groups->second.end(); ++it)
            if (it->second && !it->second->name.empty()) labels.raceNames.push_back(it->second->name);
    std::sort(labels.raceNames.begin(), labels.raceNames.end());
    labels.raceNames.erase(std::unique(labels.raceNames.begin(), labels.raceNames.end()), labels.raceNames.end());
    namesWorld = ou; cachedRaceCount = raceCount; cachedGroupCount = groupCount;
}

void addLineHook(ToolTip* self, const std::string& left, const std::string& right) {
    if (capture && capture->tooltip == self && !capture->failed) {
        try {
            RaceSections::Row row(left, right);
            row.sourceIndex = static_cast<int>(capture->rows.size());
            capture->rows.push_back(row);
            return;
        } catch (...) {
            // If buffering fails, replay what was collected and let native
            // rendering handle this and all remaining rows immediately.
            capture->failed = true;
            for (size_t i = 0; i < capture->rows.size(); ++i)
                originalAddLine(self, capture->rows[i].left, capture->rows[i].right);
        }
    }
    originalAddLine(self, left, right);
}

void setContentHook(ToolTipInventory* self, MyGUI::Widget* caller) {
    if (!enabled) { originalSetContent(self, caller); return; }
    controls.hide(self);
    Capture local(self);
    local.previous = capture;
    capture = &local;
    try { originalSetContent(self, caller); }
    catch (...) { capture = local.previous; throw; }
    capture = local.previous;
    if (local.failed) { states.erase(self); return; }
    static unsigned int diagnosticCount = 0;
    if (debugLogging && diagnosticCount++ < 32) {
        for (size_t i = 0; i < local.rows.size(); ++i) {
            std::ostringstream msg;
            msg << "source row " << i << " left(" << local.rows[i].left.size()
                << ")=" << local.rows[i].left << " right(" << local.rows[i].right.size()
                << ")=" << local.rows[i].right;
            log(msg.str());
        }
    }
    // Capture/replay keeps all non-race text, formatting and comparisons intact.
    const std::vector<RaceSections::Row>* output = &local.rows;
    RaceSections::Result result;
    try {
        TooltipState& state = states[self];
        state.caller = caller;
        state.sourceRows = local.rows;
        state.revision = revision;
        state.hasSections = false;
        state.pageCount = 1;
        refreshNames();
        result = RaceSections::transform(local.rows, labels, expanded, page, pageSize);
        state.hasSections = result.hasSections;
        state.renderedExpanded = expanded;
        state.pageCount = result.pageCount;
        state.page = result.page;
        output = &result.rows;
        if (debugLogging && state.hasSections) {
            std::ostringstream msg;
            msg << "render mode=" << (expanded ? "expanded" : "collapsed")
                << " sections=" << result.sectionCount << " entries=" << result.raceCount
                << " page=" << result.page + 1 << "/" << result.pageCount
                << " rows=" << local.rows.size() << "->" << result.rows.size();
            log(msg.str());
        }
    } catch (const std::exception& e) {
        output = &local.rows;
        states.erase(self);
        log(std::string("Keeping original tooltip after formatting error: ") + e.what());
    } catch (...) {
        output = &local.rows;
        states.erase(self);
        log("Keeping original tooltip after formatting error.");
    }
    for (size_t i = 0; i < output->size(); ++i)
        originalAddLine(self, (*output)[i].left, (*output)[i].right);
    if (result.hasSections && self->panel && !self->lines.empty()) {
        // Native positioning can narrow the first tooltip after its rows were
        // created. Reflow once at the native width to keep the right column in
        // view. setPosition requires at least one line.
        const int width = self->panel->getWidth();
        const MyGUI::IntPoint position = self->panel->getPosition();
        self->_NV_setPosition(position);
        if (self->panel->getWidth() != width) {
            self->clearLines();
            for (size_t i = 0; i < output->size(); ++i)
                originalAddLine(self, (*output)[i].left, (*output)[i].right);
        }
    }
    try { TooltipControls::Manager::reserveFooter(self, result.hasSections); }
    catch (...) { log("Could not reserve tooltip controls space."); }
    if (result.hasSections) {
        try { controls.recordSource(self, caller); }
        catch (...) { log("Could not record the tooltip source item."); }
    }
}

bool foregroundIsGame() {
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId();
}

void refreshCapturedRows(ToolTipInventory* self, TooltipState& state) {
    // A grid's caller is the whole inventory section. Calling native show()
    // from its footer would select a different item at the new mouse position.
    // Keep the original item and its already-computed comparison values.
    const RaceSections::Result result = RaceSections::transform(state.sourceRows,
        labels, expanded, page, pageSize);
    if (result.rows.empty() || !self->panel || self->caller != state.caller) return;
    int minimumFooterTop = -1;
    if (state.renderedExpanded && expanded && state.pageCount > 1 && result.pageCount > 1
        && !self->lines.empty() && self->lines.back()->content) {
        // Keep the paging buttons under the mouse when a shorter page is
        // displayed. Natural item changes and explicit collapse reset this.
        minimumFooterTop = self->lines.back()->content->getTop();
    }
    controls.hide(self);
    const MyGUI::IntPoint position = self->panel->getPosition();
    const int width = self->panel->getWidth();
    self->clearLines();
    for (size_t i = 0; i < result.rows.size(); ++i)
        originalAddLine(self, result.rows[i].left, result.rows[i].right);
    TooltipControls::Manager::reserveFooter(self, result.hasSections);
    if (minimumFooterTop >= 0 && self->lines.back()->content->getTop() < minimumFooterTop)
        self->lines.back()->content->setPosition(self->lines.back()->content->getLeft(), minimumFooterTop);
    self->_NV_setPosition(position);
    if (self->panel->getWidth() != width) {
        self->clearLines();
        for (size_t i = 0; i < result.rows.size(); ++i)
            originalAddLine(self, result.rows[i].left, result.rows[i].right);
        TooltipControls::Manager::reserveFooter(self, result.hasSections);
        if (minimumFooterTop >= 0 && self->lines.back()->content->getTop() < minimumFooterTop)
            self->lines.back()->content->setPosition(self->lines.back()->content->getLeft(), minimumFooterTop);
        self->_NV_setPosition(position);
    }
    state.revision = revision;
    state.hasSections = result.hasSections;
    state.renderedExpanded = expanded;
    state.page = result.page;
    state.pageCount = result.pageCount;
    if (debugLogging) {
        std::ostringstream msg;
        msg << "cached render mode=" << (expanded ? "expanded" : "collapsed")
            << " entries=" << result.raceCount << " page=" << result.page + 1
            << '/' << result.pageCount;
        log(msg.str());
    }
}

bool eligible(ToolTipInventory* tooltip) {
    if (!enabled || !tooltip || !tooltip->caller || !tooltip->getVisible() || !foregroundIsGame()) return false;
    std::map<ToolTipInventory*, TooltipState>::const_iterator it = states.find(tooltip);
    return it != states.end() && it->second.hasSections;
}

ToolTipInventory* primaryTooltip() {
    for (std::map<ToolTipInventory*, TooltipState>::const_iterator it = states.begin(); it != states.end(); ++it) {
        if (!eligible(it->first)) continue;
        bool comparison = false;
        for (std::map<ToolTipInventory*, TooltipState>::const_iterator other = states.begin(); other != states.end(); ++other)
            if (other->first != it->first && other->first->compareTooltip == it->first && eligible(other->first)) { comparison = true; break; }
        if (!comparison) return it->first;
    }
    return NULL;
}

void keyDownHook(InputHandler* self, OIS::KeyCode key) {
    originalKeyDown(self, key);
    ToolTipInventory* primary = primaryTooltip();
    inputEvents.onKeyDown(static_cast<unsigned>(key), eligible(primary),
        self->ctrl || self->shift || self->alt, primary, primary ? primary->caller : NULL);
}

void keyUpHook(InputHandler* self, OIS::KeyCode key) {
    originalKeyUp(self, key);
    inputEvents.onKeyUp(static_cast<unsigned>(key));
}

void notifyHook(ToolTip* self, MyGUI::Widget* sender, const MyGUI::ToolTipInfo& info) {
    if (enabled && info.type == MyGUI::ToolTipInfo::Hide && controls.shouldHold(self, foregroundIsGame())) return;
    originalNotify(self, sender, info);
}

void showHook(ToolTipInventory* self, MyGUI::Widget* sender, const MyGUI::IntPoint& point) {
    // Native inventory mouseMoved calls show directly, before update or the
    // mouse-leave event can preserve the tooltip. Keep its captured contents
    // while the pointer crosses the inventory toward the controls.
    if (enabled && controls.shouldHoldShow(self, sender, foregroundIsGame())) return;
    originalShow(self, sender, point);
}

void applyAction(InputEvents::Action action, size_t pageCount, size_t& displayedPage) {
    const PageNavigation::Action navigation = action == InputEvents::Toggle
        ? PageNavigation::Toggle : action == InputEvents::NextPage
        ? PageNavigation::Next : PageNavigation::Previous;
    if (PageNavigation::apply(navigation, expanded, page, displayedPage, pageCount)) ++revision;
}

void updateHook(ToolTipInventory* self) {
    if (!enabled) { originalUpdate(self); return; }
    const bool foreground = foregroundIsGame();
    const bool holdForControls = controls.shouldHold(self, foreground);
    if (foreground) {
        MyGUI::Widget* refreshCaller = controls.takeNativeRefresh(self);
        if (refreshCaller) {
            // Settling over another item ends the corridor even if the mouse
            // stops moving, so the native mouseMoved event does not fire again.
            const MyGUI::IntPoint mouse = MyGUI::InputManager::getInstance().getMousePosition();
            originalShow(self, refreshCaller, mouse);
        }
    }
    if (!holdForControls) originalUpdate(self);
    ToolTipInventory* target = primaryTooltip();
    const bool primary = self == target;
    if (!target) inputEvents.clearPending();
    if (primary || !target) inputEvents.syncReleased();
    std::map<ToolTipInventory*, TooltipState>::iterator it = states.find(self);
    if (it == states.end() || !self->caller || !self->getVisible() || !it->second.hasSections) {
        syncControls(self, false, 0, 1, foreground);
        return;
    }
    TooltipState& state = it->second;
    const size_t pageCount = state.pageCount;
    size_t displayedPage = state.page;
    TooltipControls::Action click;
    while (foreground && controls.pop(self, self->caller, click)) {
        applyAction(click == TooltipControls::Toggle ? InputEvents::Toggle :
            click == TooltipControls::PreviousPage ? InputEvents::PreviousPage : InputEvents::NextPage, pageCount, displayedPage);
    }
    if (primary) {
        InputEvents::Action action;
        while (inputEvents.pop(self, self->caller, eligible(self), action)) {
            applyAction(action, pageCount, displayedPage);
        }
    }
    if (state.revision != revision) {
        try { refreshCapturedRows(self, state); }
        catch (...) {
            state.revision = revision;
            state.hasSections = false;
            log("Could not refresh the captured tooltip rows.");
        }
    }
    it = states.find(self);
    syncControls(self, it != states.end() && it->second.hasSections,
        it == states.end() ? 0 : it->second.page, it == states.end() ? 1 : it->second.pageCount, foreground);
}

void destroyHook(ToolTip* self) {
    try { controls.remove(self); } catch (...) { log("Could not release tooltip controls."); }
    inputEvents.clearPending();
    // Virtual deleting destructors call the base destructor directly; hooking
    // only ToolTipInventory::_DESTRUCTOR would miss that common deletion path.
    const bool tracked = states.erase(static_cast<ToolTipInventory*>(self)) != 0;
    if (tracked && states.empty()) { namesWorld = NULL; WearableRaces::resetCache(); }
    originalDestroy(self);
}

template <typename Method, typename Function>
bool hook(const char* name, Method method, void* replacement, Function** original) {
    // KenshiLib requires the imported stub itself, not a compiler-generated
    // thunk in this DLL. Reject a mismatched build without triggering its assert.
    void* stub = (void*&)method;
    HMODULE owner = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(stub), &owner) || owner != GetModuleHandleA("KenshiLib.dll")) {
        log(std::string("Invalid imported stub for ") + name + "; plugin disabled (build with /GL and /LTCG).");
        return false;
    }
    intptr_t address = KenshiLib::GetRealAddress(method);
    if (!address || KenshiLib::AddHook(address, replacement, original) != KenshiLib::SUCCESS) {
        log(std::string("Could not hook ") + name + "; plugin disabled.");
        return false;
    }
    log(std::string("Hooked ") + name);
    return true;
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) moduleHandle = module;
    return TRUE;
}

__declspec(dllexport) void startPlugin() {
    log("Starting v0.2.2");
    const std::string ini = directoryOf(moduleHandle) + "\\RaceListCollapse.ini";
    if (!GetPrivateProfileIntA("General", "Enabled", 1, ini.c_str())) { log("Disabled in configuration."); return; }
    debugLogging = GetPrivateProfileIntA("General", "DebugLogging", 0, ini.c_str()) != 0;
    pageSize = (std::max)(1, (std::min)(12, static_cast<int>(GetPrivateProfileIntA("General", "PageSize", 6, ini.c_str()))));
    toggleKey = GetPrivateProfileIntA("Keys", "Toggle", VK_F8, ini.c_str());
    previousKey = GetPrivateProfileIntA("Keys", "PreviousPage", VK_PRIOR, ini.c_str());
    nextKey = GetPrivateProfileIntA("Keys", "NextPage", VK_NEXT, ini.c_str());
    if (toggleKey < 1 || toggleKey > 255) toggleKey = VK_F8;
    if (previousKey < 1 || previousKey > 255) previousKey = VK_PRIOR;
    if (nextKey < 1 || nextKey > 255) nextKey = VK_NEXT;
    if (!inputEvents.configure(toggleKey, previousKey, nextKey)) {
        log("Invalid keyboard mapping; using F8/PgUp/PgDn defaults.");
        toggleKey = VK_F8; previousKey = VK_PRIOR; nextKey = VK_NEXT;
        if (!inputEvents.configure(toggleKey, previousKey, nextKey)) return;
    }
    gameDirectory = directoryOf(GetModuleHandleA("KenshiLib.dll"));
    const std::string lang = languageFromSettings();
    if (!catalog.load(gameDirectory + "\\locale\\" + lang + "\\LC_MESSAGES\\main.mo"))
        catalog.load(gameDirectory + "\\locale\\" + lang + "\\main.mo");
    addNative(labels.damagePrefixes, "Damage vs");
    addNative(labels.damageExceptions, "Damage vs robots");
    addNative(labels.damageExceptions, "Damage vs humans");
    addNative(labels.damageExceptions, "Damage vs animals");
    addNative(labels.foodHeaders, "-Only for:");
    // Armour lists come from native equipment eligibility, including mod races.
    addNative(labels.wearableHeaders, "Wearable by:");
    addNative(labels.wearableHeaders, "Can be worn by:");
    labels.collapsedHint = "[" + keyName(toggleKey) + "] Expand races";
    labels.expandedHint = "[" + keyName(toggleKey) + "] Collapse races";
    labels.pagingHint = "[" + keyName(previousKey) + "/" + keyName(nextKey) + "]";
    log("Game " + KenshiLib::GetKenshiVersion().ToString() + ", base locale=" + lang);
    if (!hook("ToolTip::addLine", &ToolTip::addLine, addLineHook, &originalAddLine)) return;
    if (!hook("ToolTipInventory::setContent", &ToolTipInventory::_NV_setContent, setContentHook, &originalSetContent)) return;
    if (!hook("ToolTipInventory::show", &ToolTipInventory::_NV_show, showHook, &originalShow)) return;
    if (!hook("ToolTipInventory::update", &ToolTipInventory::_NV_update, updateHook, &originalUpdate)) return;
    if (!hook("ToolTip destructor", &ToolTip::_DESTRUCTOR, destroyHook, &originalDestroy)) return;
    if (!hook("InputHandler::keyDownEvent", &InputHandler::keyDownEvent, keyDownHook, &originalKeyDown)) return;
    if (!hook("InputHandler::keyUpEvent", &InputHandler::keyUpEvent, keyUpHook, &originalKeyUp)) return;
    if (!hook("ToolTip::notifyToolTip", &ToolTip::notifyToolTip, notifyHook, &originalNotify)) return;
    if (!hook("Armour::getTooltipData1", &Armour::_NV_getTooltipData1, armourDataHook, &originalArmourData)) return;
    enabled = true;
    log("Ready. Click Expand/Collapse and arrow buttons; " + keyName(toggleKey) + " and " + keyName(previousKey) + "/" + keyName(nextKey) + " are also available. Directional keyboard arrows are reserved for the game.");
}
