#include "internal.hpp"

#include <map>
#include <unordered_map>
#include <queue>
#include <set>

namespace stl2stp {
namespace {

struct Quadric {
    std::array<double, 10> values{};

    Quadric& scale(double factor) {
        for (auto& value : values) { value *= factor; }
        return *this;
    }

    Quadric& operator+=(const Quadric& other) {
        for (std::size_t index = 0; index < values.size(); ++index) {
            values[index] += other.values[index];
        }
        return *this;
    }

    double evaluate(const Point& point) const {
        return values[0] * point.coordinate_x * point.coordinate_x + 2 * values[1] * point.coordinate_x * point.coordinate_y +
               2 * values[2] * point.coordinate_x * point.coordinate_z + 2 * values[3] * point.coordinate_x +
               values[4] * point.coordinate_y * point.coordinate_y + 2 * values[5] * point.coordinate_y * point.coordinate_z +
               2 * values[6] * point.coordinate_y + values[7] * point.coordinate_z * point.coordinate_z +
               2 * values[8] * point.coordinate_z + values[9];
    }
};

Quadric plane_quadric(const Point& normal, double distance) {
    return {{normal.coordinate_x * normal.coordinate_x, normal.coordinate_x * normal.coordinate_y,
             normal.coordinate_x * normal.coordinate_z, normal.coordinate_x * distance,
             normal.coordinate_y * normal.coordinate_y, normal.coordinate_y * normal.coordinate_z,
             normal.coordinate_y * distance, normal.coordinate_z * normal.coordinate_z,
             normal.coordinate_z * distance, distance * distance}};
}

struct Face {
    Triangle vertices;
    bool alive = true;
};

struct Candidate {
    double error;
    std::size_t first;
    std::size_t second;
    uint64_t first_version;
    uint64_t second_version;

    bool operator>(const Candidate& other) const {
        if (error != other.error) { return error > other.error; }
        if (first != other.first) { return first > other.first; }
        return second > other.second;
    }
};

}

// Garland-Heckbert QEM edge collapse with the safeguards from the 2026-10-20 review:
// - face quadrics are area weighted (GH97), so the error is independent of tessellation density;
// - open-boundary (and non-manifold) edges get heavily weighted perpendicular constraint planes,
//   so open meshes keep their outline instead of being "eaten" from the border;
// - link condition, valence and boundary checks keep the mesh manifold (no duplicate faces);
// - normal-deviation and triangle-quality checks (phase 1) avoid needles and fold-overs; a second
//   phase relaxes only these geometric checks if the target was not reached;
// - every threshold is relative to the model size, so results do not depend on units.
Mesh simplify_mesh(const Mesh& source, std::size_t target, Progress& progress) {
    std::vector<Point> points = source.points;
    std::vector<Face> faces;
    faces.reserve(source.triangles.size());
    for (const auto& triangle : source.triangles) {
        faces.push_back({triangle, true});
    }
    Point minimum = points.empty() ? Point{} : points.front(), maximum = minimum;
    for (const auto& point : points) {
        minimum = {std::min(minimum.coordinate_x, point.coordinate_x), std::min(minimum.coordinate_y, point.coordinate_y),
                   std::min(minimum.coordinate_z, point.coordinate_z)};
        maximum = {std::max(maximum.coordinate_x, point.coordinate_x), std::max(maximum.coordinate_y, point.coordinate_y),
                   std::max(maximum.coordinate_z, point.coordinate_z)};
    }
    const Point extent = subtract(maximum, minimum);
    const double diagonal = std::sqrt(dot(extent, extent));
    const double length_epsilon = 1e-9 * diagonal;
    const double error_epsilon = 1e-12 * diagonal * diagonal * diagonal * diagonal;
    constexpr double boundary_weight = 1e3;

    std::vector<std::set<std::size_t>> adjacency(points.size());
    std::vector<Quadric> quadrics(points.size());
    std::vector<uint64_t> versions(points.size(), 0);
    std::vector<char> boundary(points.size(), 0);
    auto unit_normal = [&](const Triangle& triangle, double& area) {
        auto normal = cross(subtract(points[triangle[1]], points[triangle[0]]),
                            subtract(points[triangle[2]], points[triangle[0]]));
        const double length = std::sqrt(dot(normal, normal));
        area = 0.5 * length;
        return length > 0 ? Point{normal.coordinate_x / length, normal.coordinate_y / length, normal.coordinate_z / length}
                          : Point{};
    };
    auto edge_key = [](std::size_t first, std::size_t second) {
        return std::make_pair(std::min(first, second), std::max(first, second));
    };
    std::map<std::pair<std::size_t, std::size_t>, std::vector<std::size_t>> edge_faces;
    for (std::size_t index = 0; index < faces.size(); ++index) {
        const auto& triangle = faces[index].vertices;
        double area = 0;
        const auto normal = unit_normal(triangle, area);
        auto quadric = plane_quadric(normal, -dot(normal, points[triangle[0]]));
        quadric.scale(area);
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const auto vertex = triangle[corner];
            adjacency[vertex].insert(index);
            quadrics[vertex] += quadric;
            edge_faces[edge_key(vertex, triangle[(corner + 1) % 3])].push_back(index);
        }
        if (index % 256 == 0) {
            progress.report(0.08 * double(index + 1) / double(faces.size()));
        }
    }
    for (const auto& entry : edge_faces) {
        if (entry.second.size() == 2) { continue; }
        const auto first = entry.first.first, second = entry.first.second;
        boundary[first] = boundary[second] = 1;
        const Point direction = subtract(points[second], points[first]);
        const double length_squared = dot(direction, direction);
        for (const auto face_index : entry.second) {
            double area = 0;
            const auto normal = unit_normal(faces[face_index].vertices, area);
            auto perpendicular = cross(direction, normal);
            const double length = std::sqrt(dot(perpendicular, perpendicular));
            if (!(length > 0)) { continue; }
            perpendicular = {perpendicular.coordinate_x / length, perpendicular.coordinate_y / length,
                             perpendicular.coordinate_z / length};
            auto constraint = plane_quadric(perpendicular, -dot(perpendicular, points[first]));
            constraint.scale(boundary_weight * length_squared);
            quadrics[first] += constraint;
            quadrics[second] += constraint;
        }
    }
    edge_faces.clear();
    progress.report(0.1);

    auto solve = [&](std::size_t first, std::size_t second, Point& best, double& error) {
        Quadric combined = quadrics[first];
        combined += quadrics[second];
        const auto& values = combined.values;
        const double determinant = values[0] * (values[4] * values[7] - values[5] * values[5]) -
            values[1] * (values[1] * values[7] - values[5] * values[2]) +
            values[2] * (values[1] * values[5] - values[4] * values[2]);
        const double trace = (values[0] + values[4] + values[7]) / 3.0;
        const Point midpoint{(points[first].coordinate_x + points[second].coordinate_x) * 0.5,
                             (points[first].coordinate_y + points[second].coordinate_y) * 0.5,
                             (points[first].coordinate_z + points[second].coordinate_z) * 0.5};
        best = midpoint;
        error = combined.evaluate(midpoint);
        if (!std::isfinite(error)) {
            throw Error(STL2STP_ERROR_FORMAT, "Geometry scale exceeds QEM numerical range");
        }
        auto consider = [&](const Point& position) {
            const double cost = combined.evaluate(position);
            if (std::isfinite(cost) && cost < error) {
                best = position;
                error = cost;
            }
        };
        // Relative conditioning test (scale independent) instead of an absolute determinant threshold.
        if (trace > 0 && std::abs(determinant) > 1e-9 * trace * trace * trace) {
            const double rhs_x = -values[3], rhs_y = -values[6], rhs_z = -values[8];
            const Point optimal{
                (rhs_x * (values[4] * values[7] - values[5] * values[5]) -
                 values[1] * (rhs_y * values[7] - values[5] * rhs_z) +
                 values[2] * (rhs_y * values[5] - values[4] * rhs_z)) / determinant,
                (values[0] * (rhs_y * values[7] - values[5] * rhs_z) -
                 rhs_x * (values[1] * values[7] - values[2] * values[5]) +
                 values[2] * (values[1] * rhs_z - rhs_y * values[2])) / determinant,
                (values[0] * (values[4] * rhs_z - rhs_y * values[5]) -
                 values[1] * (values[1] * rhs_z - rhs_y * values[2]) +
                 rhs_x * (values[1] * values[5] - values[4] * values[2])) / determinant};
            const Point span = subtract(points[first], points[second]);
            const double limit = 3 * std::max({std::abs(span.coordinate_x), std::abs(span.coordinate_y),
                                               std::abs(span.coordinate_z)}) + length_epsilon;
            if (std::abs(optimal.coordinate_x - midpoint.coordinate_x) < limit &&
                std::abs(optimal.coordinate_y - midpoint.coordinate_y) < limit &&
                std::abs(optimal.coordinate_z - midpoint.coordinate_z) < limit) {
                consider(optimal);
            }
        }
        consider(points[first]);
        consider(points[second]);
        error = std::max(error, 0.0);
    };

    auto contains = [&](std::size_t face_index, std::size_t vertex) {
        const auto& vertices = faces[face_index].vertices;
        return std::find(vertices.begin(), vertices.end(), vertex) != vertices.end();
    };

    auto neighbors = [&](std::size_t vertex) {
        std::set<std::size_t> result;
        for (const auto face_index : adjacency[vertex]) {
            if (!faces[face_index].alive) { continue; }
            for (const auto other : faces[face_index].vertices) {
                if (other != vertex) { result.insert(other); }
            }
        }
        return result;
    };

    auto quality = [](const Point& first, const Point& second, const Point& third) {
        const Point a = subtract(second, first), b = subtract(third, first), c = subtract(third, second);
        const Point normal = cross(a, b);
        const double lengths = dot(a, a) + dot(b, b) + dot(c, c);
        return lengths > 0 ? 2.0 * std::sqrt(3.0) * std::sqrt(dot(normal, normal)) / lengths : 0.0;
    };

    // Topological safety: manifold edge, link condition, valence and boundary rules.
    auto topology_ok = [&](std::size_t first, std::size_t second) {
        std::vector<std::size_t> opposite;
        for (const auto face_index : adjacency[first]) {
            if (!faces[face_index].alive || !contains(face_index, second)) { continue; }
            for (const auto vertex : faces[face_index].vertices) {
                if (vertex != first && vertex != second) { opposite.push_back(vertex); }
            }
        }
        if (opposite.empty() || opposite.size() > 2) { return false; }
        const bool boundary_edge = opposite.size() == 1;
        if (boundary[first] && boundary[second] && !boundary_edge) { return false; }
        const auto first_ring = neighbors(first), second_ring = neighbors(second);
        std::size_t common = 0;
        for (const auto vertex : first_ring) {
            if (second_ring.count(vertex)) {
                if (std::find(opposite.begin(), opposite.end(), vertex) == opposite.end()) { return false; }
                ++common;
            }
        }
        if (common != opposite.size()) { return false; }
        for (const auto vertex : opposite) {
            const auto valence = neighbors(vertex).size();
            if (valence <= (boundary[vertex] ? 2u : 3u)) { return false; }
        }
        if (!boundary_edge && first_ring.size() + second_ring.size() <= 6) { return false; }
        return true;
    };

    // Geometric safety: no fold-over, bounded normal deviation and triangle quality (phase 1 only).
    auto geometry_ok = [&](std::size_t first, std::size_t second, const Point& position, bool strict) {
        std::set<std::size_t> affected = adjacency[first];
        affected.insert(adjacency[second].begin(), adjacency[second].end());
        for (const auto face_index : affected) {
            if (!faces[face_index].alive || (contains(face_index, first) && contains(face_index, second))) {
                continue;
            }
            const auto& triangle = faces[face_index].vertices;
            const Point original = cross(subtract(points[triangle[1]], points[triangle[0]]),
                                         subtract(points[triangle[2]], points[triangle[0]]));
            std::array<Point, 3> moved{};
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto vertex = triangle[corner];
                moved[corner] = vertex == first || vertex == second ? position : points[vertex];
            }
            const Point updated = cross(subtract(moved[1], moved[0]), subtract(moved[2], moved[0]));
            const double original_length = std::sqrt(dot(original, original));
            const double updated_length = std::sqrt(dot(updated, updated));
            if (!(updated_length > length_epsilon * length_epsilon)) { return false; }
            const double cosine = dot(original, updated) / (original_length * updated_length);
            if (!(cosine > (strict ? 0.25 : 0.0))) { return false; }
            if (strict) {
                const double before = quality(points[triangle[0]], points[triangle[1]], points[triangle[2]]);
                const double after = quality(moved[0], moved[1], moved[2]);
                if (after < 0.05 && after < before) { return false; }
            }
        }
        return true;
    };

    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> heap;
    auto push_edges = [&](std::size_t vertex) {
        for (const auto neighbor : neighbors(vertex)) {
            const auto first = std::min(vertex, neighbor), second = std::max(vertex, neighbor);
            Point position;
            double error;
            solve(first, second, position, error);
            heap.push({error, first, second, versions[first], versions[second]});
        }
    };

    std::size_t remaining = faces.size();
    std::size_t iterations = 0;
    for (int phase = 0; phase < 2 && remaining > target; ++phase) {
        const bool strict = phase == 0;
        heap = {};
        for (std::size_t vertex = 0; vertex < points.size(); ++vertex) {
            push_edges(vertex);
            if (vertex % 256 == 0) {
                progress.report(0.1 + 0.08 * double(vertex + 1) / double(points.size()) * (strict ? 1.0 : 0.0));
            }
        }
        std::map<std::pair<std::size_t, std::size_t>, int> rejections;
        double maximum_error = 0;
        while (remaining > target && !heap.empty()) {
            if (++iterations % 256 == 0) {
                progress.report(0.18 + 0.76 * double(faces.size() - remaining) / double(faces.size() - target));
            }
            const auto candidate = heap.top();
            heap.pop();
            const auto first = candidate.first, second = candidate.second;
            if (candidate.first_version != versions[first] || candidate.second_version != versions[second]) {
                continue;
            }
            std::size_t shared_faces = 0;
            for (const auto face_index : adjacency[first]) {
                if (faces[face_index].alive && contains(face_index, second)) { ++shared_faces; }
            }
            if (shared_faces == 0 || shared_faces >= remaining || !topology_ok(first, second)) {
                continue;
            }
            Point position;
            double error;
            solve(first, second, position, error);
            maximum_error = std::max(maximum_error, error);
            if (!geometry_ok(first, second, position, strict)) {
                int& rejected = rejections[{first, second}];
                if (rejected++ < 16) {
                    heap.push({error + (maximum_error + error_epsilon) * rejected, first, second,
                               versions[first], versions[second]});
                }
                continue;
            }
            points[first] = position;
            quadrics[first] += quadrics[second];
            boundary[first] = boundary[first] || boundary[second];
            ++versions[first];
            ++versions[second];
            for (const auto face_index : adjacency[second]) {
                auto& face = faces[face_index];
                if (!face.alive) { continue; }
                const bool shared = contains(face_index, first);
                for (auto& vertex : face.vertices) {
                    if (vertex == second) { vertex = first; }
                }
                if (shared) {
                    face.alive = false;
                    --remaining;
                } else {
                    adjacency[first].insert(face_index);
                }
            }
            adjacency[second].clear();
            push_edges(first);
        }
    }

    progress.report(0.95);
    Mesh result;
    std::vector<std::size_t> renumber(points.size(), std::numeric_limits<std::size_t>::max());
    for (std::size_t face_index = 0; face_index < faces.size(); ++face_index) {
        const auto& face = faces[face_index];
        if (face.alive) {
            Triangle triangle{};
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto vertex = face.vertices[corner];
                if (renumber[vertex] == std::numeric_limits<std::size_t>::max()) {
                    renumber[vertex] = result.points.size();
                    result.points.push_back(points[vertex]);
                }
                triangle[corner] = renumber[vertex];
            }
            result.triangles.push_back(triangle);
        }
        if (face_index % 256 == 0) {
            progress.report(0.95 + 0.04 * double(face_index + 1) / double(faces.size()));
        }
    }
    return result;
}

}
