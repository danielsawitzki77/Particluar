#include "BodyLoader.h"
#include "ConnectionValidator.h"
#include "picojson.h"
#include <fstream>
#include <sstream>

namespace BodyRenderer {

static const int MAX_DEPTH = 32;

LoadResult BodyLoader::LoadFromFile(const std::string& filepath)
{
    LoadResult result;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        result.success = false;
        result.error = "File not found: " + filepath;
        return result;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    return LoadFromString(ss.str());
}

LoadResult BodyLoader::LoadFromString(const std::string& json)
{
    LoadResult result;

    picojson::value root;
    std::string err = picojson::parse(root, json);
    if (!err.empty()) {
        result.success = false;
        result.error = err;
        return result;
    }

    if (!root.is<picojson::object>()) {
        result.success = false;
        result.error = "Expected JSON object at root";
        return result;
    }

    const picojson::object& obj = root.get<picojson::object>();

    // Detect format version
    int formatVersion = 1; // default to legacy
    if (obj.count("formatVersion") && obj.at("formatVersion").is<double>()) {
        formatVersion = static_cast<int>(obj.at("formatVersion").get<double>());
    }
    result.body.formatVersion = formatVersion;

    // Parse name
    if (obj.count("name") && obj.at("name").is<std::string>()) {
        result.body.name = obj.at("name").get<std::string>();
    }

    // Parse optional material
    if (obj.count("material") && obj.at("material").is<picojson::object>()) {
        const picojson::object& mat = obj.at("material").get<picojson::object>();
        if (mat.count("shininess") && mat.at("shininess").is<double>()) {
            float s = static_cast<float>(mat.at("shininess").get<double>());
            if (s < 1.0f) s = 1.0f;
            if (s > 128.0f) s = 128.0f;
            result.body.material.shininess = s;
        }
        if (mat.count("ambient") && mat.at("ambient").is<picojson::object>()) {
            const picojson::object& amb = mat.at("ambient").get<picojson::object>();
            if (amb.count("r") && amb.at("r").is<double>())
                result.body.material.ambient.x = static_cast<float>(amb.at("r").get<double>());
            if (amb.count("g") && amb.at("g").is<double>())
                result.body.material.ambient.y = static_cast<float>(amb.at("g").get<double>());
            if (amb.count("b") && amb.at("b").is<double>())
                result.body.material.ambient.z = static_cast<float>(amb.at("b").get<double>());
        }
    }

    // Parse root node
    const picojson::object* node_obj = nullptr;
    if (obj.count("root") && obj.at("root").is<picojson::object>()) {
        node_obj = &obj.at("root").get<picojson::object>();
    } else if (obj.count("shape")) {
        node_obj = &obj;
    }

    if (!node_obj) {
        result.success = false;
        result.error = "Missing 'root' or 'shape' field in body JSON";
        return result;
    }

    std::string parse_err;
    if (!ParseNode(static_cast<const void*>(node_obj), &result.body.root, 0, formatVersion, parse_err)) {
        result.success = false;
        result.error = parse_err;
        return result;
    }

    // Validate connection topology compatibility
    ConnectionValidator validator;
    ValidationResult validation = validator.ValidateBody(result.body);
    if (!validation.valid) {
        result.success = false;
        result.error = validation.error;
        return result;
    }

    result.success = true;
    return result;
}

bool BodyLoader::ParseNode(const void* json_obj_ptr, BodyNode* out, int depth, int formatVersion, std::string& error)
{
    if (depth > MAX_DEPTH) {
        error = "Maximum nesting depth exceeded (32 levels)";
        return false;
    }

    const picojson::object& obj = *static_cast<const picojson::object*>(json_obj_ptr);

    // Parse name (optional)
    if (obj.count("name") && obj.at("name").is<std::string>()) {
        out->name = obj.at("name").get<std::string>();
    }

    // Parse shape (required)
    if (!obj.count("shape") || !obj.at("shape").is<picojson::object>()) {
        error = "Missing 'shape' field in node '" + out->name + "'";
        return false;
    }

    const picojson::object& shape_obj = obj.at("shape").get<picojson::object>();

    // Get type
    if (!shape_obj.count("type") || !shape_obj.at("type").is<std::string>()) {
        error = "Missing 'type' in shape of node '" + out->name + "'";
        return false;
    }

    std::string type_str = shape_obj.at("type").get<std::string>();

    if (!ParseShapeParams(static_cast<const void*>(&shape_obj), type_str, &out->shape, error)) {
        return false;
    }

    // Parse color (optional with default)
    if (obj.count("color") && obj.at("color").is<picojson::object>()) {
        const picojson::object& col = obj.at("color").get<picojson::object>();
        if (col.count("r") && col.at("r").is<double>()) {
            float v = static_cast<float>(col.at("r").get<double>());
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            out->color.x = v;
        }
        if (col.count("g") && col.at("g").is<double>()) {
            float v = static_cast<float>(col.at("g").get<double>());
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            out->color.y = v;
        }
        if (col.count("b") && col.at("b").is<double>()) {
            float v = static_cast<float>(col.at("b").get<double>());
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            out->color.z = v;
        }
    }

    // Parse children (optional)
    if (obj.count("children") && obj.at("children").is<picojson::array>()) {
        const picojson::array& children = obj.at("children").get<picojson::array>();
        for (size_t i = 0; i < children.size(); ++i) {
            if (!children[i].is<picojson::object>()) {
                error = "Child " + std::to_string(i) + " is not an object";
                return false;
            }

            const picojson::object& child_obj = children[i].get<picojson::object>();

            BodyNode child_node;

            // Parse connection for this child
            if (child_obj.count("connection") && child_obj.at("connection").is<picojson::object>()) {
                const picojson::object& conn_obj = child_obj.at("connection").get<picojson::object>();
                if (formatVersion >= 2) {
                    if (!ParseConnectionV2(static_cast<const void*>(&conn_obj), &child_node.connection, error)) {
                        return false;
                    }
                } else {
                    if (!ParseConnectionV1(static_cast<const void*>(&conn_obj), &child_node.connection, error)) {
                        return false;
                    }
                }
            }

            // The child node data might be nested under "node" or be the object itself
            const picojson::object* child_node_obj = nullptr;
            if (child_obj.count("node") && child_obj.at("node").is<picojson::object>()) {
                child_node_obj = &child_obj.at("node").get<picojson::object>();
            } else if (child_obj.count("shape")) {
                child_node_obj = &child_obj;
            } else {
                error = "Child " + std::to_string(i) + " has no 'node' or 'shape' field";
                return false;
            }

            if (!ParseNode(static_cast<const void*>(child_node_obj), &child_node, depth + 1, formatVersion, error)) {
                return false;
            }

            out->children.push_back(child_node);
        }
    }

    return true;
}

bool BodyLoader::ParseShapeParams(const void* dims_ptr, const std::string& type_str, ShapeParams* out, std::string& error)
{
    const picojson::object& obj = *static_cast<const picojson::object*>(dims_ptr);

    // Determine type
    if (type_str == "cone") {
        out->type = ShapeType::Cone;
    } else if (type_str == "cylinder") {
        out->type = ShapeType::Cylinder;
    } else if (type_str == "sphere") {
        out->type = ShapeType::Sphere;
    } else if (type_str == "torus") {
        out->type = ShapeType::Torus;
    } else if (type_str == "capsule") {
        out->type = ShapeType::Capsule;
    } else {
        error = "Unknown shape type: '" + type_str + "'. Valid types: cone, cylinder, sphere, torus, capsule";
        return false;
    }

    // Helper to read a positive float (required)
    auto readFloat = [&](const std::string& field, float& target, bool allow_zero = false) -> bool {
        if (!obj.count(field) || !obj.at(field).is<double>()) {
            error = "Missing field '" + field + "' in shape";
            return false;
        }
        float v = static_cast<float>(obj.at(field).get<double>());
        if (allow_zero ? (v < 0.0f) : (v <= 0.0f)) {
            error = "'" + field + "' must be " + (allow_zero ? "non-negative" : "positive");
            return false;
        }
        target = v;
        return true;
    };

    // Helper to read an optional int — returns false only if present but invalid.
    // If absent, leaves target unchanged (uses ShapeParams default).
    auto readOptionalInt = [&](const std::string& field, int& target, int min_val, int max_val) -> bool {
        if (!obj.count(field) || !obj.at(field).is<double>()) {
            return true; // absent = use default
        }
        int v = static_cast<int>(obj.at(field).get<double>());
        if (v < min_val || v > max_val) {
            error = "'" + field + "' must be in range [" + std::to_string(min_val) + ", " + std::to_string(max_val) + "]";
            return false;
        }
        target = v;
        return true;
    };

    switch (out->type) {
    case ShapeType::Cone:
        if (!readFloat("radius", out->radius)) return false;
        if (!readFloat("height", out->height)) return false;
        // Subdivision is optional — defaults come from ShapeParams constructor
        if (!readOptionalInt("sides", out->segments, 3, 128)) return false;
        break;

    case ShapeType::Cylinder:
        if (!readFloat("radius", out->radius)) return false;
        if (!readFloat("height", out->height)) return false;
        if (!readOptionalInt("sides", out->segments, 3, 128)) return false;
        break;

    case ShapeType::Sphere:
        if (!readFloat("radius", out->radius)) return false;
        if (!readOptionalInt("slices", out->lonSegments, 4, 128)) return false;
        if (!readOptionalInt("stacks", out->latSegments, 3, 64)) return false;
        break;

    case ShapeType::Torus:
        if (!readFloat("majorRadius", out->majorRadius)) return false;
        if (!readFloat("minorRadius", out->minorRadius)) return false;
        if (!readOptionalInt("ringSegments", out->ringSegments, 3, 128)) return false;
        if (!readOptionalInt("tube_segments", out->sideSegments, 3, 64)) return false;
        if (out->minorRadius >= out->majorRadius) {
            error = "'minorRadius' must be less than 'majorRadius'";
            return false;
        }
        break;

    case ShapeType::Capsule:
        if (!readFloat("radius", out->radius)) return false;
        if (!readFloat("height", out->height)) return false;
        if (!readOptionalInt("sides", out->segments, 3, 128)) return false;
        if (out->height < 2.0f * out->radius) {
            error = "Capsule 'height' must be >= 2 * radius (need room for hemispherical caps)";
            return false;
        }
        break;
    }

    return true;
}

bool BodyLoader::ParseConnectionV1(const void* conn_ptr, Connection* out, std::string& error)
{
    const picojson::object& obj = *static_cast<const picojson::object*>(conn_ptr);
    out->isLegacy = true;

    // Parse type
    if (obj.count("type") && obj.at("type").is<std::string>()) {
        std::string t = obj.at("type").get<std::string>();
        if (t == "FaceConnection") {
            out->type = ConnectionType::FaceConnection;
        } else if (t == "EdgeConnection") {
            out->type = ConnectionType::EdgeConnection;
        } else if (t == "PointConnection") {
            out->type = ConnectionType::PointConnection;
        } else {
            error = "Unknown connection type: '" + t + "'";
            return false;
        }
    }

    // Parse face indices
    if (obj.count("parent_face") && obj.at("parent_face").is<double>()) {
        out->parentFaceIndex = static_cast<int>(obj.at("parent_face").get<double>());
    }
    if (obj.count("child_face") && obj.at("child_face").is<double>()) {
        out->childFaceIndex = static_cast<int>(obj.at("child_face").get<double>());
    }

    // Parse offsets
    if (obj.count("offsetU") && obj.at("offsetU").is<double>()) {
        out->offsetU = static_cast<float>(obj.at("offsetU").get<double>());
    }
    if (obj.count("offsetV") && obj.at("offsetV").is<double>()) {
        out->offsetV = static_cast<float>(obj.at("offsetV").get<double>());
    }

    // Parse rotation
    if (obj.count("rotation") && obj.at("rotation").is<double>()) {
        out->rotation = static_cast<float>(obj.at("rotation").get<double>());
    }

    return true;
}

bool BodyLoader::ParseConnectionV2(const void* conn_ptr, Connection* out, std::string& error)
{
    const picojson::object& obj = *static_cast<const picojson::object*>(conn_ptr);
    out->isLegacy = false;

    // Parse parentAttach
    if (obj.count("parentAttach") && obj.at("parentAttach").is<picojson::object>()) {
        const picojson::object& pa = obj.at("parentAttach").get<picojson::object>();
        if (!ParseAttachmentPoint(static_cast<const void*>(&pa), &out->parentAttach, error)) {
            return false;
        }
    } else {
        error = "Missing 'parentAttach' in v2 connection";
        return false;
    }

    // Parse childAttach
    if (obj.count("childAttach") && obj.at("childAttach").is<picojson::object>()) {
        const picojson::object& ca = obj.at("childAttach").get<picojson::object>();
        if (!ParseAttachmentPoint(static_cast<const void*>(&ca), &out->childAttach, error)) {
            return false;
        }
    } else {
        error = "Missing 'childAttach' in v2 connection";
        return false;
    }

    // Parse rotation
    if (obj.count("rotation") && obj.at("rotation").is<double>()) {
        out->rotation = static_cast<float>(obj.at("rotation").get<double>());
    }

    return true;
}

bool BodyLoader::ParseAttachmentPoint(const void* attach_ptr, AttachmentPoint* out, std::string& error)
{
    const picojson::object& obj = *static_cast<const picojson::object*>(attach_ptr);

    // Parse region
    if (obj.count("region") && obj.at("region").is<std::string>()) {
        std::string r = obj.at("region").get<std::string>();
        if (r == "surface") out->region = AttachRegion::Surface;
        else if (r == "top") out->region = AttachRegion::Top;
        else if (r == "bottom") out->region = AttachRegion::Bottom;
        else if (r == "side") out->region = AttachRegion::Side;
        else if (r == "base") out->region = AttachRegion::Base;
        else if (r == "top_cap") out->region = AttachRegion::TopCap;
        else if (r == "bottom_cap") out->region = AttachRegion::BottomCap;
        else {
            error = "Unknown attachment region: '" + r + "'";
            return false;
        }
    } else {
        error = "Missing 'region' in attachment point";
        return false;
    }

    // Parse u (default 0.5)
    if (obj.count("u") && obj.at("u").is<double>()) {
        out->u = static_cast<float>(obj.at("u").get<double>());
        if (out->u < 0.0f) out->u = 0.0f;
        if (out->u > 1.0f) out->u = 1.0f;
    }

    // Parse v (default 0.5)
    if (obj.count("v") && obj.at("v").is<double>()) {
        out->v = static_cast<float>(obj.at("v").get<double>());
        if (out->v < 0.0f) out->v = 0.0f;
        if (out->v > 1.0f) out->v = 1.0f;
    }

    return true;
}

// ============================================================================
// Serialization (always emits v2 format)
// ============================================================================

static std::string RegionToString(AttachRegion r)
{
    switch (r) {
    case AttachRegion::Surface:   return "surface";
    case AttachRegion::Top:       return "top";
    case AttachRegion::Bottom:    return "bottom";
    case AttachRegion::Side:      return "side";
    case AttachRegion::Base:      return "base";
    case AttachRegion::TopCap:    return "top_cap";
    case AttachRegion::BottomCap: return "bottom_cap";
    }
    return "surface";
}

static picojson::value SerializeAttachmentPoint(const AttachmentPoint& a)
{
    picojson::object obj;
    obj["region"] = picojson::value(RegionToString(a.region));
    obj["u"] = picojson::value(static_cast<double>(a.u));
    obj["v"] = picojson::value(static_cast<double>(a.v));
    return picojson::value(obj);
}

static picojson::value SerializeNode(const BodyNode& node)
{
    picojson::object obj;
    obj["name"] = picojson::value(node.name);

    // Shape — only emit geometric parameters, not subdivision counts.
    // Subdivision is a runtime concern (ApplySubdivision in the viewer).
    picojson::object shape;
    switch (node.shape.type) {
    case ShapeType::Cone:
        shape["type"] = picojson::value(std::string("cone"));
        shape["radius"] = picojson::value(static_cast<double>(node.shape.radius));
        shape["height"] = picojson::value(static_cast<double>(node.shape.height));
        break;
    case ShapeType::Cylinder:
        shape["type"] = picojson::value(std::string("cylinder"));
        shape["radius"] = picojson::value(static_cast<double>(node.shape.radius));
        shape["height"] = picojson::value(static_cast<double>(node.shape.height));
        break;
    case ShapeType::Sphere:
        shape["type"] = picojson::value(std::string("sphere"));
        shape["radius"] = picojson::value(static_cast<double>(node.shape.radius));
        break;
    case ShapeType::Torus:
        shape["type"] = picojson::value(std::string("torus"));
        shape["majorRadius"] = picojson::value(static_cast<double>(node.shape.majorRadius));
        shape["minorRadius"] = picojson::value(static_cast<double>(node.shape.minorRadius));
        break;
    case ShapeType::Capsule:
        shape["type"] = picojson::value(std::string("capsule"));
        shape["radius"] = picojson::value(static_cast<double>(node.shape.radius));
        shape["height"] = picojson::value(static_cast<double>(node.shape.height));
        break;
    }
    obj["shape"] = picojson::value(shape);

    // Color
    picojson::object color;
    color["r"] = picojson::value(static_cast<double>(node.color.x));
    color["g"] = picojson::value(static_cast<double>(node.color.y));
    color["b"] = picojson::value(static_cast<double>(node.color.z));
    obj["color"] = picojson::value(color);

    // Children
    if (!node.children.empty()) {
        picojson::array children;
        for (const auto& child : node.children) {
            picojson::object child_conn_obj;

            // Connection (v2 parametric)
            picojson::object conn;
            conn["parentAttach"] = SerializeAttachmentPoint(child.connection.parentAttach);
            conn["childAttach"] = SerializeAttachmentPoint(child.connection.childAttach);
            conn["rotation"] = picojson::value(static_cast<double>(child.connection.rotation));

            child_conn_obj["connection"] = picojson::value(conn);
            child_conn_obj["node"] = SerializeNode(child);

            children.push_back(picojson::value(child_conn_obj));
        }
        obj["children"] = picojson::value(children);
    }

    return picojson::value(obj);
}

std::string BodyLoader::Serialize(const Body& body) const
{
    picojson::object root;
    root["formatVersion"] = picojson::value(2.0);
    root["name"] = picojson::value(body.name);

    // Material
    picojson::object material;
    material["shininess"] = picojson::value(static_cast<double>(body.material.shininess));
    picojson::object amb;
    amb["r"] = picojson::value(static_cast<double>(body.material.ambient.x));
    amb["g"] = picojson::value(static_cast<double>(body.material.ambient.y));
    amb["b"] = picojson::value(static_cast<double>(body.material.ambient.z));
    material["ambient"] = picojson::value(amb);
    root["material"] = picojson::value(material);

    // Root node
    root["root"] = SerializeNode(body.root);

    picojson::value v(root);
    return v.serialize(true) + "\n";
}

} // namespace BodyRenderer
