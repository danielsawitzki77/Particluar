#include "GlobalConfig.h"
#include "JsonUtil.h"
#include <SDL3/SDL.h>
#include <cmath>

void GlobalConfig::ApplyDefaults()
{
    m_data.tile_width = 32;
    m_data.tile_height = 32;
    m_data.viewport_x = 0;
    m_data.viewport_y = 0;
    m_data.viewport_width = 800;
    m_data.viewport_height = 600;
    m_data.scroll_speed = 200.0f;
}

bool GlobalConfig::Validate(const GlobalConfigData& candidate) const
{
    if (candidate.tile_width < 1 || candidate.tile_width > 512) return false;
    if (candidate.tile_height < 1 || candidate.tile_height > 512) return false;
    if (candidate.viewport_x < 0 || candidate.viewport_x > 7680) return false;
    if (candidate.viewport_y < 0 || candidate.viewport_y > 4320) return false;
    if (candidate.viewport_width < 1 || candidate.viewport_width > 7680) return false;
    if (candidate.viewport_height < 1 || candidate.viewport_height > 4320) return false;
    if (candidate.scroll_speed <= 0.0f || !std::isfinite(candidate.scroll_speed)) return false;
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

    if (!JsonUtil::GetInt(obj, "tile_width", candidate.tile_width)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'tile_width' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "tile_height", candidate.tile_height)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'tile_height' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "viewport_x", candidate.viewport_x)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'viewport_x' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "viewport_y", candidate.viewport_y)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'viewport_y' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "viewport_width", candidate.viewport_width)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'viewport_width' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }
    if (!JsonUtil::GetInt(obj, "viewport_height", candidate.viewport_height)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'viewport_height' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    }

    // scroll_speed is a float, extracted via GetDouble
    double scrollSpeedDouble = 0.0;
    if (!JsonUtil::GetDouble(obj, "scroll_speed", scrollSpeedDouble)) {
        SDL_Log("[GlobalConfig] Missing or invalid field 'scroll_speed' in '%s', using defaults.", filepath.c_str());
        allFieldsOk = false;
    } else {
        candidate.scroll_speed = static_cast<float>(scrollSpeedDouble);
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
    obj["tile_width"] = picojson::value(static_cast<double>(m_data.tile_width));
    obj["tile_height"] = picojson::value(static_cast<double>(m_data.tile_height));
    obj["viewport_x"] = picojson::value(static_cast<double>(m_data.viewport_x));
    obj["viewport_y"] = picojson::value(static_cast<double>(m_data.viewport_y));
    obj["viewport_width"] = picojson::value(static_cast<double>(m_data.viewport_width));
    obj["viewport_height"] = picojson::value(static_cast<double>(m_data.viewport_height));
    obj["scroll_speed"] = picojson::value(static_cast<double>(m_data.scroll_speed));

    picojson::value root(obj);
    return JsonUtil::Serialize(root);
}
