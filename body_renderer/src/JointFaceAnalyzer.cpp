#include "JointFaceAnalyzer.h"
#include "BodyLoader.h"
#include "SubdivisionSolver.h"
#include "ConnectionSolver.h"
#include "ConnectionFaceMatcher.h"
#include "ConnectionValidator.h"
#include "FaceGenerator.h"
#include "ParametricResolver.h"
#include "ModelSwitcher.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace BodyRenderer {

// ============================================================================
// Helper: compute polygon area via cross product summation
// ============================================================================

float JointFaceAnalyzer::ComputeFaceArea(const Face& face) const
{
    if (face.vertices.size() < 3) return 0.0f;

    // Use Newell's method generalized for 3D polygon
    Vec3 cross_sum(0, 0, 0);
    int n = static_cast<int>(face.vertices.size());
    for (int i = 0; i < n; ++i) {
        const Vec3& curr = face.vertices[i];
        const Vec3& next = face.vertices[(i + 1) % n];
        cross_sum = cross_sum + curr.Cross(next);
    }
    return cross_sum.Length() * 0.5f;
}

// ============================================================================
// Vertex matching: find optimal pairing between two vertex sets
// ============================================================================

void JointFaceAnalyzer::ComputeVertexMatching(
    const std::vector<Vec3>& parent_verts_world,
    const std::vector<Vec3>& child_verts_world,
    float& out_sum_distance,
    float& out_max_distance) const
{
    out_sum_distance = 0.0f;
    out_max_distance = 0.0f;

    if (parent_verts_world.empty() || child_verts_world.empty()) return;

    // For each parent vertex, find the closest child vertex (greedy matching).
    // This is a simplification of optimal assignment but works well for
    // faces that should be identical.
    for (const auto& pv : parent_verts_world) {
        float best_dist = 1e9f;
        for (const auto& cv : child_verts_world) {
            float d = (pv - cv).Length();
            if (d < best_dist) best_dist = d;
        }
        out_sum_distance += best_dist;
        if (best_dist > out_max_distance) out_max_distance = best_dist;
    }
}

// ============================================================================
// Quality grading
// ============================================================================

JointFaceReport::Grade JointFaceAnalyzer::ComputeGrade(const JointFaceReport& report) const
{
    // PERFECT: faces are virtually identical (sub-epsilon on all metrics)
    if (report.face_center_distance < 0.001f &&
        report.normal_dot_product < -0.999f &&
        report.vertex_count_match &&
        report.max_vertex_distance < 0.001f &&
        report.area_ratio > 0.999f) {
        return JointFaceReport::PERFECT;
    }

    // GOOD: faces align well, small numerical differences
    if (report.face_center_distance < 0.01f &&
        report.normal_dot_product < -0.99f &&
        report.vertex_count_match &&
        report.max_vertex_distance < 0.02f &&
        report.area_ratio > 0.95f) {
        return JointFaceReport::GOOD;
    }

    // ACCEPTABLE: reasonable alignment, some deformation
    if (report.face_center_distance < 0.05f &&
        report.normal_dot_product < -0.95f &&
        report.area_ratio > 0.8f) {
        return JointFaceReport::ACCEPTABLE;
    }

    // POOR: significant mismatch but connected
    if (report.face_center_distance < 0.2f &&
        report.normal_dot_product < -0.8f) {
        return JointFaceReport::POOR;
    }

    return JointFaceReport::FAILING;
}

// ============================================================================
// Recursive joint analysis
// ============================================================================

static Vec3 ComputeFaceCenter(const Face& face)
{
    Vec3 center(0, 0, 0);
    if (face.vertices.empty()) return center;
    for (const auto& v : face.vertices) {
        center = center + v;
    }
    return center * (1.0f / static_cast<float>(face.vertices.size()));
}

void JointFaceAnalyzer::AnalyzeNodeRecursive(
    const BodyNode* node,
    const BodyNode* parent,
    const Mat4& parent_world,
    int subdivision_level,
    std::vector<JointFaceReport>& out_reports) const
{
    if (!node) return;

    Mat4 world = parent_world * node->local_transform;

    // If this is a child node, analyze the joint connection
    if (parent && !node->connection.is_legacy) {
        JointFaceReport report;
        report.parent_name = parent->name;
        report.child_name = node->name;
        report.subdivision_level = subdivision_level;

        ConnectionFaceMatcher faceMatcher;
        ParametricResolver resolver;

        // --- Parent side: find the connection face on the parent ---
        // Build parent rings (same logic as renderer)
        std::vector<ConnectionRing> parent_rings;
        for (int i = 0; i < static_cast<int>(parent->children.size()); ++i) {
            const BodyNode& sibling = parent->children[i];
            if (sibling.connection.is_legacy) continue;

            float mr = faceMatcher.ComputeMatchedRadius(
                parent->shape, sibling.connection.parent_attach,
                sibling.shape, sibling.connection.child_attach);
            int ms = faceMatcher.ComputeMatchedSegments(
                parent->shape, sibling.connection.parent_attach,
                sibling.shape, sibling.connection.child_attach);

            SurfacePoint pt = resolver.Resolve(parent->shape, sibling.connection.parent_attach);
            ConnectionRing ring;
            ring.center = pt.position;
            ring.normal = pt.normal;
            ring.radius = mr;
            ring.segments = ms;
            ring.child_index = i;
            ring.attach = sibling.connection.parent_attach;
            parent_rings.push_back(ring);
        }

        MatchedFaces parent_matched = faceMatcher.GenerateWithConnections(*parent, parent_rings);

        // Find which parent ring corresponds to this child
        int my_index = -1;
        for (int i = 0; i < static_cast<int>(parent->children.size()); ++i) {
            if (&parent->children[i] == node) { my_index = i; break; }
        }

        int parent_ring_idx = -1;
        for (int i = 0; i < static_cast<int>(parent_rings.size()); ++i) {
            if (parent_rings[i].child_index == my_index) {
                parent_ring_idx = i;
                break;
            }
        }

        // --- Child side: find the connection face on the child ---
        // Build child rings (own-attach is ring index 0)
        std::vector<ConnectionRing> child_rings;
        {
            float mr = faceMatcher.ComputeMatchedRadius(
                parent->shape, node->connection.parent_attach,
                node->shape, node->connection.child_attach);
            int ms = faceMatcher.ComputeMatchedSegments(
                parent->shape, node->connection.parent_attach,
                node->shape, node->connection.child_attach);
            SurfacePoint child_pt = resolver.Resolve(node->shape, node->connection.child_attach);
            ConnectionRing ring;
            ring.center = child_pt.position;
            ring.normal = child_pt.normal;
            ring.radius = mr;
            ring.segments = ms;
            ring.child_index = -1;
            ring.attach = node->connection.child_attach;
            child_rings.push_back(ring);
        }
        // Also add rings for this node's own children
        for (int i = 0; i < static_cast<int>(node->children.size()); ++i) {
            const BodyNode& grandchild = node->children[i];
            if (grandchild.connection.is_legacy) continue;

            float mr = faceMatcher.ComputeMatchedRadius(
                node->shape, grandchild.connection.parent_attach,
                grandchild.shape, grandchild.connection.child_attach);
            int ms = faceMatcher.ComputeMatchedSegments(
                node->shape, grandchild.connection.parent_attach,
                grandchild.shape, grandchild.connection.child_attach);

            SurfacePoint pt = resolver.Resolve(node->shape, grandchild.connection.parent_attach);
            ConnectionRing ring;
            ring.center = pt.position;
            ring.normal = pt.normal;
            ring.radius = mr;
            ring.segments = ms;
            ring.child_index = i;
            ring.attach = grandchild.connection.parent_attach;
            child_rings.push_back(ring);
        }

        MatchedFaces child_matched = faceMatcher.GenerateWithConnections(*node, child_rings);

        // Extract parent connection face data
        if (parent_ring_idx >= 0 &&
            parent_ring_idx < static_cast<int>(parent_matched.connection_face_indices.size())) {
            int pfi = parent_matched.connection_face_indices[parent_ring_idx];
            if (pfi >= 0 && pfi < static_cast<int>(parent_matched.faces.size())) {
                const Face& pface = parent_matched.faces[pfi];
                report.parent_face_index = pfi;
                report.parent_face_vertex_count = static_cast<int>(pface.vertices.size());
                report.parent_face_area = ComputeFaceArea(pface);

                Vec3 pcenter_local = ComputeFaceCenter(pface);
                report.parent_face_center_world = parent_world.TransformPoint(pcenter_local);
                report.parent_face_normal_world = parent_world.TransformDirection(pface.normal).Normalized();
            }
        }

        // Extract child connection face data (ring index 0 = own-attach)
        if (!child_matched.connection_face_indices.empty()) {
            int cfi = child_matched.connection_face_indices[0];
            if (cfi >= 0 && cfi < static_cast<int>(child_matched.faces.size())) {
                const Face& cface = child_matched.faces[cfi];
                report.child_face_index = cfi;
                report.child_face_vertex_count = static_cast<int>(cface.vertices.size());
                report.child_face_area = ComputeFaceArea(cface);

                Vec3 ccenter_local = ComputeFaceCenter(cface);
                report.child_face_center_world = world.TransformPoint(ccenter_local);
                report.child_face_normal_world = world.TransformDirection(cface.normal).Normalized();
            }
        }

        // --- Compute quality metrics ---
        Vec3 diff = report.parent_face_center_world - report.child_face_center_world;
        report.face_center_distance = diff.Length();
        report.normal_dot_product = report.parent_face_normal_world.Dot(report.child_face_normal_world);

        report.area_difference = std::fabs(report.parent_face_area - report.child_face_area);
        float max_area = (std::max)(report.parent_face_area, report.child_face_area);
        float min_area = (std::min)(report.parent_face_area, report.child_face_area);
        report.area_ratio = (max_area > 1e-8f) ? (min_area / max_area) : 0.0f;

        report.vertex_count_match = (report.parent_face_vertex_count == report.child_face_vertex_count);

        // Vertex matching in world space
        if (parent_ring_idx >= 0 &&
            parent_ring_idx < static_cast<int>(parent_matched.connection_face_indices.size()) &&
            !child_matched.connection_face_indices.empty()) {
            int pfi = parent_matched.connection_face_indices[parent_ring_idx];
            int cfi = child_matched.connection_face_indices[0];

            if (pfi >= 0 && pfi < static_cast<int>(parent_matched.faces.size()) &&
                cfi >= 0 && cfi < static_cast<int>(child_matched.faces.size())) {
                // Transform vertices to world space
                std::vector<Vec3> parent_verts_world;
                for (const auto& v : parent_matched.faces[pfi].vertices) {
                    parent_verts_world.push_back(parent_world.TransformPoint(v));
                }
                std::vector<Vec3> child_verts_world;
                for (const auto& v : child_matched.faces[cfi].vertices) {
                    child_verts_world.push_back(world.TransformPoint(v));
                }

                ComputeVertexMatching(parent_verts_world, child_verts_world,
                                      report.vertex_sum_distance, report.max_vertex_distance);
            }
        }

        // Grade
        report.grade = ComputeGrade(report);

        out_reports.push_back(report);
    }

    // Recurse into children
    for (const auto& child : node->children) {
        AnalyzeNodeRecursive(&child, node, world, subdivision_level, out_reports);
    }
}

// ============================================================================
// Public API
// ============================================================================

std::vector<JointFaceReport> JointFaceAnalyzer::AnalyzeBody(
    const Body& body, int subdivision_level) const
{
    // Make a mutable copy so we can prepare it
    Body prepared = body;

    SubdivisionSolver subdivSolver;
    subdivSolver.PrepareBody(prepared, subdivision_level * 8);

    std::vector<JointFaceReport> reports;
    Mat4 identity;
    identity.Identity();
    AnalyzeNodeRecursive(&prepared.root, nullptr, identity, subdivision_level, reports);
    return reports;
}

std::vector<ModelAnalysisReport> JointFaceAnalyzer::AnalyzeDirectory(
    const std::string& body_dir,
    const std::vector<int>& subdivision_levels) const
{
    std::vector<ModelAnalysisReport> results;

    ModelSwitcher switcher;
    if (!switcher.LoadDirectory(body_dir)) {
        return results;
    }

    int model_count = switcher.GetCount();
    BodyLoader loader;
    ConnectionValidator validator;

    for (int m = 0; m < model_count; ++m) {
        std::string path = switcher.GetCurrentPath();

        LoadResult lr = loader.LoadFromFile(path);
        if (!lr.success) {
            printf("[JointFaceAnalyzer] SKIP %s: load failed (%s)\n",
                   path.c_str(), lr.error.c_str());
            switcher.Next();
            continue;
        }

        auto vr = validator.ValidateBody(lr.body);
        if (!vr.valid) {
            printf("[JointFaceAnalyzer] SKIP %s: validation failed (%s)\n",
                   path.c_str(), vr.error.c_str());
            switcher.Next();
            continue;
        }

        ModelAnalysisReport model_report;
        model_report.model_name = lr.body.name;
        model_report.model_path = path;
        model_report.joint_count = 0;

        printf("[JointFaceAnalyzer] Analyzing: %s\n", lr.body.name.c_str());

        for (int level : subdivision_levels) {
            auto joint_reports = AnalyzeBody(lr.body, level);
            if (model_report.joint_count == 0) {
                model_report.joint_count = static_cast<int>(joint_reports.size());
            }
            for (auto& jr : joint_reports) {
                model_report.joints.push_back(jr);
            }
        }

        results.push_back(model_report);
        switcher.Next();
    }

    return results;
}

// ============================================================================
// Markdown report generation
// ============================================================================

static const char* GradeString(JointFaceReport::Grade g)
{
    switch (g) {
    case JointFaceReport::PERFECT:    return "PERFECT";
    case JointFaceReport::GOOD:       return "GOOD";
    case JointFaceReport::ACCEPTABLE: return "ACCEPTABLE";
    case JointFaceReport::POOR:       return "POOR";
    case JointFaceReport::FAILING:    return "FAILING";
    }
    return "UNKNOWN";
}

static const char* GradeEmoji(JointFaceReport::Grade g)
{
    switch (g) {
    case JointFaceReport::PERFECT:    return "\xe2\x9c\x85"; // checkmark
    case JointFaceReport::GOOD:       return "\xf0\x9f\x9f\xa2"; // green circle
    case JointFaceReport::ACCEPTABLE: return "\xf0\x9f\x9f\xa1"; // yellow circle
    case JointFaceReport::POOR:       return "\xf0\x9f\x9f\xa0"; // orange circle
    case JointFaceReport::FAILING:    return "\xe2\x9d\x8c"; // red X
    }
    return "?";
}

bool JointFaceAnalyzer::WriteReport(
    const std::vector<ModelAnalysisReport>& reports,
    const std::string& output_path) const
{
    std::ofstream out(output_path);
    if (!out.is_open()) return false;

    out << "# Joint Face Matching Analysis Report\n\n";
    out << "This report analyzes the quality of face matching at body joints across all models\n";
    out << "at multiple subdivision levels. The goal is to verify that connected faces are\n";
    out << "identical (matching vertex count, area, position, and opposing normals).\n\n";

    // Summary table
    out << "## Summary\n\n";
    out << "| Model | Joints | Perfect | Good | Acceptable | Poor | Failing |\n";
    out << "|-------|--------|---------|------|------------|------|--------|\n";

    int total_perfect = 0, total_good = 0, total_acceptable = 0, total_poor = 0, total_failing = 0;

    for (const auto& model : reports) {
        int perfect = 0, good = 0, acceptable = 0, poor = 0, failing = 0;
        for (const auto& jr : model.joints) {
            switch (jr.grade) {
            case JointFaceReport::PERFECT: perfect++; break;
            case JointFaceReport::GOOD: good++; break;
            case JointFaceReport::ACCEPTABLE: acceptable++; break;
            case JointFaceReport::POOR: poor++; break;
            case JointFaceReport::FAILING: failing++; break;
            }
        }
        total_perfect += perfect;
        total_good += good;
        total_acceptable += acceptable;
        total_poor += poor;
        total_failing += failing;

        out << "| " << model.model_name
            << " | " << model.joint_count
            << " | " << perfect
            << " | " << good
            << " | " << acceptable
            << " | " << poor
            << " | " << failing
            << " |\n";
    }

    out << "| **TOTAL** | - "
        << "| " << total_perfect
        << " | " << total_good
        << " | " << total_acceptable
        << " | " << total_poor
        << " | " << total_failing
        << " |\n\n";

    int total_joints = total_perfect + total_good + total_acceptable + total_poor + total_failing;
    if (total_joints > 0) {
        float pass_rate = 100.0f * (total_perfect + total_good + total_acceptable) / total_joints;
        out << "**Overall pass rate:** " << std::fixed << std::setprecision(1) << pass_rate << "%\n\n";
    }

    // Detailed per-model sections
    out << "## Detailed Results\n\n";
    out << "Grading criteria:\n";
    out << "- **PERFECT**: center distance < 0.001, normal dot < -0.999, area ratio > 0.999, vertex match < 0.001\n";
    out << "- **GOOD**: center distance < 0.01, normal dot < -0.99, area ratio > 0.95, vertex match < 0.02\n";
    out << "- **ACCEPTABLE**: center distance < 0.05, normal dot < -0.95, area ratio > 0.8\n";
    out << "- **POOR**: center distance < 0.2, normal dot < -0.8\n";
    out << "- **FAILING**: everything else\n\n";

    for (const auto& model : reports) {
        out << "### " << model.model_name << "\n\n";
        out << "File: `" << model.model_path << "`  \n";
        out << "Joints: " << model.joint_count << "\n\n";

        if (model.joints.empty()) {
            out << "_No joints to analyze (single-node body)._\n\n";
            continue;
        }

        out << "| Subdiv | Joint | Grade | Dist | Normal Dot | Area Ratio | Vtx Match | Max Vtx Dist |\n";
        out << "|--------|-------|-------|------|------------|------------|-----------|-------------|\n";

        for (const auto& jr : model.joints) {
            out << "| " << jr.subdivision_level
                << " | " << jr.parent_name << " -> " << jr.child_name
                << " | " << GradeEmoji(jr.grade) << " " << GradeString(jr.grade)
                << " | " << std::fixed << std::setprecision(6) << jr.face_center_distance
                << " | " << std::setprecision(6) << jr.normal_dot_product
                << " | " << std::setprecision(4) << jr.area_ratio
                << " | " << (jr.vertex_count_match ? "YES" : "NO")
                         << " (" << jr.parent_face_vertex_count << "/" << jr.child_face_vertex_count << ")"
                << " | " << std::setprecision(6) << jr.max_vertex_distance
                << " |\n";
        }
        out << "\n";

        // Highlight issues
        bool has_issues = false;
        for (const auto& jr : model.joints) {
            if (jr.grade == JointFaceReport::POOR || jr.grade == JointFaceReport::FAILING) {
                if (!has_issues) {
                    out << "**Issues found:**\n\n";
                    has_issues = true;
                }
                out << "- " << GradeEmoji(jr.grade) << " **" << jr.parent_name << " -> " << jr.child_name
                    << "** (subdiv " << jr.subdivision_level << "): ";
                if (!jr.vertex_count_match) {
                    out << "vertex count mismatch (" << jr.parent_face_vertex_count
                        << " vs " << jr.child_face_vertex_count << "), ";
                }
                if (jr.normal_dot_product > -0.95f) {
                    out << "normals not opposing (dot=" << std::setprecision(3) << jr.normal_dot_product << "), ";
                }
                if (jr.face_center_distance > 0.05f) {
                    out << "faces not touching (dist=" << std::setprecision(4) << jr.face_center_distance << "), ";
                }
                if (jr.area_ratio < 0.8f) {
                    out << "area mismatch (ratio=" << std::setprecision(3) << jr.area_ratio << "), ";
                }
                out << "\n";
            }
        }
        if (!has_issues) {
            out << "_All joints passing._\n";
        }
        out << "\n";
    }

    // Recommendations section
    out << "## Analysis Notes\n\n";
    out << "This analysis tests the core goal of the connection system: that at each joint,\n";
    out << "the parent body's connection face and the child body's connection face are identical\n";
    out << "(same position in world space, opposing normals, matching geometry).\n\n";
    out << "Key areas to investigate if joints are POOR/FAILING:\n\n";
    out << "1. **Normal dot != -1.0**: The face-center snapping or orientation computation is off.\n";
    out << "2. **Center distance != 0**: The positioning transform doesn't align face centers.\n";
    out << "3. **Vertex count mismatch**: Subdivision derivation isn't propagating segment counts correctly.\n";
    out << "4. **Area mismatch**: Size-matching deformation phase isn't equalizing face sizes.\n";
    out << "5. **High vertex distance**: Even with matching counts, the rotational alignment is off.\n\n";

    out.close();
    return true;
}

} // namespace BodyRenderer
