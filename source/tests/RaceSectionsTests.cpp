#include "../RaceSections.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

using RaceSections::Labels;
using RaceSections::Result;
using RaceSections::Row;
using RaceSections::transform;

static bool same(const Row& a, const Row& b) {
    return a.left == b.left && a.right == b.right && a.sourceIndex == b.sourceIndex;
}

static bool contains(const std::vector<Row>& rows, const Row& wanted) {
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (same(rows[i], wanted)) return true;
    return false;
}

static Labels names() {
    Labels labels;
    labels.raceNames.push_back("Human");
    labels.raceNames.push_back("Shek");
    labels.raceNames.push_back("Hive");
    labels.raceNames.push_back("Hive Prince");
    labels.raceNames.push_back("Skeleton");
    labels.raceNames.push_back("Scorchlander");
    labels.raceNames.push_back("Cannibal");
    labels.raceNames.push_back("Fishman");
    labels.raceNames.push_back("Southern Hive");
    labels.raceNames.push_back("Fogman");
    labels.raceNames.push_back("Queen, Ancient");
    return labels;
}

static void preservesUnrelated() {
    std::vector<Row> input;
    input.push_back(Row("#FFFFFFSword", "", 0));
    input.push_back(Row("Damage vs robots", "+50%", 1));
    input.push_back(Row("Damage vs humans", "-10%", 2));
    input.push_back(Row("Damage vs animals", "+20%", 3));
    input.push_back(Row("Description", "Only for: display text, not a race section", 4));
    Result result = transform(input, names(), false);
    assert(!result.hasSections && result.raceCount == 0);
    assert(result.rows.size() == input.size());
    for (std::size_t i = 0; i < input.size(); ++i) assert(same(input[i], result.rows[i]));
}

static void foodRoundTrip() {
    std::vector<Row> input;
    input.push_back(Row("#FFFFFFRaw meat", "", 0));
    input.push_back(Row("#CFCFCF-Only for:", "#A0A0A0Human, Shek, \nHive", 1));
    input.push_back(Row("#CFCFCF-Nutrition", "#FFFFFF15 nu", 2));
    Result collapsed = transform(input, names(), false);
    assert(collapsed.hasSections && collapsed.countExact);
    assert(collapsed.raceCount == 3 && collapsed.sectionCount == 1);
    assert(collapsed.rows[1].right == "3 entries" && collapsed.rows[1].sourceIndex == -1);
    assert(same(collapsed.rows[0], input[0]) && same(collapsed.rows[2], input[2]));
    Result expanded = transform(input, names(), true);
    assert(expanded.rows.size() == input.size() + 1);
    for (std::size_t i = 0; i < input.size(); ++i) assert(same(expanded.rows[i], input[i]));
}

static void namesWithCommasAndDuplicateDisplayNames() {
    Labels labels = names();
    labels.raceNames.push_back("Human");
    std::vector<Row> rows;
    rows.push_back(Row("-Only for:", "Human, \nQueen, Ancient, Shek", 4));
    Result result = transform(rows, labels, false);
    assert(result.countExact && result.raceCount == 3);
    result = transform(rows, labels, true, 1, 1);
    assert(result.pageCount == 3 && result.page == 1);
    assert(result.rows[0].right == "Queen, Ancient");
}

static void ambiguousAndUnknownNamesNeverInventCounts() {
    Labels labels;
    labels.raceNames.push_back("A");
    labels.raceNames.push_back("B");
    labels.raceNames.push_back("A, B");
    std::vector<Row> rows;
    rows.push_back(Row("-Only for:", "A, B", 7));
    Result result = transform(rows, labels, false);
    assert(result.hasSections && !result.countExact && result.raceCount == 0);
    assert(result.rows[0].right == labels.hiddenText);
    result = transform(rows, labels, true);
    assert(same(result.rows[0], rows[0]));
    rows[0].right = "Unknown Modded Race, Human";
    result = transform(rows, names(), true, 99, 1);
    assert(!result.countExact && same(result.rows[0], rows[0]));
}

static void wearableListAndLongPagination() {
    std::vector<Row> rows;
    rows.push_back(Row("Wearable by:", "Human, Shek, Hive, Hive Prince, Skeleton, Scorchlander, Cannibal, Fishman, Southern Hive, Fogman", 3));
    rows.push_back(Row("Acid resistance", "50%", 4));
    Result result = transform(rows, names(), true, 1, 8);
    assert(result.raceCount == 10 && result.pageCount == 2 && result.page == 1);
    assert(result.rows[0].right == "Southern Hive\nFogman");
    assert(same(result.rows[1], rows[1]));
    result = transform(rows, names(), true, 900, 8);
    assert(result.page == 1);
    result = transform(rows, names(), true, 0, 0);
    assert(result.pageCount == 1 && same(result.rows[0], rows[0]));
}

static void translatedWeaponPrefixAndGenericExceptions() {
    Labels labels = names();
    labels.damagePrefixes.push_back("Dano a");
    labels.damageExceptions.push_back("Dano a humanos");
    std::vector<Row> rows;
    rows.push_back(Row("#FFDDDD-Dano a humanos", "#00FF00+10%", 0));
    rows.push_back(Row("#FFFFFF-Dano a Shek", "#00FF00+15%", 1));
    rows.push_back(Row("#FFFFFF-Dano a Skeleton", "#FF0000-25%", 2));
    rows.push_back(Row("#FFFFFF-Weight", "#FFFFFF2 kg", 3));
    Result result = transform(rows, labels, false);
    assert(result.raceCount == 2 && result.sectionCount == 1);
    assert(same(result.rows[0], rows[0]));
    assert(contains(result.rows, rows[3]));
    result = transform(rows, labels, true, 1, 1);
    assert(contains(result.rows, rows[2]) && !contains(result.rows, rows[1]));
    assert(contains(result.rows, rows[0]) && contains(result.rows, rows[3]));
}

static void globalPageBudgetAndSafety() {
    Labels labels = names();
    std::vector<Row> rows;
    rows.push_back(Row("Damage vs Human", "+20%", 0));
    rows.push_back(Row("Damage vs Shek", "-10%", 1));
    rows.push_back(Row("Wearable by:", "Human, Shek, Hive", 2));
    rows.push_back(Row("Damage vs ordinary description", "unrelated", 3));
    Result result = transform(rows, labels, true, 1, 2);
    assert(result.raceCount == 5 && result.pageCount == 3);
    assert(!contains(result.rows, rows[0]) && !contains(result.rows, rows[1]));
    assert(result.rows[1].right == "Human\nShek");
    assert(contains(result.rows, rows[3]));
    rows.clear();
    rows.push_back(Row("-Only for:", "", 12));
    result = transform(rows, labels, false);
    assert(!result.hasSections && same(result.rows[0], rows[0]));
}

static void pluginWearableRowsAndNewlineLists() {
    std::vector<Row> rows;
    rows.push_back(Row("#DDDDDDWearable by:", "", 0));
    rows.push_back(Row("", "#FFFFFFHuman", 1));
    rows.push_back(Row("#FFFFFFShek", "", 2));
    rows.push_back(Row("Weight", "1 kg", 3));
    Result result = transform(rows, names(), false);
    assert(result.raceCount == 2 && result.sectionCount == 1);
    assert(result.rows[0].right == "2 entries");
    assert(contains(result.rows, rows[3]));
    result = transform(rows, names(), true, 1, 1);
    assert(contains(result.rows, rows[0]) && contains(result.rows, rows[2]));
    assert(!contains(result.rows, rows[1]) && contains(result.rows, rows[3]));
    rows.clear();
    rows.push_back(Row("Wearable by:", "Human\r\nShek\nHive", 4));
    result = transform(rows, names(), false);
    assert(result.countExact && result.raceCount == 3);
    result = transform(rows, names(), true, 1, 1);
    assert(result.rows[0].right == "Shek");
}

static void nativeWearableTrailingSpaceRegression() {
    // Actual native names from the Rag Loincloth runtime capture. The space
    // after MkII formerly ended the section at 26 and leaked all three tails.
    const char* nativeNames[] = {
        "----edad", "Alpha Fishman", "Cannibal", "Cannibal Skav", "Deadhive Prince",
        "Deadhive Soldier", "Deadhive Worker", "Fishman", "Flayed Man", "Flayed Woman",
        "Greenlander", "Hive Prince", "Hive Prince South Hive", "Hive Queen",
        "Hive Soldier Drone", "Hive Soldier Drone South Hive", "Hive Worker Drone",
        "Hive Worker Drone South Hive", "King Fishman", "P4 Unit", "Scorchlander",
        "Screamer MkI", "Shek", "Skeleton", "Skeleton Log-Head MKII",
        "Skeleton MKII Screamer", "Skeleton No-Head MkII ", "Skeleton P4MkII", "Soldierbot"
    };
    Labels labels;
    std::vector<Row> rows;
    rows.push_back(Row("#492620Rag Loincloth", "", 0));
    rows.push_back(Row("#140806-Wearable by:", "", 4));
    for (std::size_t i = 0; i < 29; ++i) {
        labels.raceNames.push_back(nativeNames[i]);
        rows.push_back(Row("", std::string("#140806") + nativeNames[i], static_cast<int>(i + 5)));
    }
    rows.push_back(Row("#444444[No Armour Coverage]", "", 34));
    const Result collapsed = transform(rows, labels, false, 0, 2);
    assert(collapsed.sectionCount == 1 && collapsed.raceCount == 29);
    assert(collapsed.rows.size() == 4 && collapsed.rows[1].right == "29 entries");
    assert(contains(collapsed.rows, rows.back()));
    for (std::size_t i = 2; i < rows.size() - 1; ++i)
        assert(!contains(collapsed.rows, rows[i]));
    const Result unpaged = transform(rows, labels, true, 0, 0);
    for (std::size_t i = 0; i < rows.size(); ++i) assert(same(unpaged.rows[i], rows[i]));
    // Every original row appears on exactly one page, preserving its trailing
    // spaces, colour and sourceIndex; unrelated armour text stays on all pages.
    for (std::size_t i = 0; i < 29; ++i) {
        std::size_t appearances = 0;
        for (std::size_t p = 0; p < 15; ++p) {
            const Result paged = transform(rows, labels, true, p, 2);
            assert(paged.pageCount == 15 && contains(paged.rows, rows.back()));
            if (contains(paged.rows, rows[i + 2])) ++appearances;
        }
        assert(appearances == 1);
    }
}

static void whitespaceNamesAcrossTooltipFormats() {
    Labels labels;
    labels.raceNames.push_back(" Skeleton No-Head MkII ");
    labels.raceNames.push_back("Queen, Ancient ");
    labels.raceNames.push_back("----edad");
    labels.raceNames.push_back("Human");
    labels.raceNames.push_back("Human "); // Same visible name is not ambiguous.
    std::vector<Row> rows;
    rows.push_back(Row("-Only for:", "#140806Human, \n Skeleton No-Head MkII , Queen, Ancient ", 0));
    Result result = transform(rows, labels, false);
    assert(result.countExact && result.raceCount == 3);
    result = transform(rows, labels, true, 1, 1);
    assert(result.rows[0].right == "#140806 Skeleton No-Head MkII ");
    result = transform(rows, labels, true, 2, 1);
    assert(result.rows[0].right == "#140806Queen, Ancient ");
    rows.clear();
    rows.push_back(Row("#140806-Damage vs Skeleton No-Head MkII ", "+20%", 1));
    rows.push_back(Row("#140806-Damage vs Human", "-10%", 2));
    result = transform(rows, labels, false);
    assert(result.raceCount == 2);
    result = transform(rows, labels, true, 0, 0);
    assert(same(rows[0], result.rows[0]) && same(rows[1], result.rows[1]));
    rows.clear();
    rows.push_back(Row("Wearable by:", "", 3));
    rows.push_back(Row("#140806----edad", "", 4));
    rows.push_back(Row("", "#140806Queen, Ancient ", 5));
    rows.push_back(Row("Weight", "1 kg", 6));
    result = transform(rows, labels, false);
    assert(result.raceCount == 2 && contains(result.rows, rows.back()));
    assert(!contains(result.rows, rows[1]) && !contains(result.rows, rows[2]));
}

int main() {
    preservesUnrelated();
    foodRoundTrip();
    namesWithCommasAndDuplicateDisplayNames();
    ambiguousAndUnknownNamesNeverInventCounts();
    wearableListAndLongPagination();
    translatedWeaponPrefixAndGenericExceptions();
    globalPageBudgetAndSafety();
    pluginWearableRowsAndNewlineLists();
    nativeWearableTrailingSpaceRegression();
    whitespaceNamesAcrossTooltipFormats();
    std::cout << "RaceSections: 10 scenario groups passed.\n";
    return 0;
}
