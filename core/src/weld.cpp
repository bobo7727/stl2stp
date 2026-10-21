// Near-coincident vertex weld (core/weld).
//
// STL stores every facet corner separately; the loader joins corners with identical float coordinates.
// Exporters that recompute shared points (e.g. a UV sphere pole as r*sin(pi), which is 1e-15 rather than 0)
// produce distinct points a few float ULPs apart, so the topology has microscopic holes. This weld joins
// points closer than a tolerance relative to the model size:
//   tolerance = max(1e-6 x bbox diagonal, 2 x FLT_EPSILON x max|coordinate|)
// 1e-6 of the diagonal is ~16 float32 ULPs for an origin-centred part (1 um on a 1 m part, 0.1 um on a
// 100 mm part), far below any manufacturable feature, and matches the 1e-6 sewing tolerance and the linear
// tolerance of the coplanar merge. The second term is a float32 precision floor for parts far from the
// origin, where coordinate noise is relative to the coordinate magnitude, not to the part size.
//
// Guards (thin features and slivers must survive):
//  1. Only open-boundary vertices are candidates: a vertex is welded only if, under exact joining, it lies
//     on an edge used by a single triangle. A microscopic hole always shows up as such edges; a vertex whose
//     fan is already closed has nothing to join, however close another surface passes (thin walls, slabs,
//     finely subdivided narrow faces keep every vertex).
//  2. A point is never joined to a cluster that already contains a point of one of its own triangles, so
//     welding cannot collapse a triangle (genuinely tiny edges below the tolerance are kept). Clusters are
// greedy and non-transitive: a point joins the nearest representative within the tolerance.
// Spatial hash on a grid with cell = tolerance; each point inspects its 27 neighbour cells: O(n) expected.
#include "internal.hpp"

#include <cfloat>
#include <unordered_map>

namespace stl2stp {

double weld_tolerance(const std::vector<Point>& points, double requested) {
    Point minimum{}, maximum{};
    double magnitude = 0;
    bool first = true;
    for (const auto& p : points) {
        if (first) { minimum = maximum = p; first = false; }
        minimum = {std::min(minimum.coordinate_x, p.coordinate_x), std::min(minimum.coordinate_y, p.coordinate_y),
                   std::min(minimum.coordinate_z, p.coordinate_z)};
        maximum = {std::max(maximum.coordinate_x, p.coordinate_x), std::max(maximum.coordinate_y, p.coordinate_y),
                   std::max(maximum.coordinate_z, p.coordinate_z)};
        magnitude = std::max({magnitude, std::abs(p.coordinate_x), std::abs(p.coordinate_y), std::abs(p.coordinate_z)});
    }
    const auto extent = subtract(maximum, minimum);
    const double diagonal = std::sqrt(dot(extent, extent));
    const double floor = 2.0 * double(FLT_EPSILON) * magnitude;  // >= 2 float32 ULPs of the largest coordinate
    double tolerance = requested > 0 ? requested : std::max(1e-6 * diagonal, floor);
    // Keep grid indices addressable for absurdly small overrides.
    tolerance = std::max(tolerance, 1e-18 * std::max(diagonal, magnitude));
    return tolerance;
}

namespace {

struct Cell {
    int64_t x, y, z;
    bool operator==(const Cell& other) const { return x == other.x && y == other.y && z == other.z; }
};

struct CellHash {
    std::size_t operator()(const Cell& c) const {
        uint64_t h = uint64_t(c.x) * 0x9E3779B97F4A7C15ULL;
        h ^= uint64_t(c.y) * 0xC2B2AE3D27D4EB4FULL + (h << 6) + (h >> 2);
        h ^= uint64_t(c.z) * 0x165667B19E3779F9ULL + (h << 6) + (h >> 2);
        return std::size_t(h);
    }
};

}

WeldResult weld_points(const std::vector<Point>& points, const std::vector<const std::vector<Triangle>*>& topology,
                       double tolerance, Progress* progress) {
    WeldResult result;
    result.tolerance = tolerance;
    const std::size_t count = points.size();
    result.map.resize(count);
    for (std::size_t i = 0; i < count; ++i) { result.map[i] = i; }
    if (count < 2 || count > 0xffffffffULL || !(tolerance > 0) || !std::isfinite(tolerance)) { return result; }

    // Vertex -> neighbour vertices (sharing a triangle), CSR.
    std::vector<std::size_t> offset(count + 1, 0);
    for (const auto* triangles : topology) {
        for (const auto& t : *triangles) { for (const auto v : t) { if (v < count) { offset[v + 1] += 2; } } }
    }
    for (std::size_t i = 0; i < count; ++i) { offset[i + 1] += offset[i]; }
    std::vector<std::size_t> neighbours(offset[count]);
    std::vector<std::size_t> fill(offset.begin(), offset.end() - 1);
    for (const auto* triangles : topology) {
        for (const auto& t : *triangles) {
            for (std::size_t c = 0; c < 3; ++c) {
                const auto v = t[c];
                if (v >= count) { continue; }
                neighbours[fill[v]++] = t[(c + 1) % 3];
                neighbours[fill[v]++] = t[(c + 2) % 3];
            }
        }
    }
    // Guard 1: open-boundary vertices only (edge used by exactly one triangle under exact joining).
    std::vector<char> open(count, 0);
    {
        std::unordered_map<uint64_t, uint32_t> uses;
        std::size_t total = 0;
        for (const auto* triangles : topology) { total += triangles->size(); }
        uses.reserve(total * 2);
        auto key = [](std::size_t a, std::size_t b) { return (uint64_t(std::min(a, b)) << 32) | uint64_t(std::max(a, b)); };
        for (const auto* triangles : topology) {
            for (const auto& t : *triangles) {
                for (std::size_t c = 0; c < 3; ++c) {
                    if (t[c] != t[(c + 1) % 3]) { ++uses[key(t[c], t[(c + 1) % 3])]; }
                }
            }
        }
        for (const auto& [edge, n] : uses) {
            if (n == 1) { open[std::size_t(edge >> 32)] = 1; open[std::size_t(edge & 0xffffffffULL)] = 1; }
        }
    }
    auto adjacent = [&](std::size_t a, std::size_t b) {
        for (std::size_t k = offset[a]; k < offset[a + 1]; ++k) { if (neighbours[k] == b) { return true; } }
        return false;
    };

    const Point origin = points.front();
    auto cell_of = [&](const Point& p) {
        return Cell{int64_t(std::floor((p.coordinate_x - origin.coordinate_x) / tolerance)),
                    int64_t(std::floor((p.coordinate_y - origin.coordinate_y) / tolerance)),
                    int64_t(std::floor((p.coordinate_z - origin.coordinate_z) / tolerance))};
    };
    std::unordered_map<Cell, std::vector<std::size_t>, CellHash> grid;  // cell -> representatives
    grid.reserve(count);
    std::vector<std::vector<std::size_t>> members;  // only for representatives that gained members
    std::unordered_map<std::size_t, std::size_t> member_slot;
    const double limit = tolerance * tolerance;
    for (std::size_t i = 0; i < count; ++i) {
        if (!open[i]) { continue; }  // closed fan: never welded, never a representative
        const Cell cell = cell_of(points[i]);
        std::size_t best = count;
        double best_distance = limit;
        for (int64_t dx = -1; dx <= 1; ++dx) {
            for (int64_t dy = -1; dy <= 1; ++dy) {
                for (int64_t dz = -1; dz <= 1; ++dz) {
                    const auto found = grid.find(Cell{cell.x + dx, cell.y + dy, cell.z + dz});
                    if (found == grid.end()) { continue; }
                    for (const auto r : found->second) {
                        const Point d = subtract(points[i], points[r]);
                        const double distance = dot(d, d);
                        if (distance > best_distance) { continue; }
                        // Guard: never join two vertices of one triangle (would collapse it).
                        bool conflict = adjacent(i, r);
                        const auto slot = member_slot.find(r);
                        if (!conflict && slot != member_slot.end()) {
                            for (const auto m : members[slot->second]) { if (adjacent(i, m)) { conflict = true; break; } }
                        }
                        if (conflict) { continue; }
                        best = r;
                        best_distance = distance;
                    }
                }
            }
        }
        if (best == count) {
            grid[cell].push_back(i);
        } else {
            result.map[i] = best;
            ++result.merged;
            auto slot = member_slot.find(best);
            if (slot == member_slot.end()) {
                slot = member_slot.emplace(best, members.size()).first;
                members.emplace_back();
            }
            members[slot->second].push_back(i);
        }
        if (progress && i % 4096 == 0) { progress->check(); }
    }
    return result;
}

}

stl2stp_status stl2stp_weld_vertices(stl2stp_handle* handle, double tolerance, uint64_t* merged) {
    return stl2stp::protect(handle, [&] {
        stl2stp::require_mesh(handle->mesh);
        if (!std::isfinite(tolerance)) { throw stl2stp::Error(STL2STP_ERROR_ARGUMENT, "Weld tolerance must be finite"); }
        auto& mesh = handle->mesh;
        const double used = stl2stp::weld_tolerance(mesh.points, tolerance);
        std::vector<const std::vector<stl2stp::Triangle>*> topology{&mesh.triangles};
        if (!mesh.original_triangles.empty()) { topology.push_back(&mesh.original_triangles); }
        const auto weld = stl2stp::weld_points(mesh.points, topology, used);
        if (merged) { *merged = weld.merged; }
        if (!weld.merged) { return STL2STP_OK; }
        std::vector<std::size_t> renumber(mesh.points.size(), std::numeric_limits<std::size_t>::max());
        std::vector<stl2stp::Point> points;
        for (std::size_t i = 0; i < mesh.points.size(); ++i) {
            if (weld.map[i] == i) { renumber[i] = points.size(); points.push_back(mesh.points[i]); }
        }
        auto remap = [&](std::vector<stl2stp::Triangle>& triangles, bool drop) {
            std::vector<stl2stp::Triangle> kept;
            kept.reserve(triangles.size());
            for (auto t : triangles) {
                for (auto& v : t) { v = renumber[weld.map[v]]; }
                if (drop && (t[0] == t[1] || t[1] == t[2] || t[0] == t[2])) { continue; }  // guard makes this unreachable
                kept.push_back(t);
            }
            triangles.swap(kept);
        };
        remap(mesh.triangles, true);
        remap(mesh.original_triangles, false);
        mesh.points.swap(points);
        return STL2STP_OK;
    });
}
