// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <stdint.h>

// Reads only the game's normal main.mo catalog. No translation mod dependency.
class NativeCatalog {
    std::map<std::string, std::string> messages;
    static uint32_t number(const std::vector<unsigned char>& b, size_t p, bool swap) {
        if (p > b.size() || b.size() - p < 4) return 0;
        if (swap) return (uint32_t(b[p]) << 24) | (uint32_t(b[p+1]) << 16) |
                         (uint32_t(b[p+2]) << 8) | uint32_t(b[p+3]);
        return uint32_t(b[p]) | (uint32_t(b[p+1]) << 8) |
               (uint32_t(b[p+2]) << 16) | (uint32_t(b[p+3]) << 24);
    }
    static bool slice(const std::vector<unsigned char>& b, uint32_t pos, uint32_t len, std::string& out) {
        if (pos > b.size() || len > b.size() - pos) return false;
        out.assign(reinterpret_cast<const char*>(&b[0]) + pos, len);
        return true;
    }
public:
    bool load(const std::string& path) {
        std::ifstream f(path.c_str(), std::ios::binary);
        if (!f) return false;
        f.seekg(0, std::ios::end);
        const std::streamoff length = f.tellg();
        if (length < 28 || length > 16 * 1024 * 1024) return false;
        f.seekg(0, std::ios::beg);
        std::vector<unsigned char> b(static_cast<size_t>(length));
        f.read(reinterpret_cast<char*>(&b[0]), static_cast<std::streamsize>(length));
        if (!f) return false;
        uint32_t magic = number(b, 0, false);
        if (magic != 0x950412de && magic != 0xde120495) return false;
        bool swap = magic == 0xde120495;
        uint32_t count = number(b, 8, swap), originals = number(b, 12, swap), translations = number(b, 16, swap);
        if (count > 100000 || originals > b.size() || translations > b.size() ||
            count > (b.size() - originals) / 8 || count > (b.size() - translations) / 8) return false;
        std::map<std::string, std::string> parsed;
        for (uint32_t i = 0; i < count; ++i) {
            std::string key, value;
            if (!slice(b, number(b, originals + i * 8 + 4, swap), number(b, originals + i * 8, swap), key) ||
                !slice(b, number(b, translations + i * 8 + 4, swap), number(b, translations + i * 8, swap), value)) return false;
            if (!key.empty() && !value.empty() && key.find('\0') == std::string::npos)
                parsed[key] = value.substr(0, value.find('\0'));
        }
        messages.swap(parsed);
        return true;
    }
    std::string get(const std::string& key) const {
        std::map<std::string, std::string>::const_iterator it = messages.find(key);
        return it == messages.end() ? key : it->second;
    }
};
