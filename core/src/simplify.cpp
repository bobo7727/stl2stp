#include "internal.hpp"

#include <map>
#include <queue>
#include <set>

namespace stl2stp {
namespace {

struct Quadric {
    std::array<double, 10> values{};

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

Mesh simplify_mesh(const Mesh& source, std::size_t target, Progress& progress) {
    std::vector<Point> points = source.points;
    std::vector<Face> faces;
    faces.reserve(source.triangles.size());
    for (const auto& triangle : source.triangles) {
        faces.push_back({triangle, true});
    }
    std::vector<std::set<std::size_t>> adjacency(points.size());
    std::vector<Quadric> quadrics(points.size());
    std::vector<uint64_t> versions(points.size(), 0);
    for (std::size_t index = 0; index < faces.size(); ++index) {
        const auto& triangle = faces[index].vertices;
        auto normal = cross(subtract(points[triangle[1]], points[triangle[0]]),
                            subtract(points[triangle[2]], points[triangle[0]]));
        const double length = std::sqrt(dot(normal, normal));
        normal = {normal.coordinate_x / length, normal.coordinate_y / length, normal.coordinate_z / length};
        const auto quadric = plane_quadric(normal, -dot(normal, points[triangle[0]]));
        for (const auto vertex : triangle) {
            adjacency[vertex].insert(index);
            quadrics[vertex] += quadric;
        }
        if (index % 256 == 0) {
            progress.report(0.1 * double(index + 1) / double(faces.size()));
        }
    }

    auto solve = [&](std::size_t first, std::size_t second, Point& best, double& error) {
        Quadric combined = quadrics[first];
        combined += quadrics[second];
        const auto& values = combined.values;
        const double determinant = values[0] * (values[4] * values[7] - values[5] * values[5]) -
            values[1] * (values[1] * values[7] - values[5] * values[2]) +
            values[2] * (values[1] * values[5] - values[4] * values[2]);
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
        if (std::abs(determinant) > 1e-12) {
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
            const double limit = 3 * std::max({std::abs(span.coordinate_x), std::abs(span.coordinate_y), std::abs(span.coordinate_z)}) + 1;
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

    auto collapse_ok = [&](std::size_t first, std::size_t second, const Point& position) {
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
            if (dot(original, updated) <= 0) {
                return false;
            }
        }
        return true;
    };

    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> heap;
    auto push_edges = [&](std::size_t vertex) {
        std::set<std::size_t> neighbors;
        for (const auto face_index : adjacency[vertex]) {
            if (faces[face_index].alive) {
                for (const auto neighbor : faces[face_index].vertices) {
                    if (neighbor != vertex) { neighbors.insert(neighbor); }
                }
            }
        }
        for (const auto neighbor : neighbors) {
            const auto first = std::min(vertex, neighbor), second = std::max(vertex, neighbor);
            Point position;
            double error;
            solve(first, second, position, error);
            heap.push({error, first, second, versions[first], versions[second]});
        }
    };
    for (std::size_t vertex = 0; vertex < points.size(); ++vertex) {
        push_edges(vertex);
        if (vertex % 256 == 0) {
            progress.report(0.1 + 0.08 * double(vertex + 1) / double(points.size()));
        }
    }

    std::size_t remaining = faces.size();
    std::size_t iterations = 0;
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
        bool edge_exists = false;
        std::size_t shared_faces = 0;
        for (const auto face_index : adjacency[first]) {
            if (faces[face_index].alive && contains(face_index, second)) {
                edge_exists = true;
                ++shared_faces;
            }
        }
        if (!edge_exists || shared_faces >= remaining) {
            continue;
        }
        Point position;
        double error;
        solve(first, second, position, error);
        maximum_error = std::max(maximum_error, error);
        if (!collapse_ok(first, second, position)) {
            int& rejected = rejections[{first, second}];
            if (rejected++ < 16) {
                heap.push({maximum_error + 1 + rejected, first, second, versions[first], versions[second]});
            }
            continue;
        }
        points[first] = position;
        quadrics[first] += quadrics[second];
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
