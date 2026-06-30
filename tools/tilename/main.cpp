// tilename.exe - Tile Naming CLI Tool
// Assigns deterministic names to unnamed tiles in a tileset sidecar JSON.
// Usage: tilename.exe <tileset_folder_path>
//
// Steps:
//   1. Parse sidecar JSON from the folder (using JsonUtil::ParseFile)
//   2. Iterate tiles array, find entries with empty or missing "id"
//   3. For each unnamed tile, generate name: tile_<x>_<y> (from source_rect)
//   4. Ensure no collision with existing names (append _2, _3, etc.)
//   5. If all tiles already named: exit(0) without modifying file
//   6. Otherwise: update JSON in place and write back using JsonUtil::WriteFile
//   7. On parse error: exit(1) with error message to stderr

#include "JsonUtil.h"
#include "picojson.h"
#include <iostream>
#include <string>
#include <set>
#include <cstdlib>

namespace {

// Locate the sidecar JSON file in the tileset folder.
// Convention: <folder>/<foldername>.json
std::string FindSidecarPath(const std::string& folderPath) {
    // Extract folder name from path
    std::string folder = folderPath;
    // Remove trailing slashes
    while (!folder.empty() && (folder.back() == '/' || folder.back() == '\\')) {
        folder.pop_back();
    }
    // Find the last path separator
    size_t sep = folder.find_last_of("/\\");
    std::string name = (sep == std::string::npos) ? folder : folder.substr(sep + 1);
    return folderPath + "/" + name + ".json";
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: tilename.exe <tileset_folder_path>" << std::endl;
        return 1;
    }

    std::string folderPath = argv[1];
    std::string sidecarPath = FindSidecarPath(folderPath);

    // 1. Parse sidecar JSON
    picojson::value root;
    if (!JsonUtil::ParseFile(sidecarPath, root)) {
        std::cerr << "Error: Failed to parse sidecar JSON: " << sidecarPath << std::endl;
        return 1;
    }

    if (!root.is<picojson::object>()) {
        std::cerr << "Error: Sidecar JSON root is not an object: " << sidecarPath << std::endl;
        return 1;
    }

    picojson::object& rootObj = root.get<picojson::object>();

    // Find the "tiles" array
    if (rootObj.find("tiles") == rootObj.end() || !rootObj["tiles"].is<picojson::array>()) {
        std::cerr << "Error: Sidecar JSON missing 'tiles' array: " << sidecarPath << std::endl;
        return 1;
    }

    picojson::array& tiles = rootObj["tiles"].get<picojson::array>();

    // 2. Collect all existing tile IDs
    std::set<std::string> existingIds;
    for (size_t i = 0; i < tiles.size(); ++i) {
        if (!tiles[i].is<picojson::object>()) continue;
        const picojson::object& tileObj = tiles[i].get<picojson::object>();
        auto it = tileObj.find("id");
        if (it != tileObj.end() && it->second.is<std::string>()) {
            const std::string& id = it->second.get<std::string>();
            if (!id.empty()) {
                existingIds.insert(id);
            }
        }
    }

    // 3. Identify unnamed tiles and assign names
    bool modified = false;
    for (size_t i = 0; i < tiles.size(); ++i) {
        if (!tiles[i].is<picojson::object>()) continue;
        picojson::object& tileObj = tiles[i].get<picojson::object>();

        // Check if tile needs naming
        bool needsName = false;
        auto idIt = tileObj.find("id");
        if (idIt == tileObj.end()) {
            needsName = true;
        } else if (!idIt->second.is<std::string>()) {
            needsName = true;
        } else if (idIt->second.get<std::string>().empty()) {
            needsName = true;
        }

        if (!needsName) continue;

        // Get source_rect x,y for deterministic naming
        int srcX = 0, srcY = 0;
        auto srIt = tileObj.find("source_rect");
        if (srIt != tileObj.end() && srIt->second.is<picojson::object>()) {
            const picojson::object& sr = srIt->second.get<picojson::object>();
            auto xIt = sr.find("x");
            auto yIt = sr.find("y");
            if (xIt != sr.end() && xIt->second.is<double>()) {
                srcX = static_cast<int>(xIt->second.get<double>());
            }
            if (yIt != sr.end() && yIt->second.is<double>()) {
                srcY = static_cast<int>(yIt->second.get<double>());
            }
        }

        // Generate base name: tile_<x>_<y>
        std::string baseName = "tile_" + std::to_string(srcX) + "_" + std::to_string(srcY);

        // Resolve collisions: try baseName, then baseName_2, _3, etc.
        std::string finalName = baseName;
        if (existingIds.count(finalName) > 0) {
            int suffix = 2;
            do {
                finalName = baseName + "_" + std::to_string(suffix);
                ++suffix;
            } while (existingIds.count(finalName) > 0);
        }

        // Assign the name
        tileObj["id"] = picojson::value(finalName);
        existingIds.insert(finalName);
        modified = true;
    }

    // 5. If all tiles already had names, exit cleanly
    if (!modified) {
        return 0;
    }

    // 6. Serialize and write back
    std::string json = JsonUtil::Serialize(root);
    if (!JsonUtil::WriteFile(sidecarPath, json)) {
        std::cerr << "Error: Failed to write sidecar JSON: " << sidecarPath << std::endl;
        return 1;
    }

    return 0;
}
