#include <stl2stp/stl2stp.h>

#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

volatile std::sig_atomic_t interrupted = 0;

void interrupt_handler(int) { interrupted = 1; }
int cancelled(void*) { return interrupted != 0; }

using Handle = std::unique_ptr<stl2stp_handle, decltype(&stl2stp_destroy)>;

uint64_t face_count(const std::string& value) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        throw std::runtime_error("Face count must be a positive integer");
    }
    const auto count = std::stoull(value);
    if (!count) { throw std::runtime_error("Face count must be positive"); }
    return count;
}

void check(stl2stp_handle* handle, stl2stp_status status) {
    if (status < 0) { throw std::runtime_error(stl2stp_last_error(handle)); }
    if (status > 0) { std::cerr << "warning: " << stl2stp_last_error(handle) << '\n'; }
}

template <typename Function>
void timed(const char* name, Function&& function) {
    const auto started = std::chrono::steady_clock::now();
    function();
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started);
    std::cout << name << "_ms=" << elapsed.count() << '\n';
}

void print_topology(const stl2stp_topology& counts, const char* prefix) {
    std::cout << prefix << " solids=" << counts.solids << " shells=" << counts.shells
              << " faces=" << counts.faces << " edges=" << counts.edges << " vertices=" << counts.vertices << '\n';
}

void print_mesh(stl2stp_handle* handle) {
    stl2stp_mesh_stats stats{};
    check(handle, stl2stp_get_mesh_stats(handle, &stats));
    std::cout << "mesh vertices=" << stats.vertices << " edges=" << stats.edges << " faces=" << stats.faces << '\n';
    std::cout << "bbox min=" << stats.minimum[0] << ',' << stats.minimum[1] << ',' << stats.minimum[2]
              << " max=" << stats.maximum[0] << ',' << stats.maximum[1] << ',' << stats.maximum[2] << '\n';
}

std::string lowercase(std::string text) {
    for (char& character : text) {
        if (character >= 'A' && character <= 'Z') { character = static_cast<char>(character - 'A' + 'a'); }
    }
    return text;
}

void usage() {
    std::cerr << "Usage:\n"
              << "  stl2stp-cli info <in.stl>\n"
              << "  stl2stp-cli simplify <in.stl> <out.stl> --faces N\n"
              << "  stl2stp-cli convert <in.stl> <out.step|out.igs> [--mode original|sewing] [--merge-coplanar] [--simplify N]\n"
              << "      [--merge-angle RADIANS] [--strict-original]\n"
              << "      [--author TEXT] [--org TEXT] [--product TEXT] [--description TEXT] [--timestamp ISO8601]\n"
              << "  stl2stp-cli readback <in.step>\n";
}

}

int main(int argc, char** argv) {
    try {
        std::signal(SIGINT, interrupt_handler);
        if (argc < 3) { usage(); return 2; }
        const std::string command = argv[1];
        Handle handle(stl2stp_create(), &stl2stp_destroy);
        if (!handle) { throw std::runtime_error(stl2stp_last_error(nullptr)); }
        const stl2stp_callbacks callbacks{nullptr, &cancelled, nullptr};
        if (command == "readback") {
            if (argc != 3) { usage(); return 2; }
            stl2stp_topology counts{};
            timed("readback", [&] { check(handle.get(), stl2stp_read_step(handle.get(), argv[2], &callbacks, &counts)); });
            print_topology(counts, "readback");
            return 0;
        }
        if (command != "info" && command != "simplify" && command != "convert") { usage(); return 2; }
        uint64_t target = 0;
        auto options = stl2stp_default_export_options();
        bool merge_given = false;
        bool mode_given = false;
        bool strict_given = false;
        bool angle_given = false;
        if (command == "info") {
            if (argc != 3) { usage(); return 2; }
        } else {
            if (argc < 4) { usage(); return 2; }
            for (int index = 4; index < argc; ++index) {
                const std::string argument = argv[index];
                if (command == "convert" && argument == "--merge-coplanar" && !merge_given) {
                    options.merge_coplanar = 1;
                    merge_given = true;
                } else if (command == "convert" && argument == "--strict-original" && !strict_given) {
                    options.strict_original = 1;
                    strict_given = true;
                } else if (command == "convert" && argument == "--merge-angle" && !angle_given && index + 1 < argc) {
                    const std::string value = argv[++index];
                    std::size_t used = 0;
                    double angle = -1;
                    try { angle = std::stod(value, &used); } catch (const std::exception&) { used = 0; }
                    if (used != value.size() || !(angle > 0 && angle <= 0.1)) {
                        throw std::runtime_error("--merge-angle must be a number in (0, 0.1] radians");
                    }
                    options.merge_angular_tolerance = angle;
                    angle_given = true;
                } else if (command == "convert" && argument == "--mode" && !mode_given && index + 1 < argc) {
                    const std::string selected = argv[++index];
                    if (selected == "original") { options.mode = STL2STP_MODE_ORIGINAL; }
                    else if (selected == "sewing") { options.mode = STL2STP_MODE_SEWING; }
                    else { throw std::runtime_error("Mode must be original or sewing"); }
                    mode_given = true;
                } else if (((command == "simplify" && argument == "--faces") ||
                            (command == "convert" && argument == "--simplify")) && !target && index + 1 < argc) {
                    target = face_count(argv[++index]);
                } else if (command == "convert" && index + 1 < argc &&
                           (argument == "--author" || argument == "--org" ||
                            argument == "--product" || argument == "--description" || argument == "--timestamp")) {
                    const char** field = argument == "--author" ? &options.author :
                        argument == "--org" ? &options.organization :
                        argument == "--product" ? &options.product :
                        argument == "--description" ? &options.description : &options.timestamp;
                    if (*field) { throw std::runtime_error("Duplicate option: " + argument); }
                    *field = argv[++index];
                } else {
                    throw std::runtime_error("Unknown, duplicate or incomplete option: " + argument);
                }
            }
            if (command == "simplify" && !target) { throw std::runtime_error("simplify requires --faces N"); }
        }
        timed("load", [&] { check(handle.get(), stl2stp_load_stl(handle.get(), argv[2], &callbacks)); });
        if (target) {
            timed("simplify", [&] { check(handle.get(), stl2stp_simplify(handle.get(), target, &callbacks)); });
        }
        print_mesh(handle.get());
        if (command == "info") { return 0; }
        if (command == "simplify") {
            timed("save", [&] { check(handle.get(), stl2stp_save_stl(handle.get(), argv[3], &callbacks)); });
        } else {
            std::cout << "mode=" << (options.mode == STL2STP_MODE_ORIGINAL ? "original" : "sewing") << '\n';
            const auto extension = lowercase(std::filesystem::u8path(argv[3]).extension().u8string());
            stl2stp_topology counts{};
            if (extension == ".step" || extension == ".stp") {
                timed("export", [&] { check(handle.get(), stl2stp_export_step(handle.get(), argv[3], &options, &callbacks, &counts)); });
                print_topology(counts, "export");
                timed("readback", [&] { check(handle.get(), stl2stp_read_step(handle.get(), argv[3], &callbacks, &counts)); });
                print_topology(counts, "readback");
            } else if (extension == ".igs" || extension == ".iges") {
                timed("export", [&] { check(handle.get(), stl2stp_export_iges(handle.get(), argv[3], &options, &callbacks, &counts)); });
                print_topology(counts, "export");
            } else {
                throw std::runtime_error("convert output must have .step/.stp/.igs/.iges extension");
            }
        }
        std::cout << "output_bytes=" << std::filesystem::file_size(std::filesystem::u8path(argv[3])) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return interrupted ? 130 : 1;
    }
}
