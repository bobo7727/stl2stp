// Regression tests for the 2026-10-20 code review findings (docs/core-validation.md).
// Every fixture is generated here as float32 binary STL so the cases match real STL input.
#include <stl2stp/stl2stp.h>

#include <BRepCheck_Analyzer.hxx>
#include <BRepCheck_Result.hxx>
#include <BRepCheck_ListOfStatus.hxx>
#include <BRepCheck_Status.hxx>
#include <BRep_Tool.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <ShapeProcess.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Vec = std::array<double, 3>;
using Tri = std::array<Vec, 3>;
using Handle = std::unique_ptr<stl2stp_handle, decltype(&stl2stp_destroy)>;

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

Vec sub(const Vec& a, const Vec& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec crs(const Vec& a, const Vec& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double dt(const Vec& a, const Vec& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void put_u32(std::ostream& stream, uint32_t value) {
    const unsigned char bytes[] = {static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8),
                                   static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 24)};
    stream.write(reinterpret_cast<const char*>(bytes), 4);
}

void put_f32(std::ostream& stream, double value) {
    const float converted = static_cast<float>(value);
    uint32_t bits;
    std::memcpy(&bits, &converted, 4);
    put_u32(stream, bits);
}

void write_stl(const std::filesystem::path& path, const std::vector<Tri>& triangles) {
    std::ofstream stream(path, std::ios::binary);
    std::array<char, 80> header{};
    const char signature[] = "stl2stp review fixture";
    std::copy(std::begin(signature), std::end(signature), header.begin());
    stream.write(header.data(), header.size());
    put_u32(stream, static_cast<uint32_t>(triangles.size()));
    for (const auto& triangle : triangles) {
        for (int axis = 0; axis < 3; ++axis) { put_f32(stream, 0); }
        for (const auto& point : triangle) { for (const double c : point) { put_f32(stream, c); } }
        stream.put(0);
        stream.put(0);
    }
    require(bool(stream), "Cannot write fixture " + path.u8string());
}

std::vector<Tri> read_stl(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    require(bytes.size() >= 84, "Saved STL too short");
    uint32_t count;
    std::memcpy(&count, bytes.data() + 80, 4);
    require(bytes.size() == 84 + std::size_t(count) * 50, "Saved STL size mismatch");
    std::vector<Tri> result(count);
    for (uint32_t index = 0; index < count; ++index) {
        const char* record = bytes.data() + 84 + std::size_t(index) * 50 + 12;
        for (int corner = 0; corner < 3; ++corner) {
            for (int axis = 0; axis < 3; ++axis) {
                float value;
                std::memcpy(&value, record + (corner * 3 + axis) * 4, 4);
                result[index][corner][axis] = value;
            }
        }
    }
    return result;
}

// Unit cube [0,1]^3 with each face split into n x n quads (2 triangles each), outward winding.
std::vector<Tri> cube(int n, Vec offset = {0, 0, 0}) {
    std::vector<Tri> result;
    for (int axis = 0; axis < 3; ++axis) {
        for (int side = 0; side < 2; ++side) {
            const int u = (axis + 1) % 3, v = (axis + 2) % 3;
            auto at = [&](int i, int j) {
                Vec p{};
                p[axis] = side;
                p[u] = double(i) / n;
                p[v] = double(j) / n;
                return Vec{p[0] + offset[0], p[1] + offset[1], p[2] + offset[2]};
            };
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    Tri first{at(i, j), at(i + 1, j), at(i + 1, j + 1)};
                    Tri second{at(i, j), at(i + 1, j + 1), at(i, j + 1)};
                    if (side == 0) { std::swap(first[1], first[2]); std::swap(second[1], second[2]); }
                    result.push_back(first);
                    result.push_back(second);
                }
            }
        }
    }
    return result;
}

std::vector<Tri> plane(int n) {
    std::vector<Tri> result;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            const Vec a{double(i) / n, double(j) / n, 0}, b{double(i + 1) / n, double(j) / n, 0};
            const Vec c{double(i + 1) / n, double(j + 1) / n, 0}, d{double(i) / n, double(j + 1) / n, 0};
            result.push_back({a, b, c});
            result.push_back({a, c, d});
        }
    }
    return result;
}

// UV sphere of radius r, `segments` x `rings`, built like typical exporters and the app's sample: vertex
// (i, j) at polar angle pi*j/rings. With exact_poles=false the poles are computed with sin/cos like every
// other ring, so the south pole becomes `segments` distinct points ~1e-15*r apart (sin(pi) != 0 in floating
// point): a microscopic hole that bit-exact welding (desktop-compatible) must report as open.
std::vector<Tri> uv_sphere(int segments, int rings, double r, bool exact_poles) {
    const double pi = std::acos(-1.0);
    auto at = [&](int i, int j) {
        if (exact_poles && j == 0) { return Vec{0, 0, r}; }
        if (exact_poles && j == rings) { return Vec{0, 0, -r}; }
        const double theta = pi * j / rings, phi = 2 * pi * (i % segments) / segments;
        return Vec{r * std::sin(theta) * std::cos(phi), r * std::sin(theta) * std::sin(phi), r * std::cos(theta)};
    };
    std::vector<Tri> result;
    for (int j = 0; j < rings; ++j) {
        for (int i = 0; i < segments; ++i) {
            const Vec a = at(i, j), b = at(i + 1, j), c = at(i + 1, j + 1), d = at(i, j + 1);
            if (j != 0) { result.push_back({a, d, b}); }
            if (j != rings - 1) { result.push_back({b, d, c}); }
        }
    }
    return result;
}

std::vector<Tri> rotated(std::vector<Tri> triangles) {
    // Arbitrary rotation (axis (1,2,3), 0.7 rad) and translation; written as float32 by write_stl.
    const Vec axis{1 / std::sqrt(14.0), 2 / std::sqrt(14.0), 3 / std::sqrt(14.0)};
    const double angle = 0.7, c = std::cos(angle), s = std::sin(angle);
    for (auto& triangle : triangles) {
        for (auto& p : triangle) {
            const Vec k = crs(axis, p);
            const double d = dt(axis, p);
            p = {p[0] * c + k[0] * s + axis[0] * d * (1 - c) + 12.5, p[1] * c + k[1] * s + axis[1] * d * (1 - c) - 3.25,
                 p[2] * c + k[2] * s + axis[2] * d * (1 - c) + 40.0};
        }
    }
    return triangles;
}

struct MeshReport {
    double area = 0, volume = 0;
    Vec minimum{1e300, 1e300, 1e300}, maximum{-1e300, -1e300, -1e300};
    std::size_t faces = 0, non_manifold_edges = 0, boundary_edges = 0, duplicate_faces = 0, vertices = 0, edges = 0;
};

MeshReport analyse(const std::vector<Tri>& triangles) {
    MeshReport report;
    std::map<std::array<float, 3>, std::size_t> ids;
    std::map<std::pair<std::size_t, std::size_t>, int> edge_use;
    std::set<std::array<std::size_t, 3>> unique;
    for (const auto& triangle : triangles) {
        std::array<std::size_t, 3> index{};
        for (int corner = 0; corner < 3; ++corner) {
            const auto& p = triangle[corner];
            index[corner] = ids.emplace(std::array<float, 3>{float(p[0]), float(p[1]), float(p[2])}, ids.size()).first->second;
            for (int axis = 0; axis < 3; ++axis) {
                report.minimum[axis] = std::min(report.minimum[axis], p[axis]);
                report.maximum[axis] = std::max(report.maximum[axis], p[axis]);
            }
        }
        const Vec n = crs(sub(triangle[1], triangle[0]), sub(triangle[2], triangle[0]));
        report.area += 0.5 * std::sqrt(dt(n, n));
        report.volume += dt(triangle[0], crs(triangle[1], triangle[2])) / 6.0;
        auto sorted = index;
        std::sort(sorted.begin(), sorted.end());
        if (!unique.insert(sorted).second) { ++report.duplicate_faces; }
        for (int corner = 0; corner < 3; ++corner) {
            const auto a = index[corner], b = index[(corner + 1) % 3];
            ++edge_use[{std::min(a, b), std::max(a, b)}];
        }
    }
    report.faces = triangles.size();
    report.vertices = ids.size();
    report.edges = edge_use.size();
    for (const auto& entry : edge_use) {
        if (entry.second == 1) { ++report.boundary_edges; }
        if (entry.second > 2) { ++report.non_manifold_edges; }
    }
    return report;
}

struct StepReport {
    std::size_t solids = 0, free_shells = 0, invalid_solids = 0, faces = 0;
    double volume = 0, area = 0;
    bool valid = false;
    bool brep_solid_entity = false;
    bool healed_valid = false;  // validity after OCCT's default reader-side healing
};

StepReport read_step(const std::filesystem::path& path) {
    STEPControl_Reader reader;
    require(reader.ReadFile(path.u8string().c_str()) == IFSelect_RetDone, "Cannot read " + path.u8string());
    // Disable reader-side shape healing: other CAD systems get the file as written, so we must too.
    reader.SetShapeProcessFlags(ShapeProcess::OperationsFlags());
    require(reader.TransferRoots() > 0, "No STEP roots in " + path.u8string());
    const TopoDS_Shape shape = reader.OneShape();
    StepReport report;
    BRepCheck_Analyzer analyzer(shape);
    report.valid = analyzer.IsValid();
    if (!report.valid) {  // diagnostics: first few invalid sub-shapes and their BRepCheck status codes
        int printed = 0;
        for (const auto type : {TopAbs_VERTEX, TopAbs_EDGE, TopAbs_WIRE, TopAbs_FACE, TopAbs_SHELL, TopAbs_SOLID}) {
            for (TopExp_Explorer it(shape, type); it.More() && printed < 6; it.Next()) {
                const auto result = analyzer.Result(it.Current());
                if (result.IsNull()) { continue; }
                for (const auto status : result->Status()) {
                    if (status != BRepCheck_NoError) {
                        std::cout << "  brepcheck type=" << int(type) << " status=" << int(status);
                        if (type == TopAbs_VERTEX) { std::cout << " tol=" << BRep_Tool::Tolerance(TopoDS::Vertex(it.Current())); }
                        if (type == TopAbs_EDGE) { std::cout << " tol=" << BRep_Tool::Tolerance(TopoDS::Edge(it.Current())); }
                        std::cout << '\n';
                        ++printed;
                        break;
                    }
                }
            }
        }
    }
    for (TopExp_Explorer solid(shape, TopAbs_SOLID); solid.More(); solid.Next()) {
        ++report.solids;
        GProp_GProps props;
        BRepGProp::VolumeProperties(solid.Current(), props);
        report.volume += props.Mass();
        if (!BRepCheck_Analyzer(solid.Current()).IsValid() || props.Mass() <= 0) { ++report.invalid_solids; }
    }
    for (TopExp_Explorer shell(shape, TopAbs_SHELL, TopAbs_SOLID); shell.More(); shell.Next()) { ++report.free_shells; }
    for (TopExp_Explorer face(shape, TopAbs_FACE); face.More(); face.Next()) { ++report.faces; }
    GProp_GProps surface;
    BRepGProp::SurfaceProperties(shape, surface);
    report.area = surface.Mass();
    std::ifstream text_stream(path, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(text_stream)), std::istreambuf_iterator<char>());
    report.brep_solid_entity = text.find("MANIFOLD_SOLID_BREP") != std::string::npos;
    STEPControl_Reader healed;
    if (healed.ReadFile(path.u8string().c_str()) == IFSelect_RetDone && healed.TransferRoots() > 0) {
        report.healed_valid = BRepCheck_Analyzer(healed.OneShape()).IsValid();
    }
    return report;
}

std::string describe(const StepReport& r) {
    return "solids=" + std::to_string(r.solids) + " free_shells=" + std::to_string(r.free_shells) + " faces=" +
           std::to_string(r.faces) + " invalid_solids=" + std::to_string(r.invalid_solids) + " volume=" +
           std::to_string(r.volume) + " area=" + std::to_string(r.area) + " brepcheck=" + (r.valid ? "valid" : "INVALID") +
           " healed=" + (r.healed_valid ? "valid" : "INVALID");
}

std::string describe(const MeshReport& r) {
    return "faces=" + std::to_string(r.faces) + " area=" + std::to_string(r.area) + " volume=" + std::to_string(r.volume) +
           " non_manifold_edges=" + std::to_string(r.non_manifold_edges) + " duplicate_faces=" + std::to_string(r.duplicate_faces) +
           " boundary_edges=" + std::to_string(r.boundary_edges) + " euler=" +
           std::to_string(long(r.vertices) - long(r.edges) + long(r.faces)) + " bbox_y=[" + std::to_string(r.minimum[1]) + "," +
           std::to_string(r.maximum[1]) + "]";
}

struct Fixture {
    Handle handle{stl2stp_create(), &stl2stp_destroy};
    std::filesystem::path output;

    stl2stp_status load(const std::string& name, const std::vector<Tri>& triangles) {
        const auto path = output / (name + ".stl");
        write_stl(path, triangles);
        return stl2stp_load_stl(handle.get(), path.u8string().c_str(), nullptr);
    }

    stl2stp_status export_step(const std::string& name, stl2stp_export_options options, StepReport& report) {
        const auto path = output / (name + ".step");
        const auto status = stl2stp_export_step(handle.get(), path.u8string().c_str(), &options, nullptr, nullptr);
        if (status >= 0) { report = read_step(path); }
        return status;
    }

    MeshReport simplify(std::size_t target, stl2stp_status& status) {
        status = stl2stp_simplify(handle.get(), target, nullptr);
        require(status >= 0, std::string("simplify failed: ") + stl2stp_last_error(handle.get()));
        const auto path = output / "simplified.stl";
        require(stl2stp_save_stl(handle.get(), path.u8string().c_str(), nullptr) == STL2STP_OK, "save failed");
        return analyse(read_stl(path));
    }
};

// ORIGINAL mode must never report success while writing an invalid or negative-volume solid.
void original_case(Fixture& f, const std::string& name, const std::vector<Tri>& mesh, bool closed, double volume,
                   std::size_t solids, int expected_status = -100) {
    const auto loaded = f.load(name, mesh);
    require(loaded >= 0, "load failed");
    for (const int merge : {0, 1}) {
        auto options = stl2stp_default_export_options();
        options.merge_coplanar = merge;
        StepReport report;
        const auto status = f.export_step(name + (merge ? "-merged" : ""), options, report);
        std::cout << name << (merge ? " merged" : "") << ": status=" << status << " msg='" << stl2stp_last_error(f.handle.get())
                  << "' " << describe(report) << '\n';
        require(status >= 0, "export failed");
        if (expected_status != -100) {
            require(status == expected_status, name + ": expected status " + std::to_string(expected_status));
        }
        require(report.invalid_solids == 0, name + ": exported an invalid or negative-volume solid");
        require(report.valid, name + ": exported shape fails BRepCheck");
        if (closed) {
            require(report.solids == solids && report.free_shells == 0, name + ": expected " + std::to_string(solids) + " solid(s)");
            require(std::abs(report.volume - volume) < 1e-4 * volume, name + ": volume mismatch");
        } else {
            require(report.solids == 0 && report.free_shells >= 1 && !report.brep_solid_entity,
                    name + ": open input must be exported as shell/surfaces, not MANIFOLD_SOLID_BREP");
            require(status != STL2STP_OK, name + ": open input must return a warning status");
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "Usage: stl2stp-review-tests <case> <output-dir>");
        const std::string which = argv[1];
        Fixture f;
        f.output = std::filesystem::u8path(argv[2]);
        std::filesystem::create_directories(f.output);
        require(bool(f.handle), "handle");

        if (which == "original_closed_cube") {
            original_case(f, "closed", cube(1), true, 1.0, 1, STL2STP_OK);
        } else if (which == "original_open_box") {
            auto open = cube(1);
            open.erase(open.begin(), open.begin() + 2);
            original_case(f, "open-box", open, false, 0, 0, STL2STP_WARNING_NOT_SOLID);
        } else if (which == "original_flipped_triangle") {
            auto mesh = cube(1);
            std::swap(mesh[5][1], mesh[5][2]);
            original_case(f, "flipped-one", mesh, true, 1.0, 1, STL2STP_WARNING_REPAIRED);
        } else if (which == "original_inverted") {
            auto mesh = cube(1);
            for (auto& t : mesh) { std::swap(t[1], t[2]); }
            original_case(f, "inverted", mesh, true, 1.0, 1, STL2STP_WARNING_REPAIRED);
        } else if (which == "original_non_manifold_edge") {
            auto mesh = cube(1);
            const auto second = cube(1, {1, 1, 0});  // shares the edge x=1,y=1 with the first cube
            mesh.insert(mesh.end(), second.begin(), second.end());
            const auto loaded = f.load("nonmanifold", mesh);
            require(loaded >= 0, "load failed");
            StepReport report;
            const auto status = f.export_step("nonmanifold", stl2stp_default_export_options(), report);
            std::cout << "nonmanifold: status=" << status << " " << describe(report) << '\n';
            require(status >= 0 && report.invalid_solids == 0 && report.valid, "non-manifold input exported as invalid solid");
            require(std::abs(report.volume - 2.0) < 1e-4, "non-manifold pair volume must be 2");
            require(status == STL2STP_WARNING_REPAIRED, "non-manifold fallback must report REPAIRED");
        } else if (which == "original_collinear_sliver") {
            auto mesh = cube(1);
            mesh.push_back({Vec{0, 0, 0}, Vec{0.5, 0, 0}, Vec{1, 0, 0}});
            original_case(f, "collinear", mesh, true, 1.0, 1, STL2STP_WARNING_REPAIRED);
        } else if (which == "original_disconnected") {
            auto mesh = cube(1);
            const auto second = cube(1, {3, 0, 0});
            mesh.insert(mesh.end(), second.begin(), second.end());
            original_case(f, "disconnected", mesh, true, 2.0, 2, STL2STP_OK);
        } else if (which == "qem_open_boundary") {
            require(f.load("plane", plane(20)) == STL2STP_OK, "load plane");
            stl2stp_status status;
            const auto report = f.simplify(200, status);
            std::cout << "plane 800->200: status=" << status << " " << describe(report) << '\n';
            require(std::abs(report.area - 1.0) < 1e-3, "open boundary not preserved: area changed");
            require(report.minimum[0] < 1e-6 && report.minimum[1] < 1e-6 && report.maximum[0] > 1 - 1e-6 &&
                        report.maximum[1] > 1 - 1e-6, "open boundary not preserved: bbox shrank");
            require(report.non_manifold_edges == 0 && report.duplicate_faces == 0, "plane became non-manifold");
            require(f.load("plane", plane(20)) == STL2STP_OK, "reload plane");
            const auto low = f.simplify(10, status);
            std::cout << "plane 800->10: status=" << status << " " << describe(low) << '\n';
            require(std::abs(low.area - 1.0) < 1e-3, "aggressive target collapsed the open plane");
        } else if (which == "qem_link_condition") {
            require(f.load("cube10", cube(10)) == STL2STP_OK, "load cube10");
            stl2stp_status status;
            const auto report = f.simplify(8, status);
            std::cout << "cube 1200->8: status=" << status << " " << describe(report) << '\n';
            require(report.non_manifold_edges == 0, "simplification produced non-manifold edges");
            require(report.duplicate_faces == 0, "simplification produced duplicate faces");
            require(report.boundary_edges == 0, "closed mesh became open");
            require(long(report.vertices) - long(report.edges) + long(report.faces) == 2, "closed mesh lost Euler characteristic 2");
            require(report.volume > 0, "volume must stay positive");
            require(f.load("cube10", cube(10)) == STL2STP_OK, "reload cube10");
            const auto twelve = f.simplify(12, status);
            std::cout << "cube 1200->12: status=" << status << " " << describe(twelve) << '\n';
            require(twelve.faces == 12 && std::abs(twelve.volume - 1.0) < 1e-5, "1200->12 must recover the exact cube");
        } else if (which == "qem_closed_sphere") {
            // A closed UV sphere must stay closed and watertight at 50 % and 25 %, reach the target, and export
            // as exactly one valid solid (2026-10-21 investigation: the "simplified sphere is open" report came
            // from a test mesh with an unwelded south pole, see qem_near_coincident_pole).
            const auto mesh = uv_sphere(64, 32, 25.0, true);
            const auto input = analyse(mesh);
            std::cout << "input: " << describe(input) << '\n';
            require(input.boundary_edges == 0 && input.non_manifold_edges == 0, "fixture must be closed");
            {
                require(f.load("sphere", mesh) == STL2STP_OK, "load sphere");
                StepReport step;
                const auto exported = f.export_step("sphere-100", stl2stp_default_export_options(), step);
                std::cout << "100%: step status=" << exported << " " << describe(step) << '\n';
                require(exported == STL2STP_OK && step.solids == 1 && step.free_shells == 0 && step.valid,
                        "unsimplified closed sphere must be one valid solid");
            }
            for (const std::size_t percent : {50, 25}) {
                require(f.load("sphere", mesh) == STL2STP_OK, "load sphere");
                const std::size_t target = mesh.size() * percent / 100;
                stl2stp_status status;
                const auto report = f.simplify(target, status);
                std::cout << percent << "% (" << mesh.size() << "->" << target << "): status=" << status << " "
                          << describe(report) << '\n';
                require(status == STL2STP_OK, "target not reached");
                require(report.faces <= target && report.faces + 2 >= target, "face count far from target");
                require(report.boundary_edges == 0, "closed sphere became open");
                require(report.non_manifold_edges == 0 && report.duplicate_faces == 0, "simplified sphere not manifold");
                require(long(report.vertices) - long(report.edges) + long(report.faces) == 2, "Euler characteristic must stay 2");
                require(report.volume > 0.9 * input.volume && report.volume < 1.02 * input.volume, "volume drifted");
                for (const int merge : {0, 1}) {
                    auto options = stl2stp_default_export_options();
                    options.merge_coplanar = merge;
                    StepReport step;
                    const auto exported = f.export_step("sphere-" + std::to_string(percent) + (merge ? "-merged" : ""), options, step);
                    std::cout << "  step" << (merge ? " merged" : "") << ": status=" << exported << " " << describe(step) << '\n';
                    require(exported == STL2STP_OK, "simplified closed sphere must export without warnings");
                    require(step.solids == 1 && step.free_shells == 0, "simplified closed sphere must be exactly one solid");
                    if (merge) {
                        // Same known OCCT 8.0.1 limitation as merge_rotated: merged planar faces read back with ALL
                        // reader healing disabled report BRepCheck_UnorientableShape; default readback is valid.
                        require(step.healed_valid, "merged simplified sphere must read back valid");
                    } else {
                        require(step.invalid_solids == 0 && step.valid, "simplified closed sphere must pass BRepCheck");
                    }
                    require(std::abs(step.volume - report.volume) < 1e-3 * report.volume, "STEP volume differs from mesh");
                }
            }
        } else if (which == "qem_near_coincident_pole") {
            // Sphere whose south pole is 64 distinct points ~1e-15 apart: open (128 boundary edges) under bit-exact
            // welding. The core must report it as open consistently, before and after simplification, and the
            // simplifier must not open it further or create non-manifold edges.
            const auto mesh = uv_sphere(64, 32, 25.0, false);
            const auto input = analyse(mesh);
            std::cout << "input: " << describe(input) << '\n';
            require(input.boundary_edges == 128, "fixture must have the 128-edge pole hole");
            require(f.load("pole", mesh) == STL2STP_OK, "load pole");
            auto exact = stl2stp_default_export_options();
            exact.weld_vertices = 0;  // desktop-compatible exact joining
            StepReport before;
            require(f.export_step("pole", exact, before) == STL2STP_WARNING_NOT_SOLID,
                    "unsimplified open sphere must warn NOT_SOLID");
            require(before.solids == 0, "open input must not become a solid");
            for (const std::size_t percent : {50, 25}) {
                require(f.load("pole", mesh) == STL2STP_OK, "reload pole");
                stl2stp_status status;
                const auto report = f.simplify(mesh.size() * percent / 100, status);
                std::cout << percent << "%: status=" << status << " " << describe(report) << '\n';
                require(report.boundary_edges <= input.boundary_edges, "simplification opened the mesh further");
                require(report.non_manifold_edges == 0 && report.duplicate_faces == 0, "simplified mesh not manifold");
                StepReport after;
                require(f.export_step("pole-" + std::to_string(percent), exact, after) ==
                            STL2STP_WARNING_NOT_SOLID, "simplified open sphere must still warn NOT_SOLID");
            }
            // SEWING mode (tolerance 1e-6 relative) closes the microscopic hole: one solid.
            require(f.load("pole", mesh) == STL2STP_OK, "reload pole for sewing");
            auto sewing = stl2stp_default_export_options();
            sewing.mode = STL2STP_MODE_SEWING;
            StepReport sewn;
            const auto sewn_status = f.export_step("pole-sewing", sewing, sewn);
            std::cout << "sewing: status=" << sewn_status << " " << describe(sewn) << '\n';
            require(sewn_status >= 0 && sewn.solids == 1 && sewn.invalid_solids == 0, "sewing must close the pole into one solid");
        } else if (which == "weld_open_pole") {
            // core/weld: the open-pole sphere (64 south-pole points ~1e-15 apart) becomes one solid in ORIGINAL
            // mode with the default weld, stays open with the weld off, and stays closed through QEM when welded
            // first (stl2stp_weld_vertices).
            const auto mesh = uv_sphere(64, 32, 25.0, false);
            require(f.load("weld-pole", mesh) == STL2STP_OK, "load");
            StepReport welded;
            const auto status = f.export_step("weld-pole", stl2stp_default_export_options(), welded);
            const std::string message = stl2stp_last_error(f.handle.get());
            std::cout << "weld on: status=" << status << " msg='" << message << "' " << describe(welded) << '\n';
            require(status == STL2STP_WARNING_REPAIRED, "weld must report REPAIRED");
            require(message.find("63 near-coincident vertex(es) welded") != std::string::npos, "weld note must count 63 joins");
            require(welded.solids == 1 && welded.free_shells == 0 && welded.invalid_solids == 0 && welded.valid, "weld on: 1 valid solid");
            require(welded.faces == mesh.size(), "weld must keep every triangle");
            auto off = stl2stp_default_export_options();
            off.weld_vertices = 0;
            StepReport open;
            require(f.export_step("weld-pole-off", off, open) == STL2STP_WARNING_NOT_SOLID && open.solids == 0, "weld off: still open");
            auto legacy = stl2stp_default_export_options();
            legacy.strict_original = 1;  // legacy output is always exact, weld ignored
            StepReport strict;
            require(f.export_step("weld-pole-strict", legacy, strict) == STL2STP_OK, "strict legacy export");
            for (const std::size_t percent : {50, 25}) {
                require(f.load("weld-pole", mesh) == STL2STP_OK, "reload");
                uint64_t merged = 0;
                require(stl2stp_weld_vertices(f.handle.get(), 0, &merged) == STL2STP_OK && merged == 63, "API weld joins 63");
                stl2stp_mesh_stats stats{};
                require(stl2stp_get_mesh_stats(f.handle.get(), &stats) == STL2STP_OK, "stats");
                require(stats.vertices == 1986 && stats.faces == mesh.size(), "welded sphere: 1986 vertices, all faces");
                const std::size_t target = mesh.size() * percent / 100;
                stl2stp_status simplified;
                const auto report = f.simplify(target, simplified);
                std::cout << percent << "% after weld: " << describe(report) << '\n';
                require(report.boundary_edges == 0 && report.non_manifold_edges == 0, "welded sphere stays closed through QEM");
                require(report.faces <= target && report.faces + 2 >= target, "target");
                StepReport step;
                const auto exported = f.export_step("weld-pole-" + std::to_string(percent), stl2stp_default_export_options(), step);
                require(exported == STL2STP_OK && step.solids == 1 && step.valid, "simplified welded sphere: 1 solid");
            }
        } else if (which == "weld_guards") {
            // Thin features survive: a 40 x 40 slab only 2x / 100x the automatic tolerance thick stays a slab.
            for (const double thickness_factor : {2.0, 100.0}) {
                auto slab = cube(4);
                for (auto& t : slab) { for (auto& p : t) { p[0] *= 40; p[1] *= 40; } }
                double diagonal = std::sqrt(40.0 * 40 * 2);
                const double thickness = thickness_factor * 1e-6 * diagonal;
                for (auto& t : slab) { for (auto& p : t) { p[2] *= thickness; } }
                require(f.load("slab", slab) == STL2STP_OK, "load slab");
                StepReport step;
                const auto status = f.export_step("slab", stl2stp_default_export_options(), step);
                std::cout << "slab " << thickness << ": status=" << status << " msg='" << stl2stp_last_error(f.handle.get())
                          << "' " << describe(step) << '\n';
                require(status == STL2STP_OK, "thin slab must not be welded (no REPAIRED note)");
                require(step.solids == 1 && step.valid && std::abs(step.volume - 1600 * thickness) < 1e-3 * 1600 * thickness,
                        "thin slab volume kept");
                uint64_t merged = 99;
                require(stl2stp_weld_vertices(f.handle.get(), 0, &merged) == STL2STP_OK && merged == 0, "API: nothing to weld on slab");
            }
            // Never collapse a triangle: p and q are 1e-7 apart but share a triangle (a real sliver) -> kept;
            // r is 1e-7 from p and only in another triangle -> joined.
            const Vec p{0, 0, 0}, q{1e-7, 0, 0}, r{0, 1e-7, 0};
            std::vector<Tri> pieces{{p, q, Vec{0, 10, 0}}, {r, Vec{10, 0, 1}, Vec{10, 10, 1}}};
            require(f.load("sliver", pieces) == STL2STP_OK, "load sliver");
            uint64_t merged = 0;
            require(stl2stp_weld_vertices(f.handle.get(), 0, &merged) == STL2STP_OK, "weld sliver");
            stl2stp_mesh_stats stats{};
            require(stl2stp_get_mesh_stats(f.handle.get(), &stats) == STL2STP_OK, "stats");
            std::cout << "sliver: merged=" << merged << " vertices=" << stats.vertices << " faces=" << stats.faces << '\n';
            require(merged == 1 && stats.vertices == 5 && stats.faces == 2, "sliver kept, r joined to p");
            // Explicit override and argument checks.
            require(f.load("weld-pole", uv_sphere(64, 32, 25.0, false)) == STL2STP_OK, "load");
            require(stl2stp_weld_vertices(f.handle.get(), 1e-20, &merged) == STL2STP_OK && merged == 0,
                    "override below the pole gap joins nothing");
            require(stl2stp_weld_vertices(f.handle.get(), std::nan(""), &merged) == STL2STP_ERROR_ARGUMENT, "NaN rejected");
        } else if (which == "qem_scale_invariance") {
            // Same subdivided cube at 1e-3 and 1e3 scale must simplify to the same face count and relative volume.
            std::vector<double> volumes;
            for (const double scale : {1e-3, 1.0, 1e3}) {
                auto mesh = cube(6);
                for (auto& t : mesh) { for (auto& p : t) { for (auto& c : p) { c *= scale; } } }
                require(f.load("scaled", mesh) == STL2STP_OK, "load scaled");
                stl2stp_status status;
                const auto report = f.simplify(60, status);
                const double relative = report.volume / (scale * scale * scale);
                std::cout << "scale " << scale << " 432->60: status=" << status << " faces=" << report.faces
                          << " rel_volume=" << relative << " nm=" << report.non_manifold_edges << '\n';
                require(report.non_manifold_edges == 0 && report.duplicate_faces == 0, "scaled cube non-manifold");
                volumes.push_back(relative);
            }
            require(std::abs(volumes[0] - volumes[1]) < 1e-3 && std::abs(volumes[2] - volumes[1]) < 1e-3,
                    "QEM result depends on absolute model scale");
        } else if (which == "merge_rotated") {
            for (const int n : {1, 4, 10}) {
                const auto name = "rotated-" + std::to_string(n);
                require(f.load(name, rotated(cube(n))) == STL2STP_OK, "load rotated");
                for (const auto mode : {STL2STP_MODE_ORIGINAL, STL2STP_MODE_SEWING}) {
                    auto options = stl2stp_default_export_options();
                    options.mode = mode;
                    options.merge_coplanar = 1;
                    StepReport report;
                    const auto status = f.export_step(name + (mode ? "-sewing" : "-original"), options, report);
                    std::cout << name << " (" << 12 * n * n << " tri) " << (mode ? "sewing" : "original")
                              << " merged: status=" << status << " " << describe(report) << '\n';
                    // Known limitation: OCCT 8.0.1 reports BRepCheck_UnorientableShape on some merged rotated
                    // planar faces when the file is read back with ALL reader healing disabled, although
                    // the exported shape is valid in memory and the default (healing) readback is valid.
                    require(status >= 0 && report.healed_valid && report.solids == 1,
                            "rotated merge must yield one valid solid");
                    require(std::abs(report.volume - 1.0) < 1e-3, "rotated merge volume changed");
                    if (n > 1) { require(report.faces == 6, "rotated float32 cube should merge to 6 faces"); }
                    else { require(report.faces <= 12, "rotated 12-triangle cube face count"); }
                }
            }
        } else if (which == "original_strict_legacy") {
            // strict_original keeps the byte-for-byte legacy behaviour (documented as unsafe).
            auto open = cube(1);
            open.erase(open.begin(), open.begin() + 2);
            require(f.load("legacy-open", open) == STL2STP_OK, "load");
            auto options = stl2stp_default_export_options();
            options.strict_original = 1;
            StepReport report;
            const auto status = f.export_step("legacy-open", options, report);
            std::cout << "legacy open-box: status=" << status << " " << describe(report) << '\n';
            require(status == STL2STP_OK && report.solids == 1 && report.brep_solid_entity, "strict_original must keep legacy output");
        } else if (which == "iges_non_ascii") {
            require(f.load("cube", cube(1)) == STL2STP_OK, "load");
            auto options = stl2stp_default_export_options();
            options.author = "致知屋 Bo";
            options.organization = "Café";
            options.description = "测试";
            const auto path = f.output / "meta.igs";
            const auto status = stl2stp_export_iges(f.handle.get(), path.u8string().c_str(), &options, nullptr, nullptr);
            std::cout << "iges non-ascii: status=" << status << " msg='" << stl2stp_last_error(f.handle.get()) << "'\n";
            require(status == STL2STP_WARNING_METADATA_CHANGED, "non-ASCII IGES metadata must warn");
            std::ifstream stream(path, std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
            require(std::none_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; }),
                    "IGES output must be pure ASCII");
            require(text.find("6H??? Bo") != std::string::npos && text.find("4HCaf?") != std::string::npos,
                    "IGES Hollerith strings must hold the ASCII replacement");
            options.author = "Bo";
            options.organization = "ACME";
            options.description = "plain";
            require(stl2stp_export_iges(f.handle.get(), path.u8string().c_str(), &options, nullptr, nullptr) == STL2STP_OK,
                    "ASCII IGES metadata must not warn");
        } else if (which == "malformed_fuzz") {
            // Deterministic mutation fuzz of small STL inputs: no crash, no sanitizer report, sane statuses.
            std::mt19937 random(20261020u);
            write_stl(f.output / "seed.stl", cube(2));
            std::ifstream seed_stream(f.output / "seed.stl", std::ios::binary);
            const std::string seed((std::istreambuf_iterator<char>(seed_stream)), std::istreambuf_iterator<char>());
            const std::string ascii_seed =
                "solid s\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\n"
                "facet normal 0 0 1\nouter loop\nvertex 1 0 0\nvertex 1 1 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid s\n";
            std::size_t loaded = 0, exported = 0;
            for (int iteration = 0; iteration < 300; ++iteration) {
                std::string data = iteration % 3 == 2 ? ascii_seed : seed;
                const int kind = int(random() % 6);
                if (kind == 0) { data.resize(random() % (data.size() + 1)); }
                else if (kind == 1) { for (int k = 0; k < 8; ++k) { data[random() % data.size()] ^= char(1u << (random() % 8)); } }
                else if (kind == 2 && data.size() > 84) { const uint32_t c = random(); std::memcpy(&data[80], &c, 4); }
                else if (kind == 3 && data.size() > 96) {
                    const float specials[] = {NAN, INFINITY, -INFINITY, 1e38f, -1e38f, 0.f, 1e-38f};
                    for (int k = 0; k < 4; ++k) {
                        const std::size_t off = 84 + 12 + (random() % ((data.size() - 96) / 4)) * 4;
                        std::memcpy(&data[off], &specials[random() % 7], 4);
                    }
                }
                else if (kind == 4) { data.insert(random() % data.size(), std::string(random() % 64, char(random()))); }
                else { for (int k = 0; k < 4; ++k) { data[random() % data.size()] = "0123456789.eE+-naif \n"[random() % 21]; } }
                const auto path = f.output / "fuzz.stl";
                { std::ofstream(path, std::ios::binary) << data; }
                Handle handle(stl2stp_create(), &stl2stp_destroy);
                const auto status = stl2stp_load_stl(handle.get(), path.u8string().c_str(), nullptr);
                require(status == STL2STP_OK || status == STL2STP_WARNING_PARTIAL || status == STL2STP_ERROR_FORMAT ||
                            status == STL2STP_ERROR_IO, "unexpected load status " + std::to_string(status));
                if (status < 0) { continue; }
                ++loaded;
                stl2stp_mesh_stats stats{};
                (void)stl2stp_get_mesh_stats(handle.get(), &stats);
                if (stats.faces > 2) { (void)stl2stp_simplify(handle.get(), stats.faces / 2, nullptr); }
                if (iteration % 10 == 0) {
                    for (const auto mode : {STL2STP_MODE_ORIGINAL, STL2STP_MODE_SEWING}) {
                        auto options = stl2stp_default_export_options();
                        options.mode = mode;
                        const auto out = (f.output / "fuzz.step").u8string();
                        const auto result = stl2stp_export_step(handle.get(), out.c_str(), &options, nullptr, nullptr);
                        require(result >= STL2STP_ERROR_INTERNAL && result != STL2STP_ERROR_INTERNAL,
                                std::string("internal error on fuzz input: ") + stl2stp_last_error(handle.get()));
                        exported += result >= 0;
                    }
                }
            }
            std::cout << "fuzz: 300 inputs, loaded=" << loaded << " exported=" << exported << '\n';
        } else {
            throw std::runtime_error("Unknown review case " + which);
        }
        std::cout << "PASS " << which << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
