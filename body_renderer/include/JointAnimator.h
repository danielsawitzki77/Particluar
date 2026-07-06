#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"
#include <vector>
#include <string>

namespace BodyRenderer {

// Animates connection joints one at a time by stepping the parent_face_index
// through adjacent subdivision faces along one dimension. Each step is
// interpolated using spherical linear interpolation (slerp) for smooth motion.
// After cycling through all faces of the current joint, moves to the next joint.
class JointAnimator {
public:
    JointAnimator();

    // Initialize with a body — builds the list of animatable joints
    void SetBody(const Body& body);

    // Advance the animation by dt seconds. Returns true if pose changed.
    bool Update(float dt);

    // Apply current animation state to a body's connection indices (modifies in-place).
    // After calling this, ConnectionSolver::ResolveTree must be called to recompute transforms.
    void ApplyTo(Body& body) const;

    // Enable / disable animation
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Duration each face-step takes (seconds)
    void SetStepDuration(float secs) { m_step_duration = secs; }
    float GetStepDuration() const { return m_step_duration; }

    // Duration to pause at each joint before moving to next (seconds)
    void SetJointDuration(float secs) { m_joint_duration = secs; }
    float GetJointDuration() const { return m_joint_duration; }

    // Get info about current animation state
    int GetCurrentJointIndex() const { return m_current_joint; }
    int GetJointCount() const { return static_cast<int>(m_joints.size()); }
    std::string GetCurrentJointName() const;

private:
    struct FaceRing {
        // A ring of face indices that share the same vertex count and are adjacent
        // along one subdivision dimension (e.g., lateral faces of a cylinder)
        std::vector<int> face_indices;
    };

    struct JointInfo {
        // Path from root to this node (indices into children arrays)
        std::vector<int> path;
        std::string name;
        ConnectionType conn_type;
        int original_parent_face_index;
        int original_child_face_index;
        FaceRing ring; // the ring of parent faces to step through
    };

    void CollectJoints(const BodyNode* node, std::vector<int>& current_path);
    BodyNode* NavigateToNode(Body& body, const std::vector<int>& path) const;
    BodyNode* NavigateToParent(Body& body, const std::vector<int>& path) const;
    FaceRing BuildFaceRing(const ShapeParams& parent_shape, int start_face_index) const;

    std::vector<JointInfo> m_joints;
    int m_current_joint;
    float m_elapsed;         // time into current joint's full animation cycle
    float m_step_duration;   // seconds per face step (slerp between two faces)
    float m_joint_duration;  // total seconds spent on one joint before moving on
    bool m_enabled;
    int m_current_ring_pos;  // current position in the ring being animated
};

} // namespace BodyRenderer
