#pragma once

#include "BodyTypes.h"
#include <vector>
#include <string>

namespace BodyRenderer {

// Animates individual shape scale parameters (radius, height, etc.)
// independently from joint rotation animation. Cycles through each shape
// in the body, animating its scalable dimensions one at a time.
class ShapeScaleAnimator {
public:
    ShapeScaleAnimator();

    // Initialize with a body — builds the list of scalable shapes
    void SetBody(const Body& body);

    // Advance the animation by dt seconds. Returns true if scale changed.
    bool Update(float dt);

    // Apply current scale offsets to a body's shape params (modifies in-place).
    // Call BEFORE ConnectionSolver::ResolveTree since scale affects geometry.
    void ApplyTo(Body& body) const;

    // Reset all shapes to their base values (undo animation)
    void ResetTo(Body& body) const;

    // Enable / disable animation
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Duration each dimension animates (seconds)
    void SetDimensionDuration(float secs) { m_dimension_duration = secs; }
    float GetDimensionDuration() const { return m_dimension_duration; }

    // Get info about current animation state
    int GetCurrentShapeIndex() const { return m_current_shape; }
    int GetShapeCount() const { return static_cast<int>(m_shapes.size()); }
    std::string GetCurrentShapeName() const;
    std::string GetCurrentDimensionName() const;

private:
    struct ScaleDimension {
        enum Type { Radius, Height, MajorRadius, MinorRadius };
        Type type;
        float base_value;
    };

    struct ScalableShape {
        std::vector<int> path; // path from root to this node
        std::string name;
        ShapeType shape_type;
        std::vector<ScaleDimension> dimensions;
    };

    void CollectShapes(const BodyNode* node, std::vector<int>& current_path);
    BodyNode* NavigateToNode(Body& body, const std::vector<int>& path) const;
    const BodyNode* NavigateToNodeConst(const Body& body, const std::vector<int>& path) const;

    std::vector<ScalableShape> m_shapes;
    int m_current_shape;
    int m_current_dimension;
    float m_elapsed;
    float m_dimension_duration; // seconds per dimension
    bool m_enabled;
};

} // namespace BodyRenderer
