#include "JointAnimator.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

JointAnimator::JointAnimator()
    : m_current_joint(0)
    , m_elapsed(0.0f)
    , m_joint_duration(3.0f) // 3 seconds per joint
    , m_enabled(false)
    , m_animating_forward(true)
{
}

void JointAnimator::SetBody(const Body& body)
{
    m_joints.clear();
    m_current_joint = 0;
    m_elapsed = 0.0f;
    m_animating_forward = true;

    std::vector<int> path;
    CollectJoints(&body.root, path);
}

void JointAnimator::CollectJoints(const BodyNode* node, std::vector<int>& current_path)
{
    if (!node) return;

    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        current_path.push_back(i);

        // Every child with a connection is an animatable joint
        JointInfo info;
        info.path = current_path;
        info.name = node->children[i].name;
        info.conn_type = node->children[i].connection.type;
        info.base_rotation = node->children[i].connection.rotation;
        m_joints.push_back(info);

        // Recurse into children
        CollectJoints(&node->children[i], current_path);

        current_path.pop_back();
    }
}

BodyNode* JointAnimator::NavigateToNode(Body& body, const std::vector<int>& path) const
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

float JointAnimator::Slerp(float from_angle, float to_angle, float t) const
{
    // Spherical linear interpolation on rotation angles (degrees)
    // Wrap difference to shortest path
    float diff = to_angle - from_angle;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;

    return from_angle + diff * t;
}

std::string JointAnimator::GetCurrentJointName() const
{
    if (m_joints.empty()) return "";
    return m_joints[m_current_joint].name;
}

bool JointAnimator::Update(float dt)
{
    if (!m_enabled || m_joints.empty()) return false;

    m_elapsed += dt;

    // Each joint gets m_joint_duration seconds of animation:
    // First half: rotate from base to base+90
    // Second half: rotate back from base+90 to base
    if (m_elapsed >= m_joint_duration) {
        m_elapsed -= m_joint_duration;
        m_current_joint = (m_current_joint + 1) % static_cast<int>(m_joints.size());
    }

    return true;
}

void JointAnimator::ApplyTo(Body& body) const
{
    if (!m_enabled || m_joints.empty()) return;

    // Only the current joint gets animated
    const JointInfo& info = m_joints[m_current_joint];
    BodyNode* node = const_cast<JointAnimator*>(this)->NavigateToNode(body, info.path);
    if (!node) return;

    // Compute animation progress — use smooth sine curve for slerp feel
    float t = m_elapsed / m_joint_duration;
    // Smoothstep easing: go to +90 and back
    float anim_t = std::sin(t * static_cast<float>(M_PI) * 2.0f); // -1 to 1 over full cycle
    // Map to rotation offset: [-90, +90] degrees
    float rotation_offset = anim_t * 90.0f;

    node->connection.rotation = info.base_rotation + rotation_offset;
}

} // namespace BodyRenderer
