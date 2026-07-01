#pragma once

#include "BodyTypes.h"
#include <string>

namespace BodyRenderer {

struct LoadResult {
    bool success;
    std::string error;
    Body body; // valid only if success == true

    LoadResult() : success(false) {}
};

class BodyLoader {
public:
    LoadResult LoadFromFile(const std::string& filepath);
    LoadResult LoadFromString(const std::string& json);
    std::string Serialize(const Body& body) const;

private:
    bool ParseNode(const void* json_obj, BodyNode* out, int depth, std::string& error);
    bool ParseShapeParams(const void* dims_obj, const std::string& type_str, ShapeParams* out, std::string& error);
    bool ParseConnection(const void* conn_obj, Connection* out, std::string& error);
};

} // namespace BodyRenderer
