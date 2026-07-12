#pragma once

#include "BodyTypes.h"
#include <vector>
#include <string>

namespace BodyRenderer {

// Animates connection joints one at a time, cycling through each joint's
// degrees of freedom using spherical linear interpolation (slerp on the
// rotation angle). Each joint is animated for a configurable duration,
// then the animator moves to the next joint, eventually cycling back.
class JointAnimator {
public:
    JointAnimator();

    // Initialize with a body — builds the list of animatable joints
    void SetBody(const Body& body);

    // Advance the animation by dt seconds. Returns true if pose changed.
    bool Update(float dt);

    // Apply current animation offsets to a body's transforms (modifies in-place).
    // Call after ConnectionSolver::ResolveTree to overlay animation.
    void ApplyTo(Body& body) const;

    // Enable / disable animation
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Duration each joint animates (seconds)
    void SetJointDuration(float secs) { m_joint_duration = secs; }
    float GetJointDuration() const { return m_joint_duration; }

    // Get info about current animation state
    int GetCurrentJointIndex() const { return m_current_joint; }
    int GetJointCount() const { return static_cast<int>(m_joints.size()); }
    std::string GetCurrentJointName() const;

private:
    struct JointInfo {
        // Path from root to this node (indices into children arrays)
        std::vector<int> path;
        std::string name;
        ConnectionType connType;
        float baseRotation; // original rotation value from the connection
    };

    void CollectJoints(const BodyNode* node, std::vector<int>& current_path);
    BodyNode* NavigateToNode(Body& body, const std::vector<int>& path) const;
    float Slerp(float from_angle, float to_angle, float t) const;

    std::vector<JointInfo> m_joints;
    int m_current_joint;
    float m_elapsed;        // time into current joint's animation
    float m_joint_duration; // seconds per joint
    bool m_enabled;
    bool m_animating_forward; // direction within one joint cycle
};

} // namespace BodyRenderer
