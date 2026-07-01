#include "BodyLoader.h"
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

    // Parse root node - it can be the object itself if it has "shape", or nested under "root"
    const picojson::object* node_obj = nullptr;
    if (obj.count("root") && obj.at("root").is<picojson::object>()) {
        node_obj = &obj.at("root").get<picojson::object>();
    } else if (obj.count("shape")) {
        // The root object IS the node (flat format from design doc)
        node_obj = &obj;
    }

    if (!node_obj) {
        result.success = false;
        result.error = "Missing 'root' or 'shape' field in body JSON";
        return result;
    }

    std::string parse_err;
    if (!ParseNode(static_cast<const void*>(node_obj), &result.body.root, 0, parse_err)) {
        result.success = false;
        result.error = parse_err;
        return result;
    }

    result.success = true;
    return result;
}

bool BodyLoader::ParseNode(const void* json_obj_ptr, BodyNode* out, int depth, std::string& error)
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

    // Parse color (required)
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
                if (!ParseConnection(static_cast<const void*>(&conn_obj), &child_node.connection, error)) {
                    return false;
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

            if (!ParseNode(static_cast<const void*>(child_node_obj), &child_node, depth + 1, error)) {
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
    if (type_str == "box") {
        out->type = ShapeType::Box;
    } else if (type_str == "cone") {
        out->type = ShapeType::Cone;
    } else if (type_str == "cylinder") {
        out->type = ShapeType::Cylinder;
    } else if (type_str == "sphere") {
        out->type = ShapeType::Sphere;
    } else if (type_str == "torus") {
        out->type = ShapeType::Torus;
    } else if (type_str == "frustum") {
        out->type = ShapeType::Frustum;
    } else {
        error = "Unknown shape type: '" + type_str + "'";
        return false;
    }

    // Helper to read a positive float
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

    auto readInt = [&](const std::string& field, int& target, int min_val, int max_val) -> bool {
        if (!obj.count(field) || !obj.at(field).is<double>()) {
            error = "Missing field '" + field + "' in shape";
            return false;
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
    case ShapeType::Box:
        if (!readFloat("width", out->width)) return false;
        if (!readFloat("height", out->height)) return false;
        if (!readFloat("depth", out->depth)) return false;
        break;

    case ShapeType::Cone:
        if (!readFloat("radius", out->radius)) return false;
        if (!readFloat("height", out->height)) return false;
        if (!readInt("sides", out->segments, 3, 128)) return false;
        break;

    case ShapeType::Cylinder:
        if (!readFloat("radius", out->radius)) return false;
        if (!readFloat("height", out->height)) return false;
        if (!readInt("sides", out->segments, 3, 128)) return false;
        break;

    case ShapeType::Sphere:
        if (!readFloat("radius", out->radius)) return false;
        if (!readInt("slices", out->lon_segments, 4, 128)) return false;
        if (!readInt("stacks", out->lat_segments, 3, 64)) return false;
        break;

    case ShapeType::Torus:
        if (!readFloat("major_radius", out->major_radius)) return false;
        if (!readFloat("minor_radius", out->minor_radius)) return false;
        if (!readInt("ring_segments", out->ring_segments, 3, 128)) return false;
        if (!readInt("tube_segments", out->side_segments, 3, 64)) return false;
        if (out->minor_radius >= out->major_radius) {
            error = "'minor_radius' must be less than 'major_radius'";
            return false;
        }
        break;

    case ShapeType::Frustum:
        if (!readFloat("top_radius", out->top_radius, true)) return false;
        if (!readFloat("bottom_radius", out->bottom_radius)) return false;
        if (!readFloat("height", out->height)) return false;
        if (!readInt("sides", out->segments, 3, 128)) return false;
        if (out->top_radius >= out->bottom_radius) {
            error = "'bottom_radius' must be greater than 'top_radius'";
            return false;
        }
        break;
    }

    return true;
}

bool BodyLoader::ParseConnection(const void* conn_ptr, Connection* out, std::string& error)
{
    const picojson::object& obj = *static_cast<const picojson::object*>(conn_ptr);

    // Parse type
    if (obj.count("type") && obj.at("type").is<std::string>()) {
        std::string t = obj.at("type").get<std::string>();
        if (t == "Face_Connection") {
            out->type = ConnectionType::Face_Connection;
        } else if (t == "Edge_Connection") {
            out->type = ConnectionType::Edge_Connection;
        } else if (t == "Point_Connection") {
            out->type = ConnectionType::Point_Connection;
        } else {
            error = "Unknown connection type: '" + t + "'";
            return false;
        }
    }

    // Parse face indices
    if (obj.count("parent_face") && obj.at("parent_face").is<double>()) {
        out->parent_face_index = static_cast<int>(obj.at("parent_face").get<double>());
    }
    if (obj.count("child_face") && obj.at("child_face").is<double>()) {
        out->child_face_index = static_cast<int>(obj.at("child_face").get<double>());
    }

    // Parse offsets
    if (obj.count("offset_u") && obj.at("offset_u").is<double>()) {
        out->offset_u = static_cast<float>(obj.at("offset_u").get<double>());
    }
    if (obj.count("offset_v") && obj.at("offset_v").is<double>()) {
        out->offset_v = static_cast<float>(obj.at("offset_v").get<double>());
    }

    // Parse rotation
    if (obj.count("rotation") && obj.at("rotation").is<double>()) {
        out->rotation = static_cast<float>(obj.at("rotation").get<double>());
    }

    return true;
}

// ============================================================================
// Serialization
// ============================================================================

static picojson::value SerializeNode(const BodyNode& node)
{
    picojson::object obj;
    obj["name"] = picojson::value(node.name);

    // Shape
    picojson::object shape;
    switch (node.shape.type) {
    case ShapeType::Box:
        shape["type"] = picojson::value(std::string("box"));
        shape["width"] = picojson::value(static_cast<double>(node.shape.width));
        shape["height"] = picojson::value(static_cast<double>(node.shape.height));
        shape["depth"] = picojson::value(static_cast<double>(node.shape.depth));
        break;
    case ShapeType::Cone:
        shape["type"] = picojson::value(std::string("cone"));
        shape["radius"] = picojson::value(static_cast<double>(node.shape.radius));
        shape["height"] = picojson::value(static_cast<double>(node.shape.height));
        shape["sides"] = picojson::value(static_cast<double>(node.shape.segments));
        break;
    case ShapeType::Cylinder:
        shape["type"] = picojson::value(std::string("cylinder"));
        shape["radius"] = picojson::value(static_cast<double>(node.shape.radius));
        shape["height"] = picojson::value(static_cast<double>(node.shape.height));
        shape["sides"] = picojson::value(static_cast<double>(node.shape.segments));
        break;
    case ShapeType::Sphere:
        shape["type"] = picojson::value(std::string("sphere"));
        shape["radius"] = picojson::value(static_cast<double>(node.shape.radius));
        shape["slices"] = picojson::value(static_cast<double>(node.shape.lon_segments));
        shape["stacks"] = picojson::value(static_cast<double>(node.shape.lat_segments));
        break;
    case ShapeType::Torus:
        shape["type"] = picojson::value(std::string("torus"));
        shape["major_radius"] = picojson::value(static_cast<double>(node.shape.major_radius));
        shape["minor_radius"] = picojson::value(static_cast<double>(node.shape.minor_radius));
        shape["ring_segments"] = picojson::value(static_cast<double>(node.shape.ring_segments));
        shape["tube_segments"] = picojson::value(static_cast<double>(node.shape.side_segments));
        break;
    case ShapeType::Frustum:
        shape["type"] = picojson::value(std::string("frustum"));
        shape["top_radius"] = picojson::value(static_cast<double>(node.shape.top_radius));
        shape["bottom_radius"] = picojson::value(static_cast<double>(node.shape.bottom_radius));
        shape["height"] = picojson::value(static_cast<double>(node.shape.height));
        shape["sides"] = picojson::value(static_cast<double>(node.shape.segments));
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

            // Connection
            picojson::object conn;
            switch (child.connection.type) {
            case ConnectionType::Face_Connection:
                conn["type"] = picojson::value(std::string("Face_Connection"));
                break;
            case ConnectionType::Edge_Connection:
                conn["type"] = picojson::value(std::string("Edge_Connection"));
                break;
            case ConnectionType::Point_Connection:
                conn["type"] = picojson::value(std::string("Point_Connection"));
                break;
            }
            conn["parent_face"] = picojson::value(static_cast<double>(child.connection.parent_face_index));
            conn["child_face"] = picojson::value(static_cast<double>(child.connection.child_face_index));
            conn["offset_u"] = picojson::value(static_cast<double>(child.connection.offset_u));
            conn["offset_v"] = picojson::value(static_cast<double>(child.connection.offset_v));
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
