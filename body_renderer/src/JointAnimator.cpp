#include "JointAnimator.h"
#include <cmath>
#include <algorithm>

namespace BodyRenderer {

JointAnimator::JointAnimator()
    : m_current_joint(0)
    , m_elapsed(0.0f)
    , m_step_duration(0.5f)   // 0.5 seconds to slerp between adjacent faces
    , m_joint_duration(4.0f)  // 4 seconds total per joint (cycle through ring)
    , m_enabled(false)
    , m_current_ring_pos(0)
{
}

void JointAnimator::SetBody(const Body& body)
{
    m_joints.clear();
    m_current_joint = 0;
    m_elapsed = 0.0f;
    m_current_ring_pos = 0;

    std::vector<int> path;
    CollectJoints(&body.root, path);
}

JointAnimator::FaceRing JointAnimator::BuildFaceRing(const ShapeParams& parent_shape, int start_face_index) const
{
    FaceRing ring;

    FaceGenerator faceGen;
    std::vector<Face> faces = faceGen.Generate(parent_shape);

    if (faces.empty()) {
        ring.face_indices.push_back(start_face_index);
        return ring;
    }

    // Clamp start_face_index
    if (start_face_index < 0 || start_face_index >= static_cast<int>(faces.size())) {
        start_face_index = 0;
    }

    // Find all faces with the same vertex count as the starting face.
    // These form the "ring" — they represent adjacent subdivision faces
    // along one dimension (e.g., lateral quads of a cylinder, or ring quads of a torus).
    size_t target_verts = faces[start_face_index].vertices.size();

    for (int i = 0; i < static_cast<int>(faces.size()); ++i) {
        if (faces[i].vertices.size() == target_verts) {
            ring.face_indices.push_back(i);
        }
    }

    // Ensure ring contains at least the start face
    if (ring.face_indices.empty()) {
        ring.face_indices.push_back(start_face_index);
    }

    return ring;
}

void JointAnimator::CollectJoints(const BodyNode* node, std::vector<int>& current_path)
{
    if (!node) return;

    for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
        current_path.push_back(i);

        const BodyNode& child = node->children[i];

        // Only animate Face_Connection joints (these are the shared-face joints)
        if (child.connection.type == ConnectionType::Face_Connection) {
            JointInfo info;
            info.path = current_path;
            info.name = child.name;
            info.conn_type = child.connection.type;
            info.original_parent_face_index = child.connection.parent_face_index;
            info.original_child_face_index = child.connection.child_face_index;
            info.ring = BuildFaceRing(node->shape, child.connection.parent_face_index);
            m_joints.push_back(info);
        }

        // Recurse into children
        CollectJoints(&child, current_path);

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

BodyNode* JointAnimator::NavigateToParent(Body& body, const std::vector<int>& path) const
{
    if (path.empty()) return nullptr;

    BodyNode* current = &body.root;
    for (size_t i = 0; i < path.size() - 1; ++i) {
        int idx = path[i];
        if (idx < 0 || idx >= static_cast<int>(current->children.size())) {
            return nullptr;
        }
        current = &current->children[idx];
    }
    return current;
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

    // Total time for this joint: step through entire ring and back
    if (m_elapsed >= m_joint_duration) {
        m_elapsed -= m_joint_duration;
        m_current_joint = (m_current_joint + 1) % static_cast<int>(m_joints.size());
        m_current_ring_pos = 0;
    }

    return true;
}

void JointAnimator::ApplyTo(Body& body) const
{
    if (!m_enabled || m_joints.empty()) return;

    const JointInfo& info = m_joints[m_current_joint];
    BodyNode* node = const_cast<JointAnimator*>(this)->NavigateToNode(body, info.path);
    if (!node) return;

    const FaceRing& ring = info.ring;
    if (ring.face_indices.empty()) return;

    int ring_size = static_cast<int>(ring.face_indices.size());
    if (ring_size <= 1) {
        // Only one face in ring — nothing to animate, keep original
        node->connection.parent_face_index = info.original_parent_face_index;
        return;
    }

    // Compute which face in the ring we should currently be at.
    // We step through the ring over the joint_duration: forward then backward.
    // Using ping-pong: 0 → ring_size-1 → 0 over the full duration.
    float progress = m_elapsed / m_joint_duration; // 0.0 to 1.0

    // Ping-pong: first half goes forward (0 → ring_size-1), second half backward
    float ping_pong;
    if (progress < 0.5f) {
        ping_pong = progress * 2.0f; // 0.0 → 1.0
    } else {
        ping_pong = (1.0f - progress) * 2.0f; // 1.0 → 0.0
    }

    // Map to ring index (continuous for smooth slerp)
    float continuous_idx = ping_pong * static_cast<float>(ring_size - 1);
    int face_idx = static_cast<int>(continuous_idx);
    if (face_idx >= ring_size - 1) face_idx = ring_size - 2;
    if (face_idx < 0) face_idx = 0;

    // The interpolation fraction between face_idx and face_idx+1
    // is used by setting the parent_face_index to the nearest discrete step.
    // Since faces are discrete geometry, we snap to the closest face.
    float frac = continuous_idx - static_cast<float>(face_idx);
    int target_ring_idx = (frac < 0.5f) ? face_idx : face_idx + 1;
    if (target_ring_idx >= ring_size) target_ring_idx = ring_size - 1;

    node->connection.parent_face_index = ring.face_indices[target_ring_idx];

    // Keep child face index the same (it's the "socket" on the child)
    node->connection.child_face_index = info.original_child_face_index;

    // Reset rotation to 0 for clean face-to-face alignment during animation
    node->connection.rotation = 0.0f;
}

} // namespace BodyRenderer
