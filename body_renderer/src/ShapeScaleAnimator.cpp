#include "ShapeScaleAnimator.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

ShapeScaleAnimator::ShapeScaleAnimator()
    : m_current_shape(0)
    , m_current_dimension(0)
    , m_elapsed(0.0f)
    , m_dimension_duration(2.0f) // 2 seconds per dimension
    , m_enabled(false)
{
}

void ShapeScaleAnimator::SetBody(const Body& body)
{
    m_shapes.clear();
    m_current_shape = 0;
    m_current_dimension = 0;
    m_elapsed = 0.0f;

    std::vector<int> path;
    // Include the root node itself (path empty = root)
    {
        ScalableShape ss;
        ss.path = path;
        ss.name = body.root.name;
        ss.shape_type = body.root.shape.type;

        switch (body.root.shape.type) {
        case ShapeType::Cylinder:
        case ShapeType::Cone:
        case ShapeType::Capsule:
            ss.dimensions.push_back({ScaleDimension::Radius, body.root.shape.radius});
            ss.dimensions.push_back({ScaleDimension::Height, body.root.shape.height});
            break;
        case ShapeType::Sphere:
            ss.dimensions.push_back({ScaleDimension::Radius, body.root.shape.radius});
            break;
        case ShapeType::Torus:
            ss.dimensions.push_back({ScaleDimension::MajorRadius, body.root.shape.major_radius});
            ss.dimensions.push_back({ScaleDimension::MinorRadius, body.root.shape.minor_radius});
            break;
        }

        if (!ss.dimensions.empty()) {
            m_shapes.push_back(ss);
        }
    }

    CollectShapes(&body.root, path);
}

void ShapeScaleAnimator::CollectShapes(const BodyNode* node, std::vector<int>& current_path)
{
    if (!node) return;

    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        current_path.push_back(i);

        const BodyNode& child = node->children[i];
        ScalableShape ss;
        ss.path = current_path;
        ss.name = child.name;
        ss.shape_type = child.shape.type;

        switch (child.shape.type) {
        case ShapeType::Cylinder:
        case ShapeType::Cone:
        case ShapeType::Capsule:
            ss.dimensions.push_back({ScaleDimension::Radius, child.shape.radius});
            ss.dimensions.push_back({ScaleDimension::Height, child.shape.height});
            break;
        case ShapeType::Sphere:
            ss.dimensions.push_back({ScaleDimension::Radius, child.shape.radius});
            break;
        case ShapeType::Torus:
            ss.dimensions.push_back({ScaleDimension::MajorRadius, child.shape.major_radius});
            ss.dimensions.push_back({ScaleDimension::MinorRadius, child.shape.minor_radius});
            break;
        }

        if (!ss.dimensions.empty()) {
            m_shapes.push_back(ss);
        }

        CollectShapes(&child, current_path);
        current_path.pop_back();
    }
}

BodyNode* ShapeScaleAnimator::NavigateToNode(Body& body, const std::vector<int>& path) const
{
    BodyNode* current = &body.root;
    for (size_t i = 0; i < path.size(); ++i) {
        int idx = path[i];
        if (idx < 0 || idx >= static_cast<int>(current->children.size())) {
            return nullptr;
        }
        current = &current->children[idx];
    }
    return current;
}

const BodyNode* ShapeScaleAnimator::NavigateToNodeConst(const Body& body, const std::vector<int>& path) const
{
    const BodyNode* current = &body.root;
    for (size_t i = 0; i < path.size(); ++i) {
        int idx = path[i];
        if (idx < 0 || idx >= static_cast<int>(current->children.size())) {
            return nullptr;
        }
        current = &current->children[idx];
    }
    return current;
}

std::string ShapeScaleAnimator::GetCurrentShapeName() const
{
    if (m_shapes.empty()) return "";
    return m_shapes[m_current_shape].name;
}

std::string ShapeScaleAnimator::GetCurrentDimensionName() const
{
    if (m_shapes.empty()) return "";
    const auto& shape = m_shapes[m_current_shape];
    if (m_current_dimension >= static_cast<int>(shape.dimensions.size())) return "";
    switch (shape.dimensions[m_current_dimension].type) {
    case ScaleDimension::Radius: return "radius";
    case ScaleDimension::Height: return "height";
    case ScaleDimension::MajorRadius: return "major_radius";
    case ScaleDimension::MinorRadius: return "minor_radius";
    }
    return "";
}

bool ShapeScaleAnimator::Update(float dt)
{
    if (!m_enabled || m_shapes.empty()) return false;

    m_elapsed += dt;

    if (m_elapsed >= m_dimension_duration) {
        m_elapsed -= m_dimension_duration;
        // Advance to next dimension, or next shape
        const auto& shape = m_shapes[m_current_shape];
        m_current_dimension++;
        if (m_current_dimension >= static_cast<int>(shape.dimensions.size())) {
            m_current_dimension = 0;
            m_current_shape = (m_current_shape + 1) % static_cast<int>(m_shapes.size());
        }
    }

    return true;
}

void ShapeScaleAnimator::ApplyTo(Body& body) const
{
    if (!m_enabled || m_shapes.empty()) return;

    const auto& shape_info = m_shapes[m_current_shape];
    BodyNode* node = const_cast<ShapeScaleAnimator*>(this)->NavigateToNode(body, shape_info.path);
    if (!node) return;

    // Compute scale factor using sine wave: oscillates between 0.5x and 1.5x
    float t = m_elapsed / m_dimension_duration;
    float scale = 1.0f + 0.5f * std::sin(t * 2.0f * static_cast<float>(M_PI));

    const auto& dim = shape_info.dimensions[m_current_dimension];
    float scaled_value = dim.base_value * scale;

    switch (dim.type) {
    case ScaleDimension::Radius:
        node->shape.radius = scaled_value;
        break;
    case ScaleDimension::Height:
        node->shape.height = scaled_value;
        // For capsule, ensure height >= 2*radius
        if (node->shape.type == ShapeType::Capsule && node->shape.height < 2.0f * node->shape.radius) {
            node->shape.height = 2.0f * node->shape.radius;
        }
        break;
    case ScaleDimension::MajorRadius:
        node->shape.major_radius = scaled_value;
        // Ensure major > minor for torus
        if (node->shape.type == ShapeType::Torus && node->shape.major_radius <= node->shape.minor_radius) {
            node->shape.major_radius = node->shape.minor_radius + 0.01f;
        }
        break;
    case ScaleDimension::MinorRadius:
        node->shape.minor_radius = scaled_value;
        // Ensure minor < major for torus
        if (node->shape.type == ShapeType::Torus && node->shape.minor_radius >= node->shape.major_radius) {
            node->shape.minor_radius = node->shape.major_radius - 0.01f;
        }
        break;
    }
}

void ShapeScaleAnimator::ResetTo(Body& body) const
{
    // Reset all shapes to their base values
    for (const auto& shape_info : m_shapes) {
        BodyNode* node = const_cast<ShapeScaleAnimator*>(this)->NavigateToNode(body, shape_info.path);
        if (!node) continue;

        for (const auto& dim : shape_info.dimensions) {
            switch (dim.type) {
            case ScaleDimension::Radius:
                node->shape.radius = dim.base_value;
                break;
            case ScaleDimension::Height:
                node->shape.height = dim.base_value;
                break;
            case ScaleDimension::MajorRadius:
                node->shape.major_radius = dim.base_value;
                break;
            case ScaleDimension::MinorRadius:
                node->shape.minor_radius = dim.base_value;
                break;
            }
        }
    }
}

} // namespace BodyRenderer
