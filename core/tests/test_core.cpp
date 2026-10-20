#include <stl2stp/stl2stp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using Handle = std::unique_ptr<stl2stp_handle, decltype(&stl2stp_destroy)>;

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

void status(stl2stp_handle* handle, stl2stp_status actual, stl2stp_status expected = STL2STP_OK) {
    require(actual == expected, "Expected status " + std::to_string(expected) + ", got " +
            std::to_string(actual) + ": " + stl2stp_last_error(handle));
}

stl2stp_mesh_stats stats(stl2stp_handle* handle) {
    stl2stp_mesh_stats result{};
    status(handle, stl2stp_get_mesh_stats(handle, &result));
    return result;
}

struct ProgressState {
    double last = -1;
    double cancel_at = 2;
    int calls = 0;
    bool monotonic = true;
};

void report(double fraction, void* user_data) {
    auto& state = *static_cast<ProgressState*>(user_data);
    state.monotonic = state.monotonic && std::isfinite(fraction) && fraction >= 0 && fraction <= 1 && fraction >= state.last;
    state.last = fraction;
    ++state.calls;
}

int cancel(void* user_data) {
    const auto& state = *static_cast<ProgressState*>(user_data);
    return state.last >= state.cancel_at;
}

stl2stp_callbacks callbacks(ProgressState& state) { return {&report, &cancel, &state}; }

void progress_complete(const ProgressState& state) {
    require(state.monotonic && state.last == 1 && state.calls >= 2, "Progress must be monotonic in [0,1] and complete");
}

void cube_stats(stl2stp_handle* handle) {
    const auto mesh = stats(handle);
    require(mesh.vertices == 8 && mesh.edges == 18 && mesh.faces == 12, "Cube V/E/F must be 8/18/12");
    for (int axis = 0; axis < 3; ++axis) {
        require(mesh.minimum[axis] == 0 && mesh.maximum[axis] == 1, "Incorrect unit cube bounds");
    }
}

std::string contents(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void sentinel(const std::filesystem::path& path) { std::ofstream(path, std::ios::binary) << "original output"; }

void no_temporary_files(const std::filesystem::path& directory) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        require(entry.path().filename().u8string().find(".stl2stp-") == std::string::npos, "Temporary output leaked");
    }
}

#if STL2STP_WITH_OCCT
void validate_iges(const std::string& text);

void topology_matches(const stl2stp_topology& actual, const stl2stp_topology& expected, const std::string& stage) {
    require(actual.solids == expected.solids && actual.shells == expected.shells && actual.faces == expected.faces &&
            actual.edges == expected.edges && actual.vertices == expected.vertices,
            stage + " topology S/Sh/F/E/V = " + std::to_string(actual.solids) + "/" + std::to_string(actual.shells) +
            "/" + std::to_string(actual.faces) + "/" + std::to_string(actual.edges) + "/" + std::to_string(actual.vertices));
}

void mode_conversion(stl2stp_handle* handle, const std::filesystem::path& data,
                     const std::filesystem::path& output, bool original, const std::string& fixture) {
    auto options = stl2stp_default_export_options();
    options.mode = original ? STL2STP_MODE_ORIGINAL : STL2STP_MODE_SEWING;
    const bool partial = fixture == "degenerate_duplicate" || fixture == "duplicate_open" || fixture == "collinear" ||
        fixture == "collinear_only" || fixture == "truncated";
    const std::string input = fixture == "two_shells" ? "two-shells.stl" :
        fixture == "degenerate_duplicate" ? "degenerate-duplicate.stl" :
        fixture == "duplicate_open" ? "zero-area-duplicate.stl" :
        fixture == "collinear_only" ? "collinear-only.stl" :
        fixture == "collinear" ? "collinear.stl" : fixture == "truncated" ? "truncated-binary.stl" : "cube-binary.stl";
    status(handle, stl2stp_load_stl(handle, (data / input).u8string().c_str(), nullptr),
           partial ? STL2STP_WARNING_PARTIAL : STL2STP_OK);
    if (fixture == "collinear_only") {
        require(stats(handle).faces == 0, "Clean mesh should exclude collinear-only facet");
        status(handle, stl2stp_simplify(handle, 1, nullptr), STL2STP_ERROR_FORMAT);
        status(handle, stl2stp_save_stl(handle, (output / "collinear.stl").u8string().c_str(), nullptr), STL2STP_ERROR_FORMAT);
        if (!original) {
            const auto path = output / "rejected.step";
            sentinel(path);
            stl2stp_topology untouched{123, 0, 0, 0, 0};
            status(handle, stl2stp_export_step(handle, path.u8string().c_str(), &options, nullptr, &untouched), STL2STP_ERROR_FORMAT);
            status(handle, stl2stp_export_iges(handle, path.u8string().c_str(), &options, nullptr, &untouched), STL2STP_ERROR_FORMAT);
            require(contents(path) == "original output" && untouched.solids == 123, "Empty sewing input changed output");
            no_temporary_files(output);
            return;
        }
    }
    for (const int merge : {0, 1}) {
        if (merge && fixture != "cube" && fixture != "two_shells") { break; }
        options.merge_coplanar = merge;
        stl2stp_topology expected{1, 1, merge ? 6u : 12u, merge ? 12u : 18u, 8};
        if (fixture == "two_shells") { expected = {original ? 1u : 2u, original ? 1u : 2u, merge ? 12u : 24u, merge ? 24u : 36u, 16}; }
        if (fixture == "degenerate_duplicate") { expected = {1, 1, 13, original ? 18u : 21u, original ? 8u : 11u}; }
        if (fixture == "duplicate_open") { expected = {1, 1, 2, 3, 3}; }
        if (fixture == "collinear" && original) { expected = {1, 1, 13, 20, 9}; }
        if (fixture == "collinear_only") { expected = {1, 1, 1, 3, 3}; }
        if (fixture == "truncated") { expected = {original ? 1u : 0u, original ? 1u : 2u, 5, 12, original ? 8u : 9u}; }
        const auto path = output / (fixture == "iges" ? "model.igs" : merge ? "merged.step" : "model.step");
        stl2stp_topology exported{};
        ProgressState progress;
        auto observer = callbacks(progress);
        if (fixture == "iges") {
            status(handle, stl2stp_export_iges(handle, path.u8string().c_str(), &options, &observer, &exported));
            const auto text = contents(path);
            validate_iges(text);
            require(text.find("model.igs") != std::string::npos, "IGES header used temporary filename");
            require(text.find("VINCENT") == std::string::npos && text.find("STL2STP") == std::string::npos,
                    "Default IGES metadata must be empty");
        } else {
            status(handle, stl2stp_export_step(handle, path.u8string().c_str(), &options, &observer, &exported));
            const auto text = contents(path);
            if (original) {
                for (const auto field : {"STL2STP", "VINCENT", "STL TO STEP FILE"}) {
                    require(text.find(field) == std::string::npos, std::string("Default STEP metadata leaked: ") + field);
                }
                require(text.find(path.filename().u8string()) != std::string::npos, "Header used temporary filename");
                const auto shell = text.find("CLOSED_SHELL(");
                require(shell != std::string::npos && text.find("CLOSED_SHELL(", shell + 1) == std::string::npos,
                        "Original writer must receive exactly one shell, including open/disconnected input");
            }
            stl2stp_topology expected_read = expected;
            if (fixture == "two_shells") { expected_read.solids = 2; expected_read.shells = 2; }
            if (fixture == "degenerate_duplicate" || (fixture == "collinear" && original)) { expected_read.shells = 2; }
            if (fixture == "truncated") { expected_read.solids = 0; expected_read.shells = 2; }
            if (fixture == "collinear_only") { expected_read = {0, 1, 1, 2, 2}; }
            stl2stp_topology read{};
            status(handle, stl2stp_read_step(handle, path.u8string().c_str(), nullptr, &read));
            topology_matches(read, expected_read, "readback");
        }
        progress_complete(progress);
        topology_matches(exported, expected, "export");
    }
}

void validate_iges(const std::string& text) {
    const std::string sections = "SGDPT";
    std::array<uint64_t, 5> counts{};
    std::istringstream stream(text);
    std::string record;
    std::string termination;
    std::size_t previous_section = 0;
    auto number = [](const std::string& field) {
        require(field.find_first_not_of(" 0123456789") == std::string::npos, "Invalid IGES numeric field");
        return std::stoull(field);
    };
    while (std::getline(stream, record)) {
        if (!record.empty() && record.back() == '\r') { record.pop_back(); }
        require(record.size() == 80, "IGES records must contain 80 columns");
        const auto section = sections.find(record[72]);
        require(section != std::string::npos && section >= previous_section, "Invalid IGES section order");
        previous_section = section;
        require(number(record.substr(73, 7)) == ++counts[section], "IGES record sequence incorrect");
        if (section == 4) { termination = record; }
    }
    require(counts[0] > 0 && counts[1] > 0 && counts[2] > 0 && counts[3] > 0 && counts[4] == 1,
            "IGES must contain S/G/D/P sections and one termination record");
    for (std::size_t section = 0; section < 4; ++section) {
        require(termination[section * 8] == sections[section] &&
                    number(termination.substr(section * 8 + 1, 7)) == counts[section],
                "IGES termination counts do not match records");
    }
}

void topology_cube(const stl2stp_topology& counts, bool merged) {
    require(counts.faces == (merged ? 6u : 12u), "Cube B-Rep face count incorrect");
    require(counts.shells == 1 && counts.solids == 1, "Closed cube must have exactly one shell and one solid");
    require(counts.vertices == 8 && counts.edges == (merged ? 12u : 18u), "Cube unique B-Rep V/E incorrect");
}
#endif

}

int main(int argc, char** argv) {
    try {
        require(argc == 4, "Usage: test-core <mode> <fixture-dir> <output-dir>");
        const std::string mode = argv[1];
        const auto data = std::filesystem::u8path(argv[2]);
        const auto output = std::filesystem::u8path(argv[3]);
        std::filesystem::create_directories(output);
        Handle handle(stl2stp_create(), &stl2stp_destroy);
        require(bool(handle), "Handle allocation failed");
        auto load = [&](const char* name, stl2stp_status expected = STL2STP_OK) {
            const auto path = (data / name).u8string();
            status(handle.get(), stl2stp_load_stl(handle.get(), path.c_str(), nullptr), expected);
        };

        if (mode == "load_binary" || mode == "load_ascii" || mode == "solid_header") {
            ProgressState progress;
            auto observer = callbacks(progress);
            const auto path = (data / (mode == "load_ascii" ? "cube-ascii.stl" :
                                      mode == "solid_header" ? "cube-solid-header.stl" : "cube-binary.stl")).u8string();
            status(handle.get(), stl2stp_load_stl(handle.get(), path.c_str(), &observer));
            progress_complete(progress);
            cube_stats(handle.get());
        } else if (mode == "truncated") {
            for (const auto name : {"truncated-binary.stl", "truncated-ascii.stl"}) {
                load(name, STL2STP_WARNING_PARTIAL);
                require(*stl2stp_last_error(handle.get()), "Truncation must produce a warning message");
                require(stats(handle.get()).faces == 5, "Truncation must retain five complete triangles");
            }
            load("degenerate.stl", STL2STP_WARNING_PARTIAL);
            cube_stats(handle.get());
        } else if (mode == "stats") {
            load("two-shells.stl");
            const auto mesh = stats(handle.get());
            require(mesh.vertices == 16 && mesh.edges == 36 && mesh.faces == 24, "Two-cube mesh statistics incorrect");
            require(mesh.minimum[0] == 0 && mesh.maximum[0] == 4, "Two-cube bounds incorrect");
            load("sphere.stl");
            const auto sphere = stats(handle.get());
            require(sphere.faces == 19800 && sphere.vertices == 9902 && sphere.edges == 29700, "UV sphere V/E/F incorrect");
        } else if (mode == "errors") {
            stl2stp_mesh_stats mesh{};
            status(handle.get(), stl2stp_get_mesh_stats(handle.get(), &mesh), STL2STP_ERROR_EMPTY);
            status(nullptr, stl2stp_get_mesh_stats(nullptr, &mesh), STL2STP_ERROR_ARGUMENT);
            require(*stl2stp_last_error(nullptr), "Null handle error missing");
            status(handle.get(), stl2stp_load_stl(handle.get(), nullptr, nullptr), STL2STP_ERROR_ARGUMENT);
            load("cube-binary.stl");
            for (const auto name : {"invalid.stl", "nonfinite.stl", "empty-binary.stl"}) {
                load(name, STL2STP_ERROR_FORMAT);
                cube_stats(handle.get());
            }
            load("does-not-exist.stl", STL2STP_ERROR_IO);
            cube_stats(handle.get());
            status(handle.get(), stl2stp_get_mesh_stats(handle.get(), nullptr), STL2STP_ERROR_ARGUMENT);
            status(handle.get(), stl2stp_simplify(handle.get(), 0, nullptr), STL2STP_ERROR_ARGUMENT);
            const auto missing = (output / "missing" / "file.stl").u8string();
            status(handle.get(), stl2stp_save_stl(handle.get(), missing.c_str(), nullptr), STL2STP_ERROR_IO);
            load("huge-ascii.stl");
            status(handle.get(), stl2stp_simplify(handle.get(), 1, nullptr), STL2STP_ERROR_FORMAT);
            require(stats(handle.get()).faces == 2, "Failed QEM scale check changed mesh");
            const auto huge_output = output / "huge.stl";
            sentinel(huge_output);
            status(handle.get(), stl2stp_save_stl(handle.get(), huge_output.u8string().c_str(), nullptr), STL2STP_ERROR_FORMAT);
            require(contents(huge_output) == "original output", "Float32 overflow replaced STL destination");
            no_temporary_files(output);
        } else if (mode == "simplify") {
            load("sphere.stl");
            ProgressState progress;
            auto observer = callbacks(progress);
            status(handle.get(), stl2stp_simplify(handle.get(), 2000, &observer));
            progress_complete(progress);
            const auto simplified = stats(handle.get());
            require(simplified.faces <= 2000 && simplified.faces >= 1998, "Sphere simplification target not reached");
            require(simplified.vertices + simplified.faces == simplified.edges + 2, "Simplified sphere must remain closed Euler-2 topology");
            load("cube-binary.stl");
            status(handle.get(), stl2stp_simplify(handle.get(), 12, nullptr));
            cube_stats(handle.get());
            status(handle.get(), stl2stp_simplify(handle.get(), 6, nullptr));
            require(stats(handle.get()).faces <= 6, "Cube simplification target not reached");
            const auto best_effort = stl2stp_simplify(handle.get(), 1, nullptr);
            status(handle.get(), best_effort, STL2STP_WARNING_TARGET_NOT_REACHED);
            require(*stl2stp_last_error(handle.get()), "Unreachable target must provide a warning");
            require(stats(handle.get()).faces > 0, "QEM must not delete the entire mesh");
        } else if (mode == "save") {
            load("cube-binary.stl");
            const auto path = (output / "saved.stl").u8string();
            ProgressState progress;
            auto observer = callbacks(progress);
            status(handle.get(), stl2stp_save_stl(handle.get(), path.c_str(), &observer));
            progress_complete(progress);
            require(std::filesystem::file_size(std::filesystem::u8path(path)) == 84 + 12 * 50, "Binary STL size incorrect");
            status(handle.get(), stl2stp_load_stl(handle.get(), path.c_str(), nullptr));
            cube_stats(handle.get());
            status(handle.get(), stl2stp_save_stl(handle.get(), path.c_str(), nullptr));
            no_temporary_files(output);
#if !STL2STP_WITH_OCCT
        } else if (mode == "disabled_exchange") {
            load("cube-binary.stl");
            const auto path = (output / "unavailable.step").u8string();
            stl2stp_topology counts{123, 0, 0, 0, 0};
            status(handle.get(), stl2stp_export_step(handle.get(), path.c_str(), nullptr, nullptr, &counts), STL2STP_ERROR_OCCT);
            status(handle.get(), stl2stp_export_iges(handle.get(), path.c_str(), nullptr, nullptr, &counts), STL2STP_ERROR_OCCT);
            status(handle.get(), stl2stp_read_step(handle.get(), path.c_str(), nullptr, &counts), STL2STP_ERROR_OCCT);
            require(counts.solids == 123 && !std::filesystem::exists(std::filesystem::u8path(path)),
                    "Disabled exchange must not create output or change topology");
#endif
        } else if (mode == "unicode") {
            const auto source = output / std::filesystem::u8path("立方体输入.stl");
            const auto destination = output / std::filesystem::u8path("简化输出.stl");
            std::filesystem::copy_file(data / "cube-ascii.stl", source, std::filesystem::copy_options::overwrite_existing);
            status(handle.get(), stl2stp_load_stl(handle.get(), source.u8string().c_str(), nullptr));
            status(handle.get(), stl2stp_save_stl(handle.get(), destination.u8string().c_str(), nullptr));
            status(handle.get(), stl2stp_load_stl(handle.get(), destination.u8string().c_str(), nullptr));
            cube_stats(handle.get());
        } else if (mode == "cancel_mesh") {
            load("cube-binary.stl");
            for (const auto name : {"sphere.stl", "sphere-ascii.stl"}) {
                ProgressState progress;
                progress.cancel_at = 0.3;
                auto observer = callbacks(progress);
                const auto path = (data / name).u8string();
                status(handle.get(), stl2stp_load_stl(handle.get(), path.c_str(), &observer), STL2STP_ERROR_CANCELLED);
                require(progress.monotonic && progress.last < 1, "Load cancellation must occur mid-operation");
                cube_stats(handle.get());
            }
            load("sphere.stl");
            ProgressState progress;
            progress.cancel_at = 0.4;
            auto observer = callbacks(progress);
            status(handle.get(), stl2stp_simplify(handle.get(), 2000, &observer), STL2STP_ERROR_CANCELLED);
            require(stats(handle.get()).faces == 19800, "Cancelled simplification modified source");
            const auto path = output / "cancelled.stl";
            sentinel(path);
            progress = {};
            progress.cancel_at = 0.5;
            status(handle.get(), stl2stp_save_stl(handle.get(), path.u8string().c_str(), &observer), STL2STP_ERROR_CANCELLED);
            require(contents(path) == "original output", "Cancelled save overwrote destination");
            load("cube-binary.stl");
            const stl2stp_callbacks immediate{nullptr, [](void*) { return 1; }, nullptr};
            status(handle.get(), stl2stp_load_stl(handle.get(), (data / "sphere.stl").u8string().c_str(), &immediate), STL2STP_ERROR_CANCELLED);
            cube_stats(handle.get());
            progress = {};
            progress.cancel_at = 1;
            status(handle.get(), stl2stp_load_stl(handle.get(), (data / "sphere.stl").u8string().c_str(), &observer), STL2STP_ERROR_CANCELLED);
            cube_stats(handle.get());
            no_temporary_files(output);
#if STL2STP_WITH_OCCT
        } else if (mode.rfind("original_", 0) == 0 || mode.rfind("sewing_", 0) == 0) {
            const bool original = mode.rfind("original_", 0) == 0;
            mode_conversion(handle.get(), data, output, original, mode.substr(original ? 9 : 7));
        } else if (mode == "step" || mode == "step_merge" || mode == "readback") {
            load("cube-binary.stl");
            auto options = stl2stp_default_export_options();
            options.merge_coplanar = mode == "step_merge";
            const auto path = (output / std::filesystem::u8path("立方体.step")).u8string();
            stl2stp_topology exported{};
            ProgressState progress;
            auto observer = callbacks(progress);
            status(handle.get(), stl2stp_export_step(handle.get(), path.c_str(), &options, &observer, &exported));
            progress_complete(progress);
            topology_cube(exported, options.merge_coplanar != 0);
            const auto text = contents(std::filesystem::u8path(path));
            require(text.find("AUTOMOTIVE_DESIGN") != std::string::npos, "STEP must use AP214 schema");
            stl2stp_topology read{};
            progress = {};
            Handle empty_reader(mode == "readback" ? stl2stp_create() : nullptr, &stl2stp_destroy);
            require(mode != "readback" || bool(empty_reader), "Read-back handle allocation failed");
            auto* reader = empty_reader ? empty_reader.get() : handle.get();
            status(reader, stl2stp_read_step(reader, path.c_str(), &observer, &read));
            progress_complete(progress);
            topology_cube(read, options.merge_coplanar != 0);
            require(read.solids == exported.solids && read.shells == exported.shells, "STEP read-back differs from export");
            if (empty_reader) {
                stl2stp_mesh_stats untouched{};
                status(reader, stl2stp_get_mesh_stats(reader, &untouched), STL2STP_ERROR_EMPTY);
            }
            cube_stats(handle.get());
        } else if (mode == "two_shells") {
            load("two-shells.stl");
            const auto path = (output / "two.step").u8string();
            stl2stp_topology exported{}, read{};
            auto options = stl2stp_default_export_options();
            options.mode = STL2STP_MODE_SEWING;
            for (const int merge : {0, 1}) {
                options.merge_coplanar = merge;
                status(handle.get(), stl2stp_export_step(handle.get(), path.c_str(), &options, nullptr, &exported));
                require(exported.shells == 2 && exported.solids == 2 && exported.faces == (merge ? 12u : 24u),
                        "Two disjoint cubes must have two components");
                status(handle.get(), stl2stp_read_step(handle.get(), path.c_str(), nullptr, &read));
                require(read.shells == 2 && read.solids == 2 && read.faces == exported.faces &&
                            read.vertices == 16 && read.edges == (merge ? 24u : 36u),
                        "Two-shell STEP roundtrip lost topology");
            }
        } else if (mode == "iges") {
            load("cube-binary.stl");
            const auto path = output / "cube.igs";
            stl2stp_topology counts{};
            ProgressState progress;
            auto observer = callbacks(progress);
            status(handle.get(), stl2stp_export_iges(handle.get(), path.u8string().c_str(), nullptr, &observer, &counts));
            progress_complete(progress);
            topology_cube(counts, false);
            validate_iges(contents(path));
        } else if (mode == "exchange_errors") {
            load("cube-binary.stl");
            auto options = stl2stp_default_export_options();
            options.sewing_tolerance = -1;
            const auto path = (output / "invalid.step").u8string();
            status(handle.get(), stl2stp_export_step(handle.get(), path.c_str(), &options, nullptr, nullptr), STL2STP_ERROR_ARGUMENT);
            require(!std::filesystem::exists(std::filesystem::u8path(path)), "Invalid export created output");
            options = stl2stp_default_export_options();
            options.mode = static_cast<stl2stp_mode>(99);
            status(handle.get(), stl2stp_export_step(handle.get(), path.c_str(), &options, nullptr, nullptr), STL2STP_ERROR_ARGUMENT);
            require(!std::filesystem::exists(std::filesystem::u8path(path)), "Invalid mode created output");
            stl2stp_topology counts{};
            status(handle.get(), stl2stp_read_step(handle.get(), (data / "missing.step").u8string().c_str(), nullptr, &counts), STL2STP_ERROR_IO);
            status(handle.get(), stl2stp_read_step(handle.get(), path.c_str(), nullptr, nullptr), STL2STP_ERROR_ARGUMENT);
            status(handle.get(), stl2stp_read_step(handle.get(), (data / "invalid.stl").u8string().c_str(), nullptr, &counts), STL2STP_ERROR_FORMAT);
        } else if (mode == "cancel_exchange") {
            load("cube-binary.stl");
            const auto path = output / "cancel.step";
            for (const auto conversion_mode : {STL2STP_MODE_ORIGINAL, STL2STP_MODE_SEWING}) {
                auto options = stl2stp_default_export_options();
                options.mode = conversion_mode;
                for (const double threshold : {0.0, 0.45, 0.8, 1.0}) {
                    sentinel(path);
                    ProgressState progress;
                    progress.cancel_at = threshold;
                    auto observer = callbacks(progress);
                    stl2stp_topology counts{123, 0, 0, 0, 0};
                    status(handle.get(), stl2stp_export_step(handle.get(), path.u8string().c_str(), &options, &observer, &counts), STL2STP_ERROR_CANCELLED);
                    require(contents(path) == "original output" && counts.solids == 123, "Cancelled STEP export changed output");
                    require(progress.monotonic, "Cancelled OCCT progress not monotonic");
                    progress = {};
                    progress.cancel_at = threshold;
                    status(handle.get(), stl2stp_export_iges(handle.get(), path.u8string().c_str(), &options, &observer, &counts), STL2STP_ERROR_CANCELLED);
                    require(contents(path) == "original output" && counts.solids == 123, "Cancelled IGES export changed output");
                    require(progress.monotonic, "Cancelled IGES progress not monotonic");
                    no_temporary_files(output);
                }
            }
            status(handle.get(), stl2stp_export_step(handle.get(), path.u8string().c_str(), nullptr, nullptr, nullptr));
            ProgressState progress;
            progress.cancel_at = 0.5;
            auto observer = callbacks(progress);
            stl2stp_topology counts{123, 0, 0, 0, 0};
            status(handle.get(), stl2stp_read_step(handle.get(), path.u8string().c_str(), &observer, &counts), STL2STP_ERROR_CANCELLED);
            require(counts.solids == 123, "Cancelled STEP read modified output counts");
            cube_stats(handle.get());
            no_temporary_files(output);
#endif
        } else {
            throw std::runtime_error("Unknown test mode: " + mode);
        }
        std::cout << "PASS " << mode << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
