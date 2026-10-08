// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/Item.h>
#include <kenshi/RaceData.h>
#include <kenshi/util/StringPair.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

// Race-level equipment eligibility, using Kenshi's own RaceLimiter. This says
// which races can use this equipment slot; a particular character can still be
// unable to equip it because of lost limbs, an occupied slot, etc.
// Call on the game's UI thread after GameData has loaded.
namespace WearableRaces {

struct Result {
    bool applicable;
    bool ready;
    size_t considered;
    size_t rejectedSlot;
    size_t rejectedLimiter;
    size_t eligibleRaces;
    std::vector<std::string> names; // Unique displayed entries, sorted.
    Result() : applicable(false), ready(false), considered(0), rejectedSlot(0),
        rejectedLimiter(0), eligibleRaces(0) {}
};

inline bool nativeAnimalFlag(const GameData* race) {
    // AppearanceManager::getEditorData (Steam 1.0.65, 0x79CAD) reads this exact
    // GameData bool. It is not a field of RaceData and does not require a
    // CHARACTER template. The engine's missing-key default is false; find()
    // preserves that meaning without inserting a value into GameData.
    if (!race) return false;
    const auto found = race->bdata.find("animal");
    return found != race->bdata.end() && found->second;
}

inline bool supportedSlot(AttachSlot slot) {
    // Actual wearable sections created by CharacterHuman::setupInventorySections.
    // Unknown/custom engine slots are left to their owning plugin.
    return slot == ATTACH_HAT || slot == ATTACH_BODY || slot == ATTACH_LEGS ||
        slot == ATTACH_SHIRT || slot == ATTACH_BOOTS || slot == ATTACH_BELT ||
        slot == ATTACH_BACKPACK;
}

inline bool nativeSlotAvailable(const RaceData* race, AttachSlot slot, bool animal) {
    if (!race || !supportedSlot(slot)) return false;
    // CharacterAnimal creates backpack_attach, but no armour/shirt/boot/etc.
    if (animal) return slot == ATTACH_BACKPACK;
    if (slot == ATTACH_HAT) return !race->noHats;
    if (slot == ATTACH_SHIRT) return !race->noShirts;
    if (slot == ATTACH_BOOTS) return !race->noShoes;
    return true;
}

inline Result evaluate(GameData* equipment, AttachSlot slot) {
    Result result;
    // Armour includes hats, boots, shirts, belts, body armour and legwear.
    // Containers have additional behaviour and are deliberately handled by
    // their own native tooltip rather than cast to Armour here.
    result.applicable = equipment && equipment->type == ARMOUR && supportedSlot(slot);
    if (!result.applicable || !ou) return result;
    const auto& categories = ou->gamedata.gamedataCatSID;
    const auto races = categories.find(RACE);
    if (races == categories.end() || races->second.empty()) return result;
    RaceLimiter* const limiter = RaceLimiter::getSingleton();
    if (!limiter) return result;
    // addLimit is the same idempotent native cache preparation used by
    // RootObjectFactory::_chooseClothingItemFromList before canEquip. The
    // native cache owns interpretation of "races" and "races exclude".
    if (limiter->limits.find(equipment) == limiter->limits.end())
        limiter->addLimit(equipment);
    for (auto it = races->second.begin(); it != races->second.end(); ++it) {
        GameData* const data = it->second;
        if (!data || data->name.empty()) continue;
        RaceData* const race = RaceData::getRaceData(data);
        if (!race) continue;
        ++result.considered;
        const bool animal = nativeAnimalFlag(data);
        if (!nativeSlotAvailable(race, slot, animal)) {
            ++result.rejectedSlot;
            continue;
        }
        // Calls the native virtual overload, also honouring another plugin's
        // replacement of this public eligibility method.
        if (!limiter->canEquip(equipment, race, animal)) {
            ++result.rejectedLimiter;
            continue;
        }
        ++result.eligibleRaces;
        result.names.push_back(data->name);
    }
    std::sort(result.names.begin(), result.names.end());
    result.names.erase(std::unique(result.names.begin(), result.names.end()), result.names.end());
    result.ready = result.considered != 0;
    return result;
}

// Cached only by immutable loaded equipment GameData and slot, never by Item*.
// Nothing from the temporary tooltip/widget or an inventory is retained.
struct Cache {
    GameWorld* world;
    size_t raceCount;
    std::map<std::pair<GameData*, int>, Result> entries;
    Cache() : world(NULL), raceCount(0) {}
};

inline Cache& cache() { static Cache value; return value; }
inline void resetCache() { cache().entries.clear(); cache().world = NULL; cache().raceCount = 0; }

inline Result get(Item* item) {
    if (!item || !ou) return Result();
    GameData* const data = item->getGameData();
    const AttachSlot slot = item->slotType;
    const auto& categories = ou->gamedata.gamedataCatSID;
    const auto races = categories.find(RACE);
    const size_t count = races == categories.end() ? 0 : races->second.size();
    Cache& state = cache();
    if (state.world != ou || state.raceCount != count) {
        state.entries.clear(); state.world = ou; state.raceCount = count;
    }
    const std::pair<GameData*, int> key(data, static_cast<int>(slot));
    const auto found = state.entries.find(key);
    if (found != state.entries.end()) return found->second;
    const Result result = evaluate(data, slot);
    if (result.ready) state.entries.insert(std::make_pair(key, result));
    return result;
}

inline bool append(Item* item, Ogre::vector<StringPair>::type& rows,
    const std::string& title = "Wearable by:", const std::string& colour = "#140806",
    const std::string& noneText = "None") {
    const Result result = get(item);
    if (!result.applicable || !result.ready) return false;
    if (result.names.empty()) {
        rows.push_back(StringPair(colour + "-" + title, colour + noneText));
        return true;
    }
    // Separate rows avoid comma ambiguity, preserve exact native race names,
    // and feed RaceSections' existing multiline-list parser.
    rows.push_back(StringPair(colour + "-" + title, ""));
    for (size_t i = 0; i < result.names.size(); ++i)
        rows.push_back(StringPair("", colour + result.names[i]));
    return true;
}

} // namespace WearableRaces
