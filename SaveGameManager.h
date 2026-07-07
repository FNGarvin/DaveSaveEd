// SaveGameManager.h
//
// Copyright (c) 2025 FNGarvin (184324400+FNGarvin@users.noreply.github.com)
// All rights reserved.
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
#pragma once

#include <string>
#include <vector>
#include <filesystem>

// --- JSON ORDERING PATCH ---
// Requires fifo_map.hpp in the include path to preserve insertion order
#include "json.hpp"
#include "fifo_map.hpp" 

// Workaround to adapt fifo_map to the interface expected by nlohmann::basic_json
template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;

// Define a new JSON type that preserves insertion order
using ordered_json = nlohmann::basic_json<my_workaround_fifo_map>;
// ---------------------------

// Forward declaration
struct sqlite3;

class SaveGameManager {
public:
    SaveGameManager();
    ~SaveGameManager();

    // File Operations
    bool LoadSaveFile(const std::string& filepath);
    bool WriteSaveFile(std::string& out_backup_filepath);
    static std::filesystem::path GetDefaultSaveGameDirectoryAndLatestFile(std::string& latestSaveFileName);

    // Configuration
    void SetDebugLogging(bool enabled);

    // Getters
    long long GetGold() const;
    long long GetBei() const;
    long long GetArtisansFlame() const;
    long long GetFollowerCount() const;

    // Setters
    void SetGold(long long value);
    void SetBei(long long value);
    void SetArtisansFlame(long long value);
    void SetFollowerCount(long long value);

    // Jungle DLC
    bool IsJungleDLCInstalled() const;
    long long GetJungleGold() const;
    long long GetJungleArtisansFlame() const;
    void SetJungleGold(long long value);
    void SetJungleArtisansFlame(long long value);

    // Ingredient Modifications
    void MaxOwnIngredients(sqlite3* db);
    void MaxAllIngredients(sqlite3* db);

    // State
    bool IsSaveFileLoaded() const { return m_isSaveFileLoaded; }
    std::string GetCurrentFilePath() const { return m_currentSaveFilePath; }

private:
    // Helpers
    std::string DecodeAndBypass(const std::vector<unsigned char>& encrypted_bytes);
    std::vector<unsigned char> EncodeWithBypass(const std::string& utf8_json_string);
    
    // Robust Search
    bool FindFieldDetails(const std::vector<unsigned char>& encrypted_bytes, size_t start_pos, 
                          size_t& out_field_len, size_t& out_resync_key_idx);

    std::vector<unsigned char> XorBytes(const std::vector<unsigned char>& data_bytes, size_t key_start_index);
    std::string BytesToHex(const std::vector<unsigned char>& bytes);
    std::vector<unsigned char> HexToBytes(const std::string& hex);

    // Verification Test
    void PerformRoundTripTest(const std::vector<unsigned char>& original_bytes, const std::string& original_path);

    // Members
    bool m_isSaveFileLoaded;
    bool m_debugLoggingEnabled;
    std::string m_currentSaveFilePath;
    
    // --- CHANGED: Use ordered_json instead of nlohmann::json ---
    ordered_json m_saveData; 
    
    const std::string XOR_KEY = "GameData";
};
//END OF SaveGameManager.h
