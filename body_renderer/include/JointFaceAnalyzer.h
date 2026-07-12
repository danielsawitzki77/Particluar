#pragma once

#include "BodyTypes.h"
#include "FaceGenerator.h"
#include <string>
#include <vector>

namespace BodyRenderer {

// Per-joint analysis results for one pair of connected bodies at one subdivision level.
struct JointFaceReport {
    std::string parent_name;
    std::string child_name;
    int subdivision_level;

    // Parent-side connection face
    int parentFaceIndex;
    int parent_face_vertex_count;
    float parent_face_area;
    Vec3 parent_face_center_world;
    Vec3 parent_face_normal_world;

    // Child-side connection face
    int childFaceIndex;
    int child_face_vertex_count;
    float child_face_area;
    Vec3 child_face_center_world;
    Vec3 child_face_normal_world;

    // Quality metrics
    float face_center_distance;   // distance between face centers (should be ~0)
    float normal_dot_product;     // dot of normals (should be -1.0 for flush)
    float area_difference;        // absolute difference in face areas
    float area_ratio;             // min/max area ratio (1.0 = identical)
    bool vertex_count_match;      // do both faces have the same vertex count?
    float vertex_sum_distance;    // sum of min-distance for each parent vertex to closest child vertex
    float max_vertex_distance;    // worst single vertex mismatch

    // Overall quality grade
    enum Grade { PERFECT, GOOD, ACCEPTABLE, POOR, FAILING };
    Grade grade;

    JointFaceReport()
        : subdivision_level(1)
        , parentFaceIndex(-1), parent_face_vertex_count(0), parent_face_area(0.0f)
        , childFaceIndex(-1), child_face_vertex_count(0), child_face_area(0.0f)
        , face_center_distance(0.0f), normal_dot_product(0.0f)
        , area_difference(0.0f), area_ratio(0.0f)
        , vertex_count_match(false)
        , vertex_sum_distance(0.0f), max_vertex_distance(0.0f)
        , grade(FAILING)
    {}
};

// Per-model summary across all joints and subdivision levels.
struct ModelAnalysisReport {
    std::string model_name;
    std::string model_path;
    int joint_count;
    std::vector<JointFaceReport> joints;
};

// Runs the full joint face matching analysis across all models.
class JointFaceAnalyzer {
public:
    // Analyze all JSON models in the given directory.
    // subdivision_levels: which resolutions to test (e.g., {1, 2, 4, 8})
    std::vector<ModelAnalysisReport> AnalyzeDirectory(
        const std::string& body_dir,
        const std::vector<int>& subdivision_levels) const;

    // Analyze a single body at a given subdivision level.
    std::vector<JointFaceReport> AnalyzeBody(
        const Body& body,
        int subdivision_level) const;

    // Write the full analysis to a Markdown report file.
    bool WriteReport(const std::vector<ModelAnalysisReport>& reports,
                     const std::string& output_path) const;

private:
    // Recursively analyze joint connections in a body tree.
    void AnalyzeNodeRecursive(
        const BodyNode* node,
        const BodyNode* parent,
        const Mat4& parent_world,
        int subdivision_level,
        std::vector<JointFaceReport>& out_reports) const;

    // Compute the area of a face polygon.
    float ComputeFaceArea(const Face& face) const;

    // Compute best vertex-to-vertex matching between two faces.
    void ComputeVertexMatching(
        const std::vector<Vec3>& parent_verts_world,
        const std::vector<Vec3>& child_verts_world,
        float& out_sum_distance,
        float& out_max_distance) const;

    // Assign quality grade based on metrics.
    JointFaceReport::Grade ComputeGrade(const JointFaceReport& report) const;
};

} // namespace BodyRenderer
