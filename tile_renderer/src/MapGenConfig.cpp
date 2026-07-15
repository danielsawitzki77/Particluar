#include "MapGenConfig.h"
#include "JsonUtil.h"
#include <SDL3/SDL.h>

bool MapGenConfig::Load(const std::string& filepath)
{
    picojson::value root;
    if (!JsonUtil::ParseFile(filepath, root)) {
        SDL_Log("[MapGenConfig] Failed to parse file '%s'.", filepath.c_str());
        return false;
    }

    if (!root.is<picojson::object>()) {
        SDL_Log("[MapGenConfig] Root is not a JSON object in '%s'.", filepath.c_str());
        return false;
    }

    const picojson::object& obj = root.get<picojson::object>();

    JsonUtil::GetString(obj, "name", m_data.name);

    int seedInt = 0;
    if (JsonUtil::GetInt(obj, "seed", seedInt)) {
        m_data.seed = static_cast<unsigned int>(seedInt);
    } else {
        m_data.seed = 0;
    }

    if (!JsonUtil::GetInt(obj, "width", m_data.width)) {
        m_data.width = 32;
    }
    if (!JsonUtil::GetInt(obj, "height", m_data.height)) {
        m_data.height = 32;
    }

    // Parse layers array
    const picojson::array* layersArr = nullptr;
    if (JsonUtil::GetArray(obj, "layers", layersArr)) {
        m_data.layers.clear();
        for (const picojson::value& layerVal : *layersArr) {
            if (!layerVal.is<picojson::object>()) continue;
            const picojson::object& layerObj = layerVal.get<picojson::object>();

            MapGenLayerConfig layerCfg;
            JsonUtil::GetString(layerObj, "folder", layerCfg.folder);

            const picojson::array* allowedArr = nullptr;
            if (JsonUtil::GetArray(layerObj, "allowed_tilesets", allowedArr)) {
                for (const picojson::value& item : *allowedArr) {
                    if (item.is<std::string>()) {
                        layerCfg.allowedTilesets.push_back(item.get<std::string>());
                    }
                }
            }

            m_data.layers.push_back(layerCfg);
        }
    }

    // Parse submaps array
    const picojson::array* submapsArr = nullptr;
    if (JsonUtil::GetArray(obj, "submaps", submapsArr)) {
        m_data.submaps.clear();
        for (const picojson::value& smVal : *submapsArr) {
            if (!smVal.is<picojson::object>()) continue;
            const picojson::object& smObj = smVal.get<picojson::object>();

            SubmapRef ref;
            JsonUtil::GetString(smObj, "map_file", ref.mapFile);
            if (!JsonUtil::GetInt(smObj, "chance", ref.chance)) {
                ref.chance = 1;
            }
            if (ref.chance < 0) ref.chance = 0;
            m_data.submaps.push_back(ref);
        }
    }

    return true;
}

bool MapGenConfig::Save(const std::string& filepath) const
{
    std::string json = Serialize();
    return JsonUtil::WriteFile(filepath, json);
}

bool MapGenConfig::ParseFromString(const std::string& jsonStr)
{
    picojson::value root;
    if (!JsonUtil::ParseString(jsonStr, root)) {
        SDL_Log("[MapGenConfig] Failed to parse JSON string.");
        return false;
    }

    if (!root.is<picojson::object>()) {
        SDL_Log("[MapGenConfig] Root is not a JSON object.");
        return false;
    }

    const picojson::object& obj = root.get<picojson::object>();

    JsonUtil::GetString(obj, "name", m_data.name);

    int seedInt = 0;
    if (JsonUtil::GetInt(obj, "seed", seedInt)) {
        m_data.seed = static_cast<unsigned int>(seedInt);
    } else {
        m_data.seed = 0;
    }

    if (!JsonUtil::GetInt(obj, "width", m_data.width)) {
        m_data.width = 32;
    }
    if (!JsonUtil::GetInt(obj, "height", m_data.height)) {
        m_data.height = 32;
    }

    const picojson::array* layersArr = nullptr;
    if (JsonUtil::GetArray(obj, "layers", layersArr)) {
        m_data.layers.clear();
        for (const picojson::value& layerVal : *layersArr) {
            if (!layerVal.is<picojson::object>()) continue;
            const picojson::object& layerObj = layerVal.get<picojson::object>();

            MapGenLayerConfig layerCfg;
            JsonUtil::GetString(layerObj, "folder", layerCfg.folder);

            const picojson::array* allowedArr = nullptr;
            if (JsonUtil::GetArray(layerObj, "allowed_tilesets", allowedArr)) {
                for (const picojson::value& item : *allowedArr) {
                    if (item.is<std::string>()) {
                        layerCfg.allowedTilesets.push_back(item.get<std::string>());
                    }
                }
            }

            m_data.layers.push_back(layerCfg);
        }
    }

    // Parse submaps array
    const picojson::array* submapsArr = nullptr;
    if (JsonUtil::GetArray(obj, "submaps", submapsArr)) {
        m_data.submaps.clear();
        for (const picojson::value& smVal : *submapsArr) {
            if (!smVal.is<picojson::object>()) continue;
            const picojson::object& smObj = smVal.get<picojson::object>();

            SubmapRef ref;
            JsonUtil::GetString(smObj, "map_file", ref.mapFile);
            if (!JsonUtil::GetInt(smObj, "chance", ref.chance)) {
                ref.chance = 1;
            }
            if (ref.chance < 0) ref.chance = 0;
            m_data.submaps.push_back(ref);
        }
    }

    return true;
}

std::string MapGenConfig::Serialize() const
{
    picojson::object obj;
    obj["name"] = picojson::value(m_data.name);
    obj["seed"] = picojson::value(static_cast<double>(m_data.seed));
    obj["width"] = picojson::value(static_cast<double>(m_data.width));
    obj["height"] = picojson::value(static_cast<double>(m_data.height));

    picojson::array layersArr;
    for (const MapGenLayerConfig& layerCfg : m_data.layers) {
        picojson::object layerObj;
        layerObj["folder"] = picojson::value(layerCfg.folder);

        picojson::array allowedArr;
        for (const std::string& ts : layerCfg.allowedTilesets) {
            allowedArr.push_back(picojson::value(ts));
        }
        layerObj["allowed_tilesets"] = picojson::value(allowedArr);

        layersArr.push_back(picojson::value(layerObj));
    }
    obj["layers"] = picojson::value(layersArr);

    picojson::array submapsArr;
    for (const SubmapRef& ref : m_data.submaps) {
        picojson::object smObj;
        smObj["map_file"] = picojson::value(ref.mapFile);
        smObj["chance"] = picojson::value(static_cast<double>(ref.chance));
        submapsArr.push_back(picojson::value(smObj));
    }
    if (!submapsArr.empty()) {
        obj["submaps"] = picojson::value(submapsArr);
    }

    picojson::value root(obj);
    return JsonUtil::Serialize(root);
}
