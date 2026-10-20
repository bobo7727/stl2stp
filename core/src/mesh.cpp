#include "internal.hpp"

#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace stl2stp {

thread_local std::string thread_error;

void require_path(const char* path) {
    if (!path || !*path) {
        throw Error(STL2STP_ERROR_ARGUMENT, "Empty path");
    }
}

void require_mesh(const Mesh& mesh) {
    if (mesh.triangles.empty() && mesh.original_triangles.empty()) {
        throw Error(STL2STP_ERROR_EMPTY, "No mesh loaded");
    }
}

OutputFile::OutputFile(const char* path) {
    require_path(path);
    destination_ = std::filesystem::u8path(path);
    static std::atomic<unsigned long long> serial{0};
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
        directory_ = destination_;
        directory_ += ".stl2stp-" + std::to_string(timestamp) + "-" + std::to_string(serial++);
        std::error_code error;
        if (std::filesystem::create_directory(directory_, error)) {
            temporary_ = directory_ / "output.tmp";
            return;
        }
        if (error && error != std::errc::file_exists) {
            throw Error(STL2STP_ERROR_IO, "Cannot create temporary output: " + error.message());
        }
    }
    throw Error(STL2STP_ERROR_IO, "Cannot allocate temporary output directory");
}

OutputFile::~OutputFile() {
    std::error_code ignored;
    std::filesystem::remove_all(directory_, ignored);
}

void OutputFile::commit() {
    std::error_code error;
    std::filesystem::rename(temporary_, destination_, error);
    if (error) {
        throw Error(STL2STP_ERROR_IO, "Cannot publish output: " + error.message());
    }
}

namespace {

uint32_t decode_u32(const unsigned char* bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
           (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

double decode_float(const unsigned char* bytes) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                  "STL requires IEEE-754 binary32");
    const uint32_t bits = decode_u32(bytes);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void write_u32(std::ostream& stream, uint32_t value) {
    const unsigned char bytes[] = {static_cast<unsigned char>(value),
                                   static_cast<unsigned char>(value >> 8),
                                   static_cast<unsigned char>(value >> 16),
                                   static_cast<unsigned char>(value >> 24)};
    stream.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}

void write_float(std::ostream& stream, double value) {
    const float converted = static_cast<float>(value);
    if (!std::isfinite(converted)) {
        throw Error(STL2STP_ERROR_FORMAT, "Coordinate exceeds binary STL float32 range");
    }
    uint32_t bits;
    std::memcpy(&bits, &converted, sizeof(bits));
    write_u32(stream, bits);
}

struct MeshBuilder {
    Mesh mesh;
    std::map<std::tuple<double, double, double>, std::size_t> lookup;
    bool skipped = false;
    bool has_original_triangle = false;

    void add(const std::array<Point, 3>& points) {
        for (const Point& point : points) {
            if (!std::isfinite(point.coordinate_x) || !std::isfinite(point.coordinate_y) || !std::isfinite(point.coordinate_z)) {
                throw Error(STL2STP_ERROR_FORMAT, "STL contains non-finite coordinates");
            }
        }
        Triangle triangle{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const Point& point = points[corner];
            auto insertion = lookup.emplace(std::make_tuple(point.coordinate_x, point.coordinate_y, point.coordinate_z), mesh.points.size());
            if (insertion.second) {
                mesh.points.push_back(point);
            }
            triangle[corner] = insertion.first->second;
        }
        if (triangle[0] == triangle[1] || triangle[1] == triangle[2] || triangle[0] == triangle[2]) {
            if (mesh.original_triangles.empty()) { mesh.original_triangles = mesh.triangles; }
            mesh.original_triangles.push_back(triangle);
            skipped = true;
            return;
        }
        has_original_triangle = true;
        const Point normal = cross(subtract(points[1], points[0]), subtract(points[2], points[0]));
        if (!(dot(normal, normal) > 0) || !std::isfinite(dot(normal, normal))) {
            if (mesh.original_triangles.empty()) { mesh.original_triangles = mesh.triangles; }
            mesh.original_triangles.push_back(triangle);
            skipped = true;
            return;
        }
        if (!mesh.original_triangles.empty()) { mesh.original_triangles.push_back(triangle); }
        mesh.triangles.push_back(triangle);
    }
};

std::string lowercase(std::string text) {
    for (char& character : text) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return text;
}

bool keyword(std::istream& stream, const char* expected) {
    std::string word;
    return (stream >> word) && lowercase(word) == expected;
}

bool read_point(std::istream& stream, Point& point) {
    return bool(stream >> point.coordinate_x >> point.coordinate_y >> point.coordinate_z);
}

bool binary_format(std::istream& stream, uint64_t size, uint32_t& declared) {
    std::array<unsigned char, 1024> prefix{};
    stream.read(reinterpret_cast<char*>(prefix.data()),
                static_cast<std::streamsize>(std::min<uint64_t>(prefix.size(), size)));
    stream.clear();
    stream.seekg(0);
    if (size < 84) {
        return false;
    }
    declared = decode_u32(prefix.data() + 80);
    if (size == 84 + uint64_t(declared) * 50) {
        return true;
    }
    const std::string leading(reinterpret_cast<const char*>(prefix.data()), 5);
    if (lowercase(leading) != "solid") {
        return true;
    }
    for (std::size_t index = 0; index < std::min<uint64_t>(size, prefix.size()); ++index) {
        if (prefix[index] == 0 || prefix[index] > 126 ||
            (prefix[index] < 32 && prefix[index] != '\n' && prefix[index] != '\r' && prefix[index] != '\t')) {
            return true;
        }
    }
    return false;
}

bool load_binary(std::istream& stream, uint64_t size, uint32_t declared,
                 MeshBuilder& builder, Progress& progress) {
    const uint64_t available = (size - 84) / 50;
    const uint64_t count = std::min<uint64_t>(available, declared);
    stream.seekg(84);
    for (uint64_t index = 0; index < count; ++index) {
        std::array<unsigned char, 50> record{};
        if (!stream.read(reinterpret_cast<char*>(record.data()), record.size())) {
            return true;
        }
        std::array<Point, 3> points{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const unsigned char* coordinates = record.data() + 12 + corner * 12;
            points[corner] = {decode_float(coordinates), decode_float(coordinates + 4),
                              decode_float(coordinates + 8)};
        }
        builder.add(points);
        if (index % 256 == 0) {
            progress.report(0.99 * double(index + 1) / double(count));
        }
    }
    return declared != available || size != 84 + uint64_t(declared) * 50;
}

bool load_ascii(std::istream& stream, uint64_t size, MeshBuilder& builder, Progress& progress) {
    stream.imbue(std::locale::classic());
    if (!keyword(stream, "solid")) {
        throw Error(STL2STP_ERROR_FORMAT, "Expected ASCII STL solid header");
    }
    std::string line;
    std::getline(stream, line);
    bool closed = false;
    std::string token;
    std::size_t count = 0;
    while (stream >> token) {
        token = lowercase(token);
        if (token == "endsolid") {
            closed = true;
            std::getline(stream, line);
            if (!(stream >> token)) {
                break;
            }
            if (lowercase(token) != "solid") {
                return true;
            }
            std::getline(stream, line);
            closed = false;
            continue;
        }
        std::array<Point, 3> points{};
        Point ignored_normal;
        if (token != "facet" || !keyword(stream, "normal") || !read_point(stream, ignored_normal) ||
            !keyword(stream, "outer") || !keyword(stream, "loop")) {
            return true;
        }
        for (Point& point : points) {
            if (!keyword(stream, "vertex") || !read_point(stream, point)) {
                return true;
            }
        }
        if (!keyword(stream, "endloop") || !keyword(stream, "endfacet")) {
            return true;
        }
        builder.add(points);
        if (++count % 256 == 0) {
            const auto position = stream.tellg();
            if (position >= 0) {
                progress.report(0.99 * double(position) / double(size));
            } else {
                progress.check();
            }
        }
    }
    return !closed;
}

}

}

extern "C" {

stl2stp_handle* stl2stp_create(void) {
    try {
        stl2stp::thread_error.clear();
        return new stl2stp_handle;
    } catch (...) {
        try { stl2stp::thread_error = "Cannot allocate handle"; } catch (...) {}
        return nullptr;
    }
}

void stl2stp_destroy(stl2stp_handle* handle) { delete handle; }

const char* stl2stp_last_error(const stl2stp_handle* handle) {
    return handle ? handle->error.c_str() : stl2stp::thread_error.c_str();
}

stl2stp_export_options stl2stp_default_export_options(void) {
    return {1e-6, 0, STL2STP_MODE_ORIGINAL, nullptr, nullptr, nullptr, nullptr};
}

stl2stp_status stl2stp_load_stl(stl2stp_handle* handle, const char* path,
                               const stl2stp_callbacks* callbacks) {
    return stl2stp::protect(handle, [&] {
        stl2stp::require_path(path);
        stl2stp::Progress progress(callbacks);
        progress.report(0);
        std::ifstream stream(std::filesystem::u8path(path), std::ios::binary);
        if (!stream) {
            throw stl2stp::Error(STL2STP_ERROR_IO, "Cannot open STL input");
        }
        stream.seekg(0, std::ios::end);
        const auto length = stream.tellg();
        if (length < 0) {
            throw stl2stp::Error(STL2STP_ERROR_IO, "Cannot determine STL length");
        }
        const uint64_t size = static_cast<uint64_t>(length);
        if (!size) {
            throw stl2stp::Error(STL2STP_ERROR_FORMAT, "Empty STL file");
        }
        stream.seekg(0);
        uint32_t declared = 0;
        stl2stp::MeshBuilder builder;
        const bool partial = stl2stp::binary_format(stream, size, declared)
            ? stl2stp::load_binary(stream, size, declared, builder, progress)
            : stl2stp::load_ascii(stream, size, builder, progress);
        if (stream.bad()) {
            throw stl2stp::Error(STL2STP_ERROR_IO, "Failed reading STL input");
        }
        if (builder.mesh.triangles.empty() && !builder.has_original_triangle) {
            throw stl2stp::Error(STL2STP_ERROR_FORMAT, "No complete non-degenerate STL triangles");
        }
        if (partial || builder.skipped) {
            handle->error = "Loaded complete triangles only: truncated/malformed data or degenerate facets";
        }
        progress.report(1);
        builder.mesh.original_weld_required = true;
        handle->mesh = std::move(builder.mesh);
        return partial || builder.skipped ? STL2STP_WARNING_PARTIAL : STL2STP_OK;
    });
}

stl2stp_status stl2stp_get_mesh_stats(stl2stp_handle* handle, stl2stp_mesh_stats* stats) {
    return stl2stp::protect(handle, [&] {
        if (!stats) {
            throw stl2stp::Error(STL2STP_ERROR_ARGUMENT, "Null mesh statistics output");
        }
        stl2stp::require_mesh(handle->mesh);
        stl2stp_mesh_stats result{};
        result.vertices = handle->mesh.points.size();
        result.faces = handle->mesh.triangles.size();
        std::set<std::pair<std::size_t, std::size_t>> edges;
        for (const auto& triangle : handle->mesh.triangles) {
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto first = triangle[corner];
                const auto second = triangle[(corner + 1) % 3];
                edges.emplace(std::min(first, second), std::max(first, second));
            }
        }
        result.edges = edges.size();
        std::fill(std::begin(result.minimum), std::end(result.minimum), std::numeric_limits<double>::infinity());
        std::fill(std::begin(result.maximum), std::end(result.maximum), -std::numeric_limits<double>::infinity());
        for (const auto& point : handle->mesh.points) {
            const double coordinates[] = {point.coordinate_x, point.coordinate_y, point.coordinate_z};
            for (std::size_t axis = 0; axis < 3; ++axis) {
                result.minimum[axis] = std::min(result.minimum[axis], coordinates[axis]);
                result.maximum[axis] = std::max(result.maximum[axis], coordinates[axis]);
            }
        }
        *stats = result;
        return STL2STP_OK;
    });
}

stl2stp_status stl2stp_simplify(stl2stp_handle* handle, uint64_t target_faces,
                               const stl2stp_callbacks* callbacks) {
    return stl2stp::protect(handle, [&] {
        stl2stp::require_mesh(handle->mesh);
        if (!target_faces || target_faces > std::numeric_limits<std::size_t>::max()) {
            throw stl2stp::Error(STL2STP_ERROR_ARGUMENT, "Target face count must be positive and addressable");
        }
        if (handle->mesh.triangles.empty()) {
            throw stl2stp::Error(STL2STP_ERROR_FORMAT, "No non-collinear triangles for simplification");
        }
        stl2stp::Progress progress(callbacks);
        progress.report(0);
        if (target_faces >= handle->mesh.triangles.size()) {
            progress.report(1);
            return STL2STP_OK;
        }
        auto result = stl2stp::simplify_mesh(handle->mesh, static_cast<std::size_t>(target_faces), progress);
        const bool reached = result.triangles.size() <= target_faces;
        if (!reached) {
            handle->error = "QEM stopped before target: remaining collapses would flip faces or candidates exhausted";
        }
        progress.report(1);
        handle->mesh = std::move(result);
        return reached ? STL2STP_OK : STL2STP_WARNING_TARGET_NOT_REACHED;
    });
}

stl2stp_status stl2stp_save_stl(stl2stp_handle* handle, const char* path,
                               const stl2stp_callbacks* callbacks) {
    return stl2stp::protect(handle, [&] {
        stl2stp::require_mesh(handle->mesh);
        if (handle->mesh.triangles.empty()) {
            throw stl2stp::Error(STL2STP_ERROR_FORMAT, "No non-collinear triangles for STL output");
        }
        if (handle->mesh.triangles.size() > std::numeric_limits<uint32_t>::max()) {
            throw stl2stp::Error(STL2STP_ERROR_FORMAT, "Too many faces for binary STL uint32 count");
        }
        stl2stp::Progress progress(callbacks);
        progress.report(0);
        stl2stp::OutputFile output(path);
        std::ofstream stream(output.temporary(), std::ios::binary);
        if (!stream) {
            throw stl2stp::Error(STL2STP_ERROR_IO, "Cannot open STL output");
        }
        std::array<char, 80> header{};
        const char signature[] = "stl2stp-core binary STL";
        std::copy(std::begin(signature), std::end(signature), header.begin());
        stream.write(header.data(), header.size());
        stl2stp::write_u32(stream, static_cast<uint32_t>(handle->mesh.triangles.size()));
        std::size_t index = 0;
        for (const auto& triangle : handle->mesh.triangles) {
            const auto& first = handle->mesh.points[triangle[0]];
            const auto& second = handle->mesh.points[triangle[1]];
            const auto& third = handle->mesh.points[triangle[2]];
            auto normal = stl2stp::cross(stl2stp::subtract(second, first), stl2stp::subtract(third, first));
            const double length = std::sqrt(stl2stp::dot(normal, normal));
            if (length > 0) {
                normal = {normal.coordinate_x / length, normal.coordinate_y / length, normal.coordinate_z / length};
            }
            stl2stp::write_float(stream, normal.coordinate_x);
            stl2stp::write_float(stream, normal.coordinate_y);
            stl2stp::write_float(stream, normal.coordinate_z);
            for (const auto point_index : triangle) {
                const auto& point = handle->mesh.points[point_index];
                stl2stp::write_float(stream, point.coordinate_x);
                stl2stp::write_float(stream, point.coordinate_y);
                stl2stp::write_float(stream, point.coordinate_z);
            }
            stream.put(0);
            stream.put(0);
            if (++index % 256 == 0) {
                progress.report(0.99 * double(index) / double(handle->mesh.triangles.size()));
            }
        }
        stream.close();
        if (!stream) {
            throw stl2stp::Error(STL2STP_ERROR_IO, "Failed writing STL output");
        }
        progress.report(1);
        output.commit();
        return STL2STP_OK;
    });
}

}
