#ifndef COMPACT_RACE_SECTIONS_H
#define COMPACT_RACE_SECTIONS_H

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

// Pure C++98: no game pointers, UI calls, or changes to item/race data.
namespace RaceSections {

struct Row {
    std::string left;
    std::string right;
    // A nonnegative index means the complete original StringPair can be reused.
    // Synthetic and changed rows have -1, so stat comparison values are not lost.
    int sourceIndex;
    Row() : sourceIndex(-1) {}
    Row(const std::string& a, const std::string& b, int index = -1)
        : left(a), right(b), sourceIndex(index) {}
};

struct Labels {
    std::vector<std::string> foodHeaders;
    std::vector<std::string> wearableHeaders;
    std::vector<std::string> damagePrefixes;
    std::vector<std::string> damageExceptions;
    // Include loaded RACE and RACE_GROUP display names. Food combines complete
    // groups, so the UI intentionally counts entries rather than unique races.
    std::vector<std::string> raceNames;
    std::string damageTitle;
    std::string entriesWord;
    std::string hiddenText;
    std::string collapsedHint;
    std::string expandedHint;
    std::string pagingHint;

    Labels() : damageTitle("Damage vs races"), entriesWord("entries"),
        hiddenText("race list hidden"), collapsedHint("[F8] Expand races"),
        expandedHint("[F8] Collapse races"), pagingHint("[PgUp/PgDn]") {
        foodHeaders.push_back("Only for:");
        foodHeaders.push_back("Edible by:");
        wearableHeaders.push_back("Wearable by:");
        damagePrefixes.push_back("Damage vs");
        damageExceptions.push_back("Damage vs robots");
        damageExceptions.push_back("Damage vs humans");
        damageExceptions.push_back("Damage vs animals");
    }
};

struct Result {
    std::vector<Row> rows;
    std::size_t raceCount; // Displayed list entries; may include race groups.
    std::size_t sectionCount;
    std::size_t page;
    std::size_t pageCount;
    bool hasSections;
    bool countExact;
    Result() : raceCount(0), sectionCount(0), page(0), pageCount(1),
        hasSections(false), countExact(true) {}
};

namespace Detail {

inline bool space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

inline bool colourAt(const std::string& s, std::size_t pos) {
    if (pos >= s.size() || s[pos] != '#' || s.size() - pos < 7) return false;
    for (std::size_t i = pos + 1; i < pos + 7; ++i)
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}

inline std::string stripColours(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size();) {
        // MyGUI uses ## for a literal hash.
        if (i + 1 < s.size() && s[i] == '#' && s[i + 1] == '#') {
            out += '#'; i += 2;
        } else if (colourAt(s, i)) i += 7;
        else out += s[i++];
    }
    return out;
}

inline std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && space(s[a])) ++a;
    while (b > a && space(s[b - 1])) --b;
    return s.substr(a, b - a);
}

inline std::string normal(const std::string& s) {
    std::string out = trim(stripColours(s));
    if (!out.empty() && out[0] == '-') out = trim(out.substr(1));
    return out;
}

inline std::string key(const std::string& s) {
    std::string out = normal(s);
    if (!out.empty() && out[out.size() - 1] == ':')
        out = trim(out.substr(0, out.size() - 1));
    return out;
}

inline bool exactKey(const std::string& s, const std::vector<std::string>& keys) {
    const std::string n = key(s);
    for (std::size_t i = 0; i < keys.size(); ++i)
        if (!keys[i].empty() && n == key(keys[i])) return true;
    return false;
}

inline std::string number(std::size_t value) {
    std::ostringstream out; out << value; return out.str();
}

inline std::string firstColour(const std::string& s) {
    std::size_t p = 0;
    while (p < s.size() && space(s[p])) ++p;
    return colourAt(s, p) ? s.substr(p, 7) : std::string();
}

inline bool hasName(const std::string& s, const Labels& labels) {
    const std::string visible = trim(s);
    if (visible.empty()) return false;
    for (std::size_t i = 0; i < labels.raceNames.size(); ++i)
        // Native GameData names may contain boundary whitespace (for example
        // "Skeleton No-Head MkII "). Match the same visible spelling on both
        // sides; original rows and dictionary strings remain byte-for-byte.
        if (visible == trim(labels.raceNames[i])) return true;
    return false;
}

// Parse using actual display names, never a blind comma count. Each suffix is
// resolved once; ambiguous parses are rejected instead of inventing a count.
inline bool parseNames(const std::string& raw, const Labels& labels,
    std::vector<std::string>& names) {
    const std::string text = trim(stripColours(raw));
    names.clear();
    if (text.empty() || labels.raceNames.empty()) return false;
    std::vector<std::string> visibleNames;
    for (std::size_t n = 0; n < labels.raceNames.size(); ++n)
        visibleNames.push_back(trim(labels.raceNames[n]));
    // Parallel arrays keep this compatible with the game's C++98 toolchain.
    std::vector<unsigned> ways(text.size() + 1, 0);
    std::vector<std::size_t> next(text.size() + 1, 0);
    std::vector<std::size_t> chosen(text.size() + 1, 0);
    ways[text.size()] = 1;
    for (std::size_t position = text.size(); position-- > 0;) {
        for (std::size_t n = 0; n < labels.raceNames.size(); ++n) {
            const std::string& name = visibleNames[n];
            if (name.empty() || name.size() > text.size() - position ||
                text.compare(position, name.size(), name) != 0) continue;
            // Duplicate display names must not make an otherwise exact parse ambiguous.
            bool duplicate = false;
            for (std::size_t j = 0; j < n; ++j)
                if (visibleNames[j] == name) { duplicate = true; break; }
            if (duplicate) continue;
            std::size_t end = position + name.size();
            bool newline = false;
            while (end < text.size() && space(text[end])) {
                if (text[end] == '\r' || text[end] == '\n') newline = true;
                ++end;
            }
            if (end != text.size()) {
                if (text[end] == ',') {
                    ++end;
                    while (end < text.size() && space(text[end])) ++end;
                    if (end == text.size()) continue;
                } else if (!newline) continue;
            }
            if (ways[end] == 0) continue;
            if (ways[position] == 0) { next[position] = end; chosen[position] = n; }
            ways[position] = std::min(2U, ways[position] + ways[end]);
        }
    }
    if (ways[0] != 1) return false;
    for (std::size_t pos = 0; pos < text.size(); pos = next[pos])
        names.push_back(labels.raceNames[chosen[pos]]);
    return !names.empty();
}

inline bool damageRow(const Row& row, const Labels& labels) {
    if (trim(stripColours(row.right)).empty()) return false;
    const std::string caption = normal(row.left);
    for (std::size_t i = 0; i < labels.damageExceptions.size(); ++i)
        if (caption == normal(labels.damageExceptions[i])) return false;
    for (std::size_t i = 0; i < labels.damagePrefixes.size(); ++i) {
        const std::string prefix = normal(labels.damagePrefixes[i]);
        if (prefix.empty() || caption.size() <= prefix.size() ||
            caption.compare(0, prefix.size(), prefix) != 0 ||
            !space(caption[prefix.size()])) continue;
        const std::string race = trim(caption.substr(prefix.size()));
        if (!race.empty() && (labels.raceNames.empty() || hasName(race, labels)))
            return true;
    }
    return false;
}

struct Section {
    std::size_t begin;
    std::size_t end;
    bool list;
    bool exact;
    bool hasHeader;
    Row header;
    std::vector<std::string> names;
    std::vector<Row> entries;
    Section() : begin(0), end(0), list(false), exact(true), hasHeader(false) {}
    std::size_t size() const { return list ? names.size() : entries.size(); }
};

inline std::vector<Section> sections(const std::vector<Row>& rows, const Labels& labels) {
    std::vector<Section> out;
    for (std::size_t i = 0; i < rows.size();) {
        const bool listHeader = exactKey(rows[i].left, labels.foodHeaders) ||
            exactKey(rows[i].left, labels.wearableHeaders);
        if (listHeader && !trim(stripColours(rows[i].right)).empty()) {
            Section s; s.begin = i; s.end = i + 1; s.list = true; s.hasHeader = true;
            s.header = rows[i];
            s.exact = parseNames(rows[i].right, labels, s.names);
            out.push_back(s); ++i;
        } else if (listHeader) {
            // Some UI plugins render one race per row. Only consume rows whose
            // complete visible text is a loaded race/group name, never stats.
            Section s; s.begin = i; s.end = i + 1; s.hasHeader = true;
            s.header = rows[i];
            while (s.end < rows.size()) {
                const Row& candidate = rows[s.end];
                // Names can themselves begin with a hyphen. Only header/stat
                // matching removes a native leading '-' decoration.
                const std::string left = trim(stripColours(candidate.left));
                const std::string right = trim(stripColours(candidate.right));
                if (!((left.empty() && hasName(right, labels)) ||
                    (right.empty() && hasName(left, labels)))) break;
                s.entries.push_back(candidate); ++s.end;
            }
            if (!s.entries.empty()) { out.push_back(s); i = s.end; }
            else ++i;
        } else if (damageRow(rows[i], labels)) {
            Section s; s.begin = i; s.list = false;
            s.header = Row(firstColour(rows[i].left) + labels.damageTitle, "");
            while (i < rows.size() && damageRow(rows[i], labels))
                s.entries.push_back(rows[i++]);
            s.end = i; out.push_back(s);
        } else ++i;
    }
    return out;
}

inline std::string namesForPage(const Section& section, std::size_t begin, std::size_t end) {
    std::string out = firstColour(section.header.right);
    for (std::size_t i = begin; i < end; ++i) {
        // One entry per visual line keeps long modded names readable and bounds
        // the height by the configured page size. Original layout is untouched
        // whenever the complete section fits on the current page.
        if (i != begin) out += "\n";
        out += section.names[i];
    }
    return out;
}

} // namespace Detail

// A shared page budget limits all selected race entries, rather than eight per
// section. Unknown formats remain fully available in expanded mode.
inline Result transform(const std::vector<Row>& rows, const Labels& labels,
    bool expanded, std::size_t requestedPage = 0, std::size_t pageSize = 8) {
    Result result;
    const std::vector<Detail::Section> sections = Detail::sections(rows, labels);
    result.sectionCount = sections.size();
    result.hasSections = !sections.empty();
    for (std::size_t i = 0; i < sections.size(); ++i) {
        result.raceCount += sections[i].size();
        if (!sections[i].exact) result.countExact = false;
    }
    if (pageSize && result.raceCount)
        result.pageCount = 1 + (result.raceCount - 1) / pageSize;
    result.page = std::min(requestedPage, result.pageCount - 1);
    const std::size_t first = pageSize ? result.page * pageSize : 0;
    const std::size_t last = pageSize ? std::min(result.raceCount, first +
        std::min(pageSize, result.raceCount - first)) : result.raceCount;
    std::size_t source = 0, ordinal = 0;
    for (std::size_t i = 0; i < sections.size(); ++i) {
        const Detail::Section& s = sections[i];
        while (source < s.begin) result.rows.push_back(rows[source++]);
        const std::size_t count = s.size();
        if (!expanded) {
            Row summary = s.header; summary.sourceIndex = -1;
            summary.right = s.exact ? Detail::number(count) + " " + labels.entriesWord : labels.hiddenText;
            result.rows.push_back(summary);
        } else if (!s.exact || (first <= ordinal && last >= ordinal + count)) {
            // Exact original bytes and source indices are kept when unpaged.
            for (std::size_t p = s.begin; p < s.end; ++p) result.rows.push_back(rows[p]);
        } else {
            const std::size_t localBegin = first > ordinal ? std::min(count, first - ordinal) : 0;
            const std::size_t localEnd = last > ordinal ? std::min(count, last - ordinal) : 0;
            Row summary = s.header; summary.sourceIndex = -1;
            if (localBegin < localEnd) {
                if (s.list) {
                    summary.right = Detail::namesForPage(s, localBegin, localEnd);
                    result.rows.push_back(summary);
                } else {
                    if (s.hasHeader) result.rows.push_back(s.header);
                    for (std::size_t p = localBegin; p < localEnd; ++p)
                        result.rows.push_back(s.entries[p]);
                }
            } else {
                summary.right = Detail::number(count) + " " + labels.entriesWord;
                result.rows.push_back(summary);
            }
        }
        ordinal += count;
        source = s.end;
    }
    while (source < rows.size()) result.rows.push_back(rows[source++]);
    if (result.hasSections) {
        std::string hint = expanded ? labels.expandedHint : labels.collapsedHint;
        if (expanded && result.pageCount > 1)
            hint += "  " + labels.pagingHint + " " + Detail::number(result.page + 1) + "/" + Detail::number(result.pageCount);
        result.rows.push_back(Row("#A0A0A0" + hint, ""));
    }
    return result;
}

} // namespace RaceSections
#endif
