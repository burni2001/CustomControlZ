#pragma once
// Minimal INI file reader/writer for macOS.
// Uses the same [Section] / Key=Value format as the Windows build so the same
// settings.ini works on both platforms.
#ifndef _WIN32

#include <string>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdio>

class IniConfig {
public:
    // Load from file. Returns true on success (missing file is not an error).
    bool load(const std::string& path) {
        path_ = path;
        data_.clear();
        std::ifstream f(path);
        if (!f.is_open()) return true;  // no file yet — defaults will be used

        std::string section;
        std::string line;
        while (std::getline(f, line)) {
            trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line.front() == '[' && line.back() == ']') {
                section = line.substr(1, line.size() - 2);
                trim(section);
                continue;
            }
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            trim(key); trim(val);
            // Strip inline comments
            auto sc = val.find(';');
            if (sc != std::string::npos) { val = val.substr(0, sc); trim(val); }
            data_[section + '\0' + key] = val;
        }
        return true;
    }

    // Persist a string value and save to disk.
    void writeString(const std::string& section, const std::string& key, const std::string& value) {
        data_[section + '\0' + key] = value;
        save();
    }

    // Persist an integer value.
    void writeInt(const std::string& section, const std::string& key, int value) {
        writeString(section, key, std::to_string(value));
    }

    // Read a string value. Returns defaultVal if not found.
    std::string getString(const std::string& section, const std::string& key,
                          const std::string& defaultVal = "") const {
        auto it = data_.find(section + '\0' + key);
        return (it != data_.end()) ? it->second : defaultVal;
    }

    // Read an integer value. Returns defaultVal if not found or not parseable.
    int getInt(const std::string& section, const std::string& key, int defaultVal = 0) const {
        auto it = data_.find(section + '\0' + key);
        if (it == data_.end()) return defaultVal;
        try { return std::stoi(it->second); }
        catch (...) { return defaultVal; }
    }

    // Helpers for wide-string section/key names (used by game profile INI keys)
    int getInt(const wchar_t* section, const wchar_t* key, int defaultVal = 0) const {
        return getInt(wstr(section), wstr(key), defaultVal);
    }
    void writeInt(const wchar_t* section, const wchar_t* key, int value) {
        writeInt(wstr(section), wstr(key), value);
    }
    std::string getString(const wchar_t* section, const wchar_t* key,
                          const std::string& def = "") const {
        return getString(wstr(section), wstr(key), def);
    }
    void writeString(const wchar_t* section, const wchar_t* key, const std::string& value) {
        writeString(wstr(section), wstr(key), value);
    }

private:
    std::string path_;
    std::unordered_map<std::string, std::string> data_;  // "Section\0Key" → "Value"

    // Convert wchar_t* to UTF-8 string (section/key names are ASCII so this is safe)
    static std::string wstr(const wchar_t* ws) {
        if (!ws) return {};
        std::string out;
        while (*ws) { out += static_cast<char>(*ws & 0x7F); ++ws; }
        return out;
    }

    static void trim(std::string& s) {
        auto notSpace = [](unsigned char c){ return !std::isspace(c); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    }

    // Re-write the whole file from the in-memory map.
    void save() const {
        if (path_.empty()) return;

        // Collect sections and their key-value pairs in insertion order isn't
        // guaranteed with unordered_map, so we rebuild a sorted structure.
        std::map<std::string, std::map<std::string, std::string>> sections;
        for (auto& [composite, val] : data_) {
            auto sep = composite.find('\0');
            if (sep == std::string::npos) continue;
            sections[composite.substr(0, sep)][composite.substr(sep + 1)] = val;
        }

        std::ofstream f(path_, std::ios::trunc);
        if (!f.is_open()) return;
        for (auto& [sec, kvs] : sections) {
            f << '[' << sec << "]\n";
            for (auto& [k, v] : kvs) f << k << '=' << v << '\n';
            f << '\n';
        }
    }
};

// Global config instance — initialised in main_mac.cpp
extern IniConfig g_config;

#endif // !_WIN32
