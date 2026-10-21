#ifndef STL2STP_INTERNAL_HPP
#define STL2STP_INTERNAL_HPP

#include <stl2stp/stl2stp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace stl2stp {

struct Point {
    double coordinate_x = 0;
    double coordinate_y = 0;
    double coordinate_z = 0;
};

inline Point subtract(const Point& first, const Point& second) {
    return {first.coordinate_x - second.coordinate_x,
            first.coordinate_y - second.coordinate_y,
            first.coordinate_z - second.coordinate_z};
}

inline Point cross(const Point& first, const Point& second) {
    return {first.coordinate_y * second.coordinate_z - first.coordinate_z * second.coordinate_y,
            first.coordinate_z * second.coordinate_x - first.coordinate_x * second.coordinate_z,
            first.coordinate_x * second.coordinate_y - first.coordinate_y * second.coordinate_x};
}

inline double dot(const Point& first, const Point& second) {
    return first.coordinate_x * second.coordinate_x + first.coordinate_y * second.coordinate_y +
           first.coordinate_z * second.coordinate_z;
}

using Triangle = std::array<std::size_t, 3>;

struct Mesh {
    std::vector<Point> points;
    std::vector<Triangle> triangles;
    std::vector<Triangle> original_triangles;
    bool original_weld_required = false;
};

struct Error : std::runtime_error {
    stl2stp_status status;
    Error(stl2stp_status code, const std::string& message)
        : std::runtime_error(message), status(code) {}
};

class Progress {
public:
    explicit Progress(const stl2stp_callbacks* callbacks)
        : callbacks_(callbacks ? *callbacks : stl2stp_callbacks{}) {}

    bool cancelled() const {
        return callbacks_.cancelled && callbacks_.cancelled(callbacks_.user_data) != 0;
    }

    void check() const {
        if (cancelled()) {
            throw Error(STL2STP_ERROR_CANCELLED, "Operation cancelled");
        }
    }

    void report(double fraction) {
        check();
        fraction = std::clamp(fraction, 0.0, 1.0);
        if (fraction > last_) {
            last_ = fraction;
            if (callbacks_.progress) {
                callbacks_.progress(fraction, callbacks_.user_data);
            }
        }
        check();
    }

private:
    stl2stp_callbacks callbacks_;
    double last_ = -1;
};

class OutputFile {
public:
    explicit OutputFile(const char* path);
    ~OutputFile();
    OutputFile(const OutputFile&) = delete;
    OutputFile& operator=(const OutputFile&) = delete;
    const std::filesystem::path& temporary() const { return temporary_; }
    void commit();

private:
    std::filesystem::path destination_;
    std::filesystem::path directory_;
    std::filesystem::path temporary_;
};

// Near-coincident vertex weld (core/weld). map[i] is the representative of point i (map[r] == r).
struct WeldResult {
    std::vector<std::size_t> map;
    std::size_t merged = 0;
    double tolerance = 0;
};
double weld_tolerance(const std::vector<Point>& points, double requested);
WeldResult weld_points(const std::vector<Point>& points, const std::vector<const std::vector<Triangle>*>& topology,
                       double tolerance, Progress* progress = nullptr);

void require_path(const char* path);
void require_mesh(const Mesh& mesh);
Mesh simplify_mesh(const Mesh& source, std::size_t target, Progress& progress);

}

struct stl2stp_handle {
    stl2stp::Mesh mesh;
    std::string error;
};

namespace stl2stp {

extern thread_local std::string thread_error;

template <typename Function>
stl2stp_status protect(stl2stp_handle* handle, Function&& function) noexcept {
    std::string& message = handle ? handle->error : thread_error;
    message.clear();
    try {
        if (!handle) {
            throw Error(STL2STP_ERROR_ARGUMENT, "Null handle");
        }
        return function();
    } catch (const Error& error) {
        try { message = error.what(); } catch (...) {}
        return error.status;
    } catch (const std::bad_alloc&) {
        try { message = "Out of memory"; } catch (...) {}
        return STL2STP_ERROR_MEMORY;
    } catch (const std::exception& error) {
        try { message = error.what(); } catch (...) {}
        return STL2STP_ERROR_INTERNAL;
    } catch (...) {
        try { message = "Unexpected failure"; } catch (...) {}
        return STL2STP_ERROR_INTERNAL;
    }
}

}

#endif
