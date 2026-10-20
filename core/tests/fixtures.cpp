#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Point = std::array<float, 3>;
using Triangle = std::array<Point, 3>;

void write_u32(std::ostream& stream, uint32_t value) {
    const unsigned char bytes[] = {static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8),
                                   static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 24)};
    stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

void write_float(std::ostream& stream, float value) {
    uint32_t bits;
    static_assert(sizeof(float) == sizeof(bits), "Requires float32");
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32(stream, bits);
}

void binary(const std::filesystem::path& path, const std::vector<Triangle>& triangles, bool solid_header = false) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) { throw std::runtime_error("Cannot write fixture"); }
    std::array<char, 80> header{};
    const std::string signature = solid_header ? "solid binary fixture" : "stl2stp generated test fixture";
    std::copy(signature.begin(), signature.end(), header.begin());
    stream.write(header.data(), header.size());
    write_u32(stream, static_cast<uint32_t>(triangles.size()));
    for (const auto& triangle : triangles) {
        for (int axis = 0; axis < 3; ++axis) { write_float(stream, 0); }
        for (const auto& point : triangle) {
            for (const auto coordinate : point) { write_float(stream, coordinate); }
        }
        stream.put(0);
        stream.put(0);
    }
    stream.close();
    if (!stream) { throw std::runtime_error("Fixture write failed"); }
}

void ascii(const std::filesystem::path& path, const std::vector<Triangle>& triangles, bool truncated = false) {
    std::ofstream stream(path);
    stream.imbue(std::locale::classic());
    stream << std::setprecision(9) << "solid generated\n";
    for (const auto& triangle : triangles) {
        stream << "facet normal 0 0 0\nouter loop\n";
        for (const auto& point : triangle) {
            stream << "vertex " << point[0] << ' ' << point[1] << ' ' << point[2] << '\n';
        }
        stream << "endloop\nendfacet\n";
    }
    if (truncated) { stream << "facet normal 0 0 1\nouter loop\nvertex 0 0"; }
    else { stream << "endsolid generated\n"; }
    stream.close();
    if (!stream) { throw std::runtime_error("ASCII fixture write failed"); }
}

std::vector<Triangle> cube(float offset = 0) {
    const std::array<Point, 8> points{{{offset, 0, 0}, {offset + 1, 0, 0}, {offset + 1, 1, 0}, {offset, 1, 0},
                                      {offset, 0, 1}, {offset + 1, 0, 1}, {offset + 1, 1, 1}, {offset, 1, 1}}};
    const std::array<std::array<std::size_t, 3>, 12> faces{{{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
                                                         {0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
                                                         {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}}};
    std::vector<Triangle> result;
    for (const auto& face : faces) { result.push_back({points[face[0]], points[face[1]], points[face[2]]}); }
    return result;
}

std::vector<Triangle> sphere(int rings, int segments) {
    const double pi = std::acos(-1.0);
    std::vector<Point> points{{0, 0, 1}};
    for (int ring = 1; ring < rings; ++ring) {
        const double latitude = pi * ring / rings;
        for (int segment = 0; segment < segments; ++segment) {
            const double longitude = 2 * pi * segment / segments;
            points.push_back({static_cast<float>(std::sin(latitude) * std::cos(longitude)),
                              static_cast<float>(std::sin(latitude) * std::sin(longitude)),
                              static_cast<float>(std::cos(latitude))});
        }
    }
    points.push_back({0, 0, -1});
    std::vector<Triangle> triangles;
    auto add = [&](std::size_t first, std::size_t second, std::size_t third) {
        Triangle triangle{points[first], points[second], points[third]};
        const auto& origin = triangle[0];
        const Point edge_first{triangle[1][0] - origin[0], triangle[1][1] - origin[1], triangle[1][2] - origin[2]};
        const Point edge_second{triangle[2][0] - origin[0], triangle[2][1] - origin[1], triangle[2][2] - origin[2]};
        const Point normal{edge_first[1] * edge_second[2] - edge_first[2] * edge_second[1],
                           edge_first[2] * edge_second[0] - edge_first[0] * edge_second[2],
                           edge_first[0] * edge_second[1] - edge_first[1] * edge_second[0]};
        if (normal[0] * origin[0] + normal[1] * origin[1] + normal[2] * origin[2] < 0) {
            std::swap(triangle[1], triangle[2]);
        }
        triangles.push_back(triangle);
    };
    for (int segment = 0; segment < segments; ++segment) {
        const auto next = (segment + 1) % segments;
        add(0, 1 + segment, 1 + next);
        for (int ring = 0; ring < rings - 2; ++ring) {
            const auto first = 1 + ring * segments + segment;
            const auto second = 1 + ring * segments + next;
            const auto third = first + segments;
            const auto fourth = second + segments;
            add(first, third, fourth);
            add(first, fourth, second);
        }
        add(points.size() - 1, 1 + (rings - 2) * segments + next, 1 + (rings - 2) * segments + segment);
    }
    return triangles;
}

int integer(const char* value) {
    const std::string text(value);
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) {
        throw std::runtime_error("Expected positive integer fixture size");
    }
    const int result = std::stoi(text);
    if (result < 3 || result > 1000) { throw std::runtime_error("Fixture size must be in [3, 1000]"); }
    return result;
}

}

int main(int argc, char** argv) {
    try {
        if (argc < 2) { throw std::runtime_error("Usage: fixture-generator <build-data-dir> [--rings N] [--segments N]"); }
        int rings = 100, segments = 100;
        for (int index = 2; index < argc; ++index) {
            const std::string argument = argv[index];
            if (index + 1 >= argc) { throw std::runtime_error("Missing fixture size"); }
            if (argument == "--rings") { rings = integer(argv[++index]); }
            else if (argument == "--segments") { segments = integer(argv[++index]); }
            else { throw std::runtime_error("Unknown fixture option"); }
        }
        const auto directory = std::filesystem::u8path(argv[1]);
        std::filesystem::create_directories(directory);
        const auto triangles = cube();
        binary(directory / "cube-binary.stl", triangles);
        binary(directory / "cube-solid-header.stl", triangles, true);
        ascii(directory / "cube-ascii.stl", triangles);
        auto two = triangles;
        const auto second = cube(3);
        two.insert(two.end(), second.begin(), second.end());
        binary(directory / "two-shells.stl", two);
        binary(directory / "truncated-binary.stl", triangles);
        std::filesystem::resize_file(directory / "truncated-binary.stl", 84 + 5 * 50 + 17);
        ascii(directory / "truncated-ascii.stl", {triangles.begin(), triangles.begin() + 5}, true);
        auto degenerate = triangles;
        degenerate.push_back({Point{0, 0, 0}, Point{0, 0, 0}, Point{0, 0, 0}});
        binary(directory / "degenerate.stl", degenerate);
        auto nonfinite = triangles;
        nonfinite[0][0][0] = std::numeric_limits<float>::infinity();
        binary(directory / "nonfinite.stl", nonfinite);
        binary(directory / "empty-binary.stl", {});
        std::ofstream(directory / "invalid.stl") << "Not an STL\n";
        std::ofstream(directory / "huge-ascii.stl")
            << "solid huge\nfacet normal 1 0 0\nouter loop\nvertex 1e308 0 0\n"
            << "vertex 1e308 1 0\nvertex 1e308 1 1\nendloop\nendfacet\n"
            << "facet normal 1 0 0\nouter loop\nvertex 1e308 0 0\n"
            << "vertex 1e308 1 1\nvertex 1e308 0 1\nendloop\nendfacet\nendsolid huge\n";
        const auto uv_sphere = sphere(rings, segments);
        binary(directory / "sphere.stl", uv_sphere);
        ascii(directory / "sphere-ascii.stl", uv_sphere);
        std::cout << "Generated cube=12 two-shells=24 sphere=" << uv_sphere.size() << " triangles\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
