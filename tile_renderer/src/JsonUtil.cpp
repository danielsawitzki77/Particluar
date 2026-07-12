#include "JsonUtil.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <sstream>

namespace JsonUtil {

bool ParseFile(const std::string& filepath, picojson::value& outRoot)
{
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        SDL_Log("[JsonUtil] Failed to open file: %s", filepath.c_str());
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    if (file.bad()) {
        SDL_Log("[JsonUtil] I/O error reading file: %s", filepath.c_str());
        return false;
    }

    return ParseString(ss.str(), outRoot);
}

bool ParseString(const std::string& jsonStr, picojson::value& outRoot)
{
    std::string err = picojson::parse(outRoot, jsonStr);
    if (!err.empty()) {
        SDL_Log("[JsonUtil] Parse error: %s", err.c_str());
        return false;
    }
    return true;
}

std::string Serialize(const picojson::value& val)
{
    return val.serialize(true);
}

bool WriteFile(const std::string& filepath, const std::string& json)
{
    std::ofstream file(filepath, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        SDL_Log("[JsonUtil] Failed to open file for writing: %s", filepath.c_str());
        return false;
    }

    file << json;
    if (file.bad()) {
        SDL_Log("[JsonUtil] I/O error writing file: %s", filepath.c_str());
        return false;
    }

    return true;
}

bool GetInt(const picojson::object& obj, const std::string& key, int& out)
{
    auto it = obj.find(key);
    if (it == obj.end()) return false;
    if (!it->second.is<double>()) return false;
    out = static_cast<int>(it->second.get<double>());
    return true;
}

bool GetDouble(const picojson::object& obj, const std::string& key, double& out)
{
    auto it = obj.find(key);
    if (it == obj.end()) return false;
    if (!it->second.is<double>()) return false;
    out = it->second.get<double>();
    return true;
}

bool GetString(const picojson::object& obj, const std::string& key, std::string& out)
{
    auto it = obj.find(key);
    if (it == obj.end()) return false;
    if (!it->second.is<std::string>()) return false;
    out = it->second.get<std::string>();
    return true;
}

bool GetBool(const picojson::object& obj, const std::string& key, bool& out)
{
    auto it = obj.find(key);
    if (it == obj.end()) return false;
    if (!it->second.is<bool>()) return false;
    out = it->second.get<bool>();
    return true;
}

bool GetArray(const picojson::object& obj, const std::string& key, const picojson::array*& out)
{
    auto it = obj.find(key);
    if (it == obj.end()) return false;
    if (!it->second.is<picojson::array>()) return false;
    out = &it->second.get<picojson::array>();
    return true;
}

bool GetObject(const picojson::object& obj, const std::string& key, const picojson::object*& out)
{
    auto it = obj.find(key);
    if (it == obj.end()) return false;
    if (!it->second.is<picojson::object>()) return false;
    out = &it->second.get<picojson::object>();
    return true;
}

} // namespace JsonUtil
