#pragma once

#include "picojson.h"
#include <string>

namespace JsonUtil {

    // Parse a JSON file from disk. Returns false and logs on error.
    bool ParseFile(const std::string& filepath, picojson::value& outRoot);

    // Parse a JSON string. Returns false and logs on error.
    bool ParseString(const std::string& jsonStr, picojson::value& outRoot);

    // Serialize a picojson value to pretty-printed JSON (2-space indent).
    std::string Serialize(const picojson::value& val);

    // Write JSON string to file. Returns false on I/O error.
    bool WriteFile(const std::string& filepath, const std::string& json);

    // Safe field extraction helpers
    bool GetInt(const picojson::object& obj, const std::string& key, int& out);
    bool GetDouble(const picojson::object& obj, const std::string& key, double& out);
    bool GetString(const picojson::object& obj, const std::string& key, std::string& out);
    bool GetBool(const picojson::object& obj, const std::string& key, bool& out);
    bool GetArray(const picojson::object& obj, const std::string& key, const picojson::array*& out);
    bool GetObject(const picojson::object& obj, const std::string& key, const picojson::object*& out);

} // namespace JsonUtil
