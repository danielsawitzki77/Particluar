#include "GlobalConfig.h"
#include "JsonUtil.h"
#include <SDL3/SDL.h>
#include <cmath>

void GlobalConfig::ApplyDefaults()
{
    m_data.tileWidth = 32;
    m_data.tileHeight = 32;
    m_data.viewportX = 0;
    m_data.viewportY = 0;
    m_data.viewportWidth = 800;
    m_data.viewportHeight = 600;
    m_data.scrollSpeed = 200.0f;
    m_data.wfcMaxBacktracks = 4;
    m_data.debugShowBlocking = false;
}

bool GlobalConfig::Validate(const GlobalConfigData& candidate) const
{
    if (candidate.tileWidth < 1 || candidate.tileWidth > 512) return false;
    if (candidate.tileHeight < 1 || candidate.tileHeight > 512) return false;
    if (candidate.viewportX < 0 || candidate.viewportX > 7680) return false;
    if (candidate.viewportY < 0 || candidate.viewportY > 4320) return false;
    if (candidate.viewportWidth < 1 || candidate.viewportWidth > 7680) return false;
    if (candidate.viewportHeight < 1 || candidate.viewportHeight > 4320) return false;
    if (candidate.scrollSpeed <= 0.0f || !std::isfinite(candidate.scrollSpeed)) return false;
    return true;
}

bool GlobalConfig::Load(const std::string& filepath)
{
    picojson::value root;
    if (!JsonUtil::ParseFile(filepath, root)) {
        SDL_Log("[GlobalConfig] Failed to parse file '%s', using defaults.", filepath.c_str());
        ApplyDefaults();
        return false;
    }

    if (!root.is<picojson::object>()) {
        SDL_Log("[GlobalConfig] Root is not a JSON object in '%s', using defaults.", filepath.c_str());
        ApplyDefaults();
        return false;
    }

    const picojson::object& obj = root.get<picojson::object>();

    GlobalConfigData candidate;
    bool allFieldsOk = true;

    if (!JsonUtil::GetInt(obj, "tile_width", candidate.tileWidth)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'tile_width' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "tile_height", candidate.tileHeight)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'tile_height' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "viewport_x", candidate.viewportX)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'viewport_x' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "viewport_y", candidate.viewportY)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'viewport_y' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "viewport_width", candidate.viewportWidth)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'viewport_width' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "viewport_height", candidate.viewportHeight)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'viewport_height' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }

    // scroll_speed is a float, extracted via GetDouble
    double scrollSpeedDouble = 0.0;
    if (!JsonUtil::GetDouble(obj, "scroll_speed", scrollSpeedDouble)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'scroll_speed' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    } else {
        candidate.scrollSpeed = static_cast<float>(scrollSpeedDouble);
    }

    // wfc_max_backtracks is optional (defaults to 4)
    if (!JsonUtil::GetInt(obj, "wfc_max_backtracks", candidate.wfcMaxBacktracks)) {
        candidate.wfcMaxBacktracks = 4;
    }
    if (candidate.wfcMaxBacktracks < 0) candidate.wfcMaxBacktracks = 0;
    if (candidate.wfcMaxBacktracks > 1000) candidate.wfcMaxBacktracks = 1000;

    // debug_show_blocking is optional (defaults to false)
    {
        bool boolVal = false;
        if (JsonUtil::GetBool(obj, "debug_show_blocking", boolVal)) {
            candidate.debugShowBlocking = boolVal;
        } else {
            candidate.debugShowBlocking = false;
        }
    }

    if (!allFieldsOk) {
        ApplyDefaults();
        return false;
    }

    if (!Validate(candidate)) {
        SDL_Log("[GlobalConfig] One or more fields out of valid range in '%s', using defaults.", filepath.c_str());
        ApplyDefaults();
        return false;
    }

    m_data = candidate;
    return true;
}

const GlobalConfigData& GlobalConfig::Get() const
{
    return m_data;
}

std::string GlobalConfig::Serialize() const
{
    picojson::object obj;
    obj["tile_width"] = picojson::value(static_cast<double>(m_data.tileWidth));
    obj["tile_height"] = picojson::value(static_cast<double>(m_data.tileHeight));
    obj["viewport_x"] = picojson::value(static_cast<double>(m_data.viewportX));
    obj["viewport_y"] = picojson::value(static_cast<double>(m_data.viewportY));
    obj["viewport_width"] = picojson::value(static_cast<double>(m_data.viewportWidth));
    obj["viewport_height"] = picojson::value(static_cast<double>(m_data.viewportHeight));
    obj["scroll_speed"] = picojson::value(static_cast<double>(m_data.scrollSpeed));
    obj["wfc_max_backtracks"] = picojson::value(static_cast<double>(m_data.wfcMaxBacktracks));
    obj["debug_show_blocking"] = picojson::value(m_data.debugShowBlocking);

    picojson::value root(obj);
    return JsonUtil::Serialize(root);
}
