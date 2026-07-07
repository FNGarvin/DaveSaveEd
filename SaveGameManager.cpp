// SaveGameManager.cpp
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
// Disclaimer: This project and its creators are not affiliated with Mintrocket, Nexon,
// or any other entities associated with the game "Dave the Diver." This is an independent
// fan-made tool.
//
#define NOMINMAX 

#include "SaveGameManager.h"
#include <fstream>       
#include <shlobj.h>      
#include <chrono>        
#include <iomanip>       
#include <sstream>       
#include <algorithm>     
#include "sqlite3.h"     
#include "Logger.h"      
// json.hpp is included via header
#include <vector>        
#include <map>           
#include <set>           
#include <string>        
#include <stdexcept>     
#include <filesystem>    
#include <regex>         

const long long SAVE_MAX_CURRENCY = 999999999LL;

static const std::string BYPASS_PREFIX = "BYPASSED_HEX::";

static const std::vector<unsigned char> TRIGGER_FARMANIMAL = {
    '"', 'F', 'a', 'r', 'm', 'A', 'n', 'i', 'm', 'a', 'l', '"', ':',
    '[', '{', '"', 'F', 'a', 'r', 'm', 'A', 'n', 'i', 'm', 'a', 'l',
    'I', 'D', '"', ':', '1', '1', '0', '9', '0', '0', '0', '1', ',',
    '"', 'N', 'a', 'm', 'e', '"', ':', '"'
};

static const std::vector<std::vector<unsigned char>> TROUBLESOME_TRIGGERS = {
    TRIGGER_FARMANIMAL
};
static const std::vector<unsigned char> END_MARKER = { '"', '}', ']', ',' };


SaveGameManager::SaveGameManager() : m_isSaveFileLoaded(false), m_debugLoggingEnabled(false) {
    LogMessage(LOG_INFO_LEVEL, "SaveGameManager initialized.");
}

SaveGameManager::~SaveGameManager() {
    LogMessage(LOG_INFO_LEVEL, "SaveGameManager shutting down.");
}

void SaveGameManager::SetDebugLogging(bool enabled) {
    m_debugLoggingEnabled = enabled;
    LogMessage(LOG_INFO_LEVEL, enabled ? "Debug logging ENABLED." : "Debug logging DISABLED.");
}

// --- Helpers ---
std::string SaveGameManager::BytesToHex(const std::vector<unsigned char>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char byte : bytes) ss << std::setw(2) << static_cast<int>(byte);
    return ss.str();
}

std::vector<unsigned char> SaveGameManager::HexToBytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    if (hex.length() % 2 != 0) return bytes;
    bytes.reserve(hex.length() / 2);
    for (size_t i = 0; i < hex.length(); i += 2) {
        try {
            bytes.push_back(static_cast<unsigned char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
        } catch (...) { bytes.clear(); return bytes; }
    }
    return bytes;
}

std::vector<unsigned char> SaveGameManager::XorBytes(const std::vector<unsigned char>& data_bytes, size_t key_start_index) {
    std::vector<unsigned char> output;
    output.reserve(data_bytes.size());
    size_t key_len = XOR_KEY.length();
    if (key_len == 0) return data_bytes; 
    for (size_t i = 0; i < data_bytes.size(); ++i) {
        output.push_back(data_bytes[i] ^ XOR_KEY[(key_start_index + i) % key_len]);
    }
    return output;
}

// --- SEARCH ---
bool SaveGameManager::FindFieldDetails(const std::vector<unsigned char>& encrypted_bytes, size_t start_pos,
                                      size_t& out_field_len, size_t& out_resync_key_idx)
{
    size_t key_len = XOR_KEY.length();
    if (start_pos >= encrypted_bytes.size()) return false;

    // Pass 1: Eager Match (Matches Python behavior)
    bool field_len_found = false;
    std::vector<unsigned char> slice_for_len_check(encrypted_bytes.begin() + start_pos, encrypted_bytes.end());

    for (size_t offset_pass1 = 0; offset_pass1 < key_len; ++offset_pass1) {
        size_t temp_key_idx = (start_pos + offset_pass1) % key_len;
        std::vector<unsigned char> decrypted_slice = XorBytes(slice_for_len_check, temp_key_idx);

        auto it = std::search(decrypted_slice.begin(), decrypted_slice.end(), END_MARKER.begin(), END_MARKER.end());
        if (it != decrypted_slice.end()) {
            out_field_len = std::distance(decrypted_slice.begin(), it);
            field_len_found = true;
            LogMessage(LOG_INFO_LEVEL, ("[DEBUG] Pass 1 Success. Offset: " + std::to_string(offset_pass1) + ". Len: " + std::to_string(out_field_len)).c_str());
            break; 
        }
    }

    if (!field_len_found) return false;

    // Pass 2: Verify next key
    size_t resync_pos = start_pos + out_field_len;
    if (resync_pos >= encrypted_bytes.size()) return false;

    size_t slice_len = std::min(static_cast<size_t>(50), encrypted_bytes.size() - resync_pos);
    std::vector<unsigned char> slice_for_offset_check(encrypted_bytes.begin() + resync_pos, 
                                                      encrypted_bytes.begin() + resync_pos + slice_len);

    for (size_t offset_pass2 = 0; offset_pass2 < key_len; ++offset_pass2) {
        size_t temp_key_idx = (resync_pos + offset_pass2) % key_len;
        std::vector<unsigned char> decrypted_slice = XorBytes(slice_for_offset_check, temp_key_idx);

        if (decrypted_slice.size() >= END_MARKER.size() &&
            std::equal(END_MARKER.begin(), END_MARKER.end(), decrypted_slice.begin())) 
        {
            out_resync_key_idx = temp_key_idx;
            LogMessage(LOG_INFO_LEVEL, ("[DEBUG] Pass 2 Success. Next Key: " + std::to_string(out_resync_key_idx)).c_str());
            return true;
        }
    }
    return false;
}

// --- DECODE (Standard) ---
std::string SaveGameManager::DecodeAndBypass(const std::vector<unsigned char>& encrypted_bytes) {
    std::vector<unsigned char> output_buffer;
    output_buffer.reserve(encrypted_bytes.size());
    size_t data_idx = 0;
    size_t key_idx = 0;
    size_t key_len = XOR_KEY.length();

    if (key_len == 0) return "";

    while (data_idx < encrypted_bytes.size()) {
        unsigned char decrypted_byte = encrypted_bytes[data_idx] ^ XOR_KEY[key_idx % key_len];
        output_buffer.push_back(decrypted_byte);

        unsigned char last_char = output_buffer.back();
        bool trigger_found = false;

        for (const auto& trigger : TROUBLESOME_TRIGGERS) {
            if (trigger.back() != last_char) continue;

            if (output_buffer.size() >= trigger.size()) {
                if (std::equal(trigger.begin(), trigger.end(), output_buffer.end() - trigger.size())) {
                    size_t field_start_pos = data_idx + 1;
                    size_t field_len = 0;
                    size_t next_valid_key_idx = 0;

                    if (FindFieldDetails(encrypted_bytes, field_start_pos, field_len, next_valid_key_idx)) {
                        std::vector<unsigned char> garbage_ciphertext;
                        if (field_start_pos + field_len <= encrypted_bytes.size()) {
                             garbage_ciphertext.assign(
                                encrypted_bytes.begin() + field_start_pos,
                                encrypted_bytes.begin() + field_start_pos + field_len
                            );
                        }

                        output_buffer.resize(output_buffer.size() - trigger.size());
                        output_buffer.insert(output_buffer.end(), trigger.begin(), trigger.end());

                        std::string bypass_string = BYPASS_PREFIX + BytesToHex(garbage_ciphertext) + ":" + std::to_string(next_valid_key_idx);
                        output_buffer.insert(output_buffer.end(), bypass_string.begin(), bypass_string.end());

                        data_idx = field_start_pos + field_len;
                        key_idx = (next_valid_key_idx + key_len - 1) % key_len;
                        data_idx--; 
                        trigger_found = true;
                    } 
                    break;
                }
            }
        }
        if (!trigger_found) { /* */ }
        data_idx++;
        key_idx++;
    }
    return std::string(output_buffer.begin(), output_buffer.end());
}

// --- ENCODE (Standard) ---
std::vector<unsigned char> SaveGameManager::EncodeWithBypass(const std::string& utf8_json_string) {
    std::vector<unsigned char> output_bytes;
    output_bytes.reserve(utf8_json_string.length()); 
    size_t key_len = XOR_KEY.length();
    size_t key_idx = 0;

    try {
        std::regex pattern(BYPASS_PREFIX + "([a-fA-F0-9]+):(\\d+)");
        auto it_begin = std::sregex_iterator(utf8_json_string.begin(), utf8_json_string.end(), pattern);
        auto it_end = std::sregex_iterator();
        std::string::const_iterator last_match_end = utf8_json_string.begin();

        for (std::sregex_iterator i = it_begin; i != it_end; ++i) {
            std::smatch match = *i;
            std::string::const_iterator match_start = utf8_json_string.begin() + match.position(0);

            std::string clean_part_str(last_match_end, match_start);
            std::vector<unsigned char> clean_part_bytes(clean_part_str.begin(), clean_part_str.end());
            std::vector<unsigned char> encrypted_clean = XorBytes(clean_part_bytes, key_idx);
            output_bytes.insert(output_bytes.end(), encrypted_clean.begin(), encrypted_clean.end());
            
            key_idx = (key_idx + clean_part_bytes.size()) % key_len;

            std::string hex_data = match.str(1);
            std::vector<unsigned char> garbage_ciphertext = HexToBytes(hex_data);
            
            output_bytes.insert(output_bytes.end(), garbage_ciphertext.begin(), garbage_ciphertext.end());
            
            size_t next_key_idx = std::stoul(match.str(2));
            key_idx = next_key_idx;

            last_match_end = utf8_json_string.begin() + match.position(0) + match.length(0);
        }

        std::string remaining_part_str(last_match_end, utf8_json_string.end());
        std::vector<unsigned char> remaining_part_bytes(remaining_part_str.begin(), remaining_part_str.end());
        std::vector<unsigned char> encrypted_remaining = XorBytes(remaining_part_bytes, key_idx);
        output_bytes.insert(output_bytes.end(), encrypted_remaining.begin(), encrypted_remaining.end());

    } catch (const std::exception& e) {
         LogMessage(LOG_ERROR_LEVEL, (std::string("Error during encode: ") + e.what()).c_str());
         throw; 
    }
    return output_bytes;
}

// --- ROUND TRIP TEST (Verification) ---
void SaveGameManager::PerformRoundTripTest(const std::vector<unsigned char>& original_bytes, const std::string& original_path) {
    if (!m_debugLoggingEnabled) return;

    LogMessage(LOG_INFO_LEVEL, "--- START BYPASS TEST (Saving to test_bypass.sav) ---");
    try {
        std::string decoded_str = DecodeAndBypass(original_bytes);
        
        // Manual Injection to verify translation works on modified data
        std::regex bei_regex("\"m_Bei\":\\d+");
        decoded_str = std::regex_replace(decoded_str, bei_regex, "\"m_Bei\":999999999");
        std::regex flame_regex("\"m_ChefFlame\":\\d+");
        decoded_str = std::regex_replace(decoded_str, flame_regex, "\"m_ChefFlame\":999999");

        std::vector<unsigned char> encoded_bytes = EncodeWithBypass(decoded_str);

        std::filesystem::path path(original_path);
        std::filesystem::path test_path = path.parent_path() / "test_bypass.sav";
        
        std::ofstream test_file(test_path, std::ios::binary | std::ios::trunc);
        if (test_file) {
            test_file.write(reinterpret_cast<const char*>(encoded_bytes.data()), encoded_bytes.size());
            test_file.close();
            LogMessage(LOG_INFO_LEVEL, ("[TEST SUCCESS] Wrote modified bypass test file."));
        } else {
            LogMessage(LOG_ERROR_LEVEL, "[TEST FAIL] Could not write test file.");
        }
    } catch (const std::exception& e) {
        LogMessage(LOG_ERROR_LEVEL, ("[TEST ERROR] Exception: " + std::string(e.what())).c_str());
    }
    LogMessage(LOG_INFO_LEVEL, "--- END BYPASS TEST ---");
}

// --- LoadSaveFile ---
bool SaveGameManager::LoadSaveFile(const std::string& filepath) {
    LogMessage(LOG_INFO_LEVEL, ("Loading: " + filepath).c_str());
    m_isSaveFileLoaded = false;
    
    m_saveData = ordered_json(); 

    try {
        std::ifstream input_file(filepath, std::ios::binary);
        if (!input_file) return false;
        std::vector<unsigned char> xor_encrypted_bytes((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
        input_file.close();

        // Run verification test if debug is enabled
        PerformRoundTripTest(xor_encrypted_bytes, filepath);

        // Decode
        std::string json_str_utf8 = DecodeAndBypass(xor_encrypted_bytes);
        
        // Debug Dump (Conditional)
        if (m_debugLoggingEnabled) {
            std::ofstream debug_file("decrypted_debug.json", std::ios::binary);
            if (debug_file) {
                debug_file.write(json_str_utf8.data(), json_str_utf8.size());
                LogMessage(LOG_INFO_LEVEL, "[DEBUG] Dumped decrypted JSON to local dir.");
            }
        }

        // Use ordered_json parser
        m_saveData = ordered_json::parse(json_str_utf8);
        m_currentSaveFilePath = filepath;
        m_isSaveFileLoaded = true;
        LogMessage(LOG_INFO_LEVEL, "Loaded successfully.");
        return true;

    } catch (const std::exception& e) {
        LogMessage(LOG_ERROR_LEVEL, ("Load Error: " + std::string(e.what())).c_str());
    }
    return false;
}

// --- WriteSaveFile ---
bool SaveGameManager::WriteSaveFile(std::string& out_backup_filepath) {
    out_backup_filepath.clear();
    if (!m_isSaveFileLoaded) return false;

    try {
        std::filesystem::path original_path(m_currentSaveFilePath);
        std::filesystem::path backup_dir;

        // Backup to TEMP directory
        WCHAR tempPathBuffer[MAX_PATH];
        DWORD length = GetTempPathW(MAX_PATH, tempPathBuffer);
        if (length == 0 || length > MAX_PATH) {
            LogMessage(LOG_ERROR_LEVEL, "Failed to get system temporary path. Fallback to local.");
            backup_dir = original_path.parent_path() / "backups";
        } else {
            std::filesystem::path base_temp_path = tempPathBuffer;
            backup_dir = base_temp_path / "DaveSaveEd_Backups";
        }
        std::filesystem::create_directories(backup_dir);
        
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf; localtime_s(&tm_buf, &now_c);
        std::stringstream ss; ss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
        
        std::string backup_filename = original_path.stem().string() + "_" + ss.str() + ".sav";
        std::filesystem::path backup_path = backup_dir / backup_filename;

        std::filesystem::copy(original_path, backup_path, std::filesystem::copy_options::overwrite_existing);
        LogMessage(LOG_INFO_LEVEL, ("Original save backed up to: " + backup_path.string()).c_str());
        out_backup_filepath = backup_path.string();

        // Encode (ordered_json preserves insertion order)
        std::string json_to_write = m_saveData.dump();
        std::vector<unsigned char> final_bytes = EncodeWithBypass(json_to_write);

        std::ofstream output_file(m_currentSaveFilePath, std::ios::binary | std::ios::trunc);
        if (!output_file) {
            LogMessage(LOG_ERROR_LEVEL, "Could not open save file for writing.");
            return false;
        }
        output_file.write(reinterpret_cast<const char*>(final_bytes.data()), final_bytes.size());
        
        LogMessage(LOG_INFO_LEVEL, "Saved successfully.");
        return true;
    } catch (const std::exception& e) {
        LogMessage(LOG_ERROR_LEVEL, ("Write Error: " + std::string(e.what())).c_str());
        return false;
    }
}

// --- Stats Helpers (Updated to use ordered_json if accessed directly) ---
long long SaveGameManager::GetGold() const {
    if (m_saveData["PlayerInfo"].contains("m_Gold")) return m_saveData["PlayerInfo"]["m_Gold"];
    return 0;
}
long long SaveGameManager::GetBei() const {
    if (m_saveData["PlayerInfo"].contains("m_Bei")) return m_saveData["PlayerInfo"]["m_Bei"];
    return 0;
}
long long SaveGameManager::GetArtisansFlame() const {
    if (m_saveData["PlayerInfo"].contains("m_ChefFlame")) return m_saveData["PlayerInfo"]["m_ChefFlame"];
    return 0;
}
long long SaveGameManager::GetFollowerCount() const {
    if (m_saveData["SNSInfo"].contains("m_Follow_Count")) return m_saveData["SNSInfo"]["m_Follow_Count"];
    return 0;
}
void SaveGameManager::SetGold(long long value) { m_saveData["PlayerInfo"]["m_Gold"] = std::min(value, SAVE_MAX_CURRENCY); }
void SaveGameManager::SetBei(long long value) { m_saveData["PlayerInfo"]["m_Bei"] = std::min(value, SAVE_MAX_CURRENCY); }
void SaveGameManager::SetArtisansFlame(long long value) { m_saveData["PlayerInfo"]["m_ChefFlame"] = std::min(value, SAVE_MAX_CURRENCY); }
void SaveGameManager::SetFollowerCount(long long value) { m_saveData["SNSInfo"]["m_Follow_Count"] = value; }

// --- Jungle DLC ---
bool SaveGameManager::IsJungleDLCInstalled() const {
    if (!m_isSaveFileLoaded) return false;
    if (!m_saveData["GameInfo"]["installedDLCs"].is_array()) return false;
    for (const auto& dlc : m_saveData["GameInfo"]["installedDLCs"])
        if (dlc.get<int>() == 14250001) return true;
    return false;
}

long long SaveGameManager::GetJungleGold() const {
    if (m_saveData.contains("JDLCContents") &&
        m_saveData["JDLCContents"].contains("junglePlayerInfoSave") &&
        m_saveData["JDLCContents"]["junglePlayerInfoSave"].contains("jungleGold"))
        return m_saveData["JDLCContents"]["junglePlayerInfoSave"]["jungleGold"];
    return 0;
}

long long SaveGameManager::GetJungleArtisansFlame() const {
    if (m_saveData.contains("JDLCContents") &&
        m_saveData["JDLCContents"].contains("junglePlayerInfoSave") &&
        m_saveData["JDLCContents"]["junglePlayerInfoSave"].contains("jungleChefFlame"))
        return m_saveData["JDLCContents"]["junglePlayerInfoSave"]["jungleChefFlame"];
    return 0;
}

void SaveGameManager::SetJungleGold(long long value) {
    m_saveData["JDLCContents"]["junglePlayerInfoSave"]["jungleGold"] = std::min(value, SAVE_MAX_CURRENCY);
}

void SaveGameManager::SetJungleArtisansFlame(long long value) {
    m_saveData["JDLCContents"]["junglePlayerInfoSave"]["jungleChefFlame"] = std::min(value, SAVE_MAX_CURRENCY);
}

// --- Ingredients Helpers ---
static int GetDesiredMaxCountForTier(int item_db_max_count) {
    if (item_db_max_count >= 9999) return 6666;
    if (item_db_max_count >= 999) return 666;
    if (item_db_max_count >= 99) return 66;
    return 0;
}

void SaveGameManager::MaxOwnIngredients(sqlite3* db) {
    if (!m_isSaveFileLoaded || !db) return;

    // Ocean ingredients: look up MaxCount via Items.ItemDataID
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT MaxCount FROM Items WHERE ItemDataID = ?;", -1, &stmt, NULL);
    for (auto& item : m_saveData["Ingredients"].items()) {
        if (item.value().contains("ingredientsID")) {
            sqlite3_reset(stmt);
            sqlite3_bind_int(stmt, 1, item.value()["ingredientsID"]);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int max = sqlite3_column_int(stmt, 0);
                int target = GetDesiredMaxCountForTier(max);
                if (target > 0) item.value()["count"] = target;
            }
        }
    }
    sqlite3_finalize(stmt);

    // Jungle ingredients: look up MaxCount via Items.TID (parentID in save)
    if (!IsJungleDLCInstalled()) return;
    if (!m_saveData["JDLCContents"].contains("jungleIngredientsSave")) return;

    sqlite3_stmt *jstmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT MaxCount FROM Items WHERE TID = ?;", -1, &jstmt, NULL);
    for (auto& item : m_saveData["JDLCContents"]["jungleIngredientsSave"].items()) {
        if (item.value().contains("parentID")) {
            sqlite3_reset(jstmt);
            sqlite3_bind_int(jstmt, 1, item.value()["parentID"]);
            if (sqlite3_step(jstmt) == SQLITE_ROW) {
                int max = sqlite3_column_int(jstmt, 0);
                int target = GetDesiredMaxCountForTier(max);
                if (target > 0) item.value()["count"] = target;
            }
        }
    }
    sqlite3_finalize(jstmt);
}

void SaveGameManager::MaxAllIngredients(sqlite3* db) {
    if (!m_isSaveFileLoaded || !db) return;
    const std::map<int, int> dlcTypeToIdMap = { {1, 14252001}, {3, 14252201}, {4, 14250001}, {5, 14252401} };
    std::set<int> installedDlcIds;
    if (m_saveData["GameInfo"]["installedDLCs"].is_array()) {
        for (const auto& dlc : m_saveData["GameInfo"]["installedDLCs"]) installedDlcIds.insert(dlc.get<int>());
    }

    sqlite3_stmt *stmt = nullptr;
    const char* sql = "SELECT I.TID, T.TID as pID, T.MaxCount, T.DLCType FROM Ingredients I JOIN Items T ON I.TID = T.ItemDataID";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return;

    bool jungleInstalled = installedDlcIds.count(14250001) > 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id     = sqlite3_column_int(stmt, 0);
        int parent = sqlite3_column_int(stmt, 1);
        int max    = sqlite3_column_int(stmt, 2);
        int dlc    = sqlite3_column_int(stmt, 3);

        if (dlcTypeToIdMap.count(dlc) && !installedDlcIds.count(dlcTypeToIdMap.at(dlc))) continue;

        int target = GetDesiredMaxCountForTier(max);
        if (target == 0) continue;

        std::string key = std::to_string(id);

        if (id > 99999999) {
            // 9-digit TID → jungle fish/material ingredient → jungleIngredientsSave
            if (!jungleInstalled) continue;
            if (!m_saveData["JDLCContents"].contains("jungleIngredientsSave")) continue;
            if (m_saveData["JDLCContents"]["jungleIngredientsSave"].contains(key)) {
                m_saveData["JDLCContents"]["jungleIngredientsSave"][key]["count"] = target;
            } else {
                ordered_json entry;
                entry["ingredientID"] = id;
                entry["count"] = target;
                entry["isNew"] = true;
                entry["level"] = 1;
                entry["parentID"] = parent;
                entry["lastGainTime"] = "04/01/2025 12:34:56";
                entry["lastGainGameTime"] = "10/03/2022 08:30:52";
                m_saveData["JDLCContents"]["jungleIngredientsSave"][key] = entry;
            }
        } else {
            // 8-digit TID → ocean or jungle farm ingredient → main Ingredients section
            if (m_saveData["Ingredients"].contains(key)) {
                m_saveData["Ingredients"][key]["count"] = target;
            } else {
                ordered_json entry;
                entry["ingredientsID"] = id;
                entry["parentID"] = parent;
                entry["count"] = target;
                entry["level"] = 1;
                entry["branchCount"] = 0;
                entry["isNew"] = true;
                entry["placeTagMask"] = 1;
                entry["lastGainTime"] = "04/01/2025 12:34:56";
                entry["lastGainGameTime"] = "10/03/2022 08:30:52";
                m_saveData["Ingredients"][key] = entry;
            }
        }
    }
    sqlite3_finalize(stmt);
}

std::filesystem::path SaveGameManager::GetDefaultSaveGameDirectoryAndLatestFile(std::string& latestSaveFileName) {
    LogMessage(LOG_INFO_LEVEL, "Searching for latest save file (Steam and Xbox)...");
    
    // --- Steam Discovery ---
    PWSTR pszPath = NULL;
    std::filesystem::path steamBaseDir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, NULL, &pszPath))) {
        steamBaseDir = pszPath; CoTaskMemFree(pszPath);
        steamBaseDir /= L"nexon"; steamBaseDir /= L"DAVE THE DIVER"; steamBaseDir /= L"SteamSData";
    } else {
        const char* localAppDataEnv = getenv("LOCALAPPDATA");
        if (localAppDataEnv) {
             steamBaseDir = localAppDataEnv;
             steamBaseDir = steamBaseDir.parent_path() / "LocalLow";
             steamBaseDir /= L"nexon"; steamBaseDir /= L"DAVE THE DIVER"; steamBaseDir /= L"SteamSData";
        }
    }

    std::filesystem::path steamIDPath;
    if (std::filesystem::exists(steamBaseDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(steamBaseDir)) {
            if (entry.is_directory()) {
                std::string folderName = entry.path().filename().string();
                if (!folderName.empty() && std::all_of(folderName.begin(), folderName.end(), ::isdigit)) {
                    steamIDPath = entry.path();
                    break;
                }
            }
        }
    }
    if (steamIDPath.empty()) steamIDPath = steamBaseDir;
    
    std::filesystem::file_time_type latestSteamTime;
    std::filesystem::path latestSteamFile;
    if (std::filesystem::exists(steamIDPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(steamIDPath)) {
            if (entry.is_regular_file()) {
                std::string fileName = entry.path().filename().string();
                bool is_autosave = (fileName.find("GameSave") == 0 && fileName.find("_GD.sav") != std::string::npos);
                bool is_manual_save = (fileName.find("m_") == 0 && fileName.find(".sav") != std::string::npos);
                if (is_autosave || is_manual_save) {
                    auto ftime = std::filesystem::last_write_time(entry.path());
                    if (latestSteamFile.empty() || ftime > latestSteamTime) {
                        latestSteamTime = ftime;
                        latestSteamFile = entry.path();
                    }
                }
            }
        }
    }

    // --- Xbox Discovery ---
    std::filesystem::path xboxBaseDir;
    const char* localAppDataValue = getenv("LOCALAPPDATA");
    if (localAppDataValue) {
        std::filesystem::path packagesDir = std::filesystem::path(localAppDataValue) / "Packages";
        if (std::filesystem::exists(packagesDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(packagesDir)) {
                if (entry.is_directory() && entry.path().filename().string().find("Mintrocket.DaveTheDiver") == 0) {
                    xboxBaseDir = entry.path() / "SystemAppData" / "wgs";
                    break;
                }
            }
        }
    }

    std::filesystem::file_time_type latestXboxTime;
    std::filesystem::path latestXboxFile;
    std::filesystem::path xboxActiveDir;

    if (!xboxBaseDir.empty() && std::filesystem::exists(xboxBaseDir)) {
        try {
            // Xbox saves are in subdirectories with "crazy numbers"
            for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(xboxBaseDir)) {
                if (dirEntry.is_regular_file()) {
                    std::string fileName = dirEntry.path().filename().string();
                    // Skip containers.index and small metadata files
                    if (fileName == "containers.index" || fileName.find("container.") == 0) continue;
                    
                    // Xbox save files are usually large hex strings and fairly large
                    if (std::filesystem::file_size(dirEntry.path()) > 1024) { 
                        auto ftime = std::filesystem::last_write_time(dirEntry.path());
                        if (latestXboxFile.empty() || ftime > latestXboxTime) {
                            latestXboxTime = ftime;
                            latestXboxFile = dirEntry.path();
                            xboxActiveDir = dirEntry.path().parent_path();
                        }
                    }
                }
            }
        } catch (...) {
            LogMessage(LOG_ERROR_LEVEL, "Error iterating Xbox save directory.");
        }
    }

    // --- Compare and Select ---
    if (!latestSteamFile.empty() && !latestXboxFile.empty()) {
        if (latestXboxTime > latestSteamTime) {
            LogMessage(LOG_INFO_LEVEL, "Latest save is XBOX version.");
            latestSaveFileName = latestXboxFile.string();
            return xboxActiveDir;
        } else {
            LogMessage(LOG_INFO_LEVEL, "Latest save is STEAM version.");
            latestSaveFileName = latestSteamFile.string();
            return steamIDPath;
        }
    } else if (!latestXboxFile.empty()) {
        LogMessage(LOG_INFO_LEVEL, "Only XBOX save found.");
        latestSaveFileName = latestXboxFile.string();
        return xboxActiveDir;
    } else if (!latestSteamFile.empty()) {
        LogMessage(LOG_INFO_LEVEL, "Only STEAM save found.");
        latestSaveFileName = latestSteamFile.string();
        return steamIDPath;
    }

    LogMessage(LOG_INFO_LEVEL, "No save files found.");
    return steamIDPath; // Fallback to steam base path even if empty
}
//END OF SaveGameManager.cpp
