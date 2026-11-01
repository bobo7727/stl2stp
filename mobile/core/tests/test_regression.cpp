// Conversion regression: public C ABI in, OCCT STEP/IGES readers back.
// Expectations live in tests/fixtures/manifest.json (see gen_fixtures.py).
#include <stl2stp/stl2stp.h>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESData_IGESModel.hxx>
#include <TCollection_HAsciiString.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <STEPControl_Reader.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shell.hxx>
#include <TopTools_ShapeMapHasher.hxx>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Json {
    enum class Kind { null, boolean, number, string, array, object };
    Kind kind = Kind::null;
    bool boolean = false;
    double number = 0;
    std::string text;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;

    const Json* find(const std::string& key) const {
        for (const auto& entry : object) {
            if (entry.first == key) { return &entry.second; }
        }
        return nullptr;
    }
    const Json& at(const std::string& key) const {
        const Json* value = find(key);
        if (!value) { throw std::runtime_error("manifest missing " + key); }
        return *value;
    }
    bool has(const std::string& key) const { return find(key) != nullptr; }
    bool is_null() const { return kind == Kind::null; }
    double as_number() const {
        if (kind != Kind::number) { throw std::runtime_error("expected number"); }
        return number;
    }
    int as_int() const { return static_cast<int>(std::llround(as_number())); }
    const std::string& as_string() const {
        if (kind != Kind::string) { throw std::runtime_error("expected string"); }
        return text;
    }
};

struct Parser {
    const std::string& input;
    std::size_t index = 0;

    explicit Parser(const std::string& text) : input(text) {}

    void skip() {
        while (index < input.size() && std::isspace(static_cast<unsigned char>(input[index]))) { ++index; }
    }
    char peek() {
        skip();
        if (index >= input.size()) { throw std::runtime_error("truncated JSON"); }
        return input[index];
    }
    char get() {
        const char character = peek();
        ++index;
        return character;
    }
    [[noreturn]] void fail(const std::string& message) const { throw std::runtime_error("JSON: " + message); }

    Json parse() {
        Json value = parse_value();
        skip();
        if (index != input.size()) { fail("trailing data"); }
        return value;
    }
    Json parse_value() {
        const char character = peek();
        if (character == '{') { return parse_object(); }
        if (character == '[') { return parse_array(); }
        if (character == '"') { return parse_string(); }
        if (character == 't' || character == 'f') { return parse_bool(); }
        if (character == 'n') { return parse_null(); }
        return parse_number();
    }
    Json parse_object() {
        Json value;
        value.kind = Json::Kind::object;
        get();
        if (peek() == '}') { get(); return value; }
        while (true) {
            Json key = parse_string();
            if (get() != ':') { fail("expected colon"); }
            value.object.emplace_back(key.text, parse_value());
            const char separator = get();
            if (separator == '}') { break; }
            if (separator != ',') { fail("expected comma"); }
        }
        return value;
    }
    Json parse_array() {
        Json value;
        value.kind = Json::Kind::array;
        get();
        if (peek() == ']') { get(); return value; }
        while (true) {
            value.array.push_back(parse_value());
            const char separator = get();
            if (separator == ']') { break; }
            if (separator != ',') { fail("expected comma"); }
        }
        return value;
    }
    Json parse_string() {
        if (get() != '"') { fail("expected string"); }
        Json value;
        value.kind = Json::Kind::string;
        while (true) {
            if (index >= input.size()) { fail("unterminated string"); }
            char character = input[index++];
            if (character == '"') { break; }
            if (character == '\\') {
                if (index >= input.size()) { fail("bad escape"); }
                const char escaped = input[index++];
                switch (escaped) {
                case '"': case '\\': case '/': value.text.push_back(escaped); break;
                case 'b': value.text.push_back('\b'); break;
                case 'f': value.text.push_back('\f'); break;
                case 'n': value.text.push_back('\n'); break;
                case 'r': value.text.push_back('\r'); break;
                case 't': value.text.push_back('\t'); break;
                default: fail("unsupported escape");
                }
            } else {
                value.text.push_back(character);
            }
        }
        return value;
    }
    Json parse_bool() {
        Json value;
        value.kind = Json::Kind::boolean;
        if (input.compare(index, 4, "true") == 0) { value.boolean = true; index += 4; return value; }
        if (input.compare(index, 5, "false") == 0) { value.boolean = false; index += 5; return value; }
        fail("expected boolean");
    }
    Json parse_null() {
        if (input.compare(index, 4, "null") != 0) { fail("expected null"); }
        index += 4;
        return Json{};
    }
    Json parse_number() {
        const std::size_t start = index;
        skip();
        const std::size_t number_start = index;
        if (peek() == '-') { ++index; }
        if (index >= input.size() || !std::isdigit(static_cast<unsigned char>(input[index]))) { fail("expected number"); }
        while (index < input.size() && std::isdigit(static_cast<unsigned char>(input[index]))) { ++index; }
        if (index < input.size() && input[index] == '.') {
            ++index;
            while (index < input.size() && std::isdigit(static_cast<unsigned char>(input[index]))) { ++index; }
        }
        if (index < input.size() && (input[index] == 'e' || input[index] == 'E')) {
            ++index;
            if (index < input.size() && (input[index] == '+' || input[index] == '-')) { ++index; }
            while (index < input.size() && std::isdigit(static_cast<unsigned char>(input[index]))) { ++index; }
        }
        Json value;
        value.kind = Json::Kind::number;
        try {
            value.number = std::stod(input.substr(number_start, index - number_start));
        } catch (const std::exception&) {
            fail("bad number at " + std::to_string(start));
        }
        return value;
    }
};

Json load_json(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) { throw std::runtime_error("cannot read " + path.string()); }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return Parser(buffer.str()).parse();
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { throw std::runtime_error("cannot read " + path.string()); }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool near(double actual, double expected, double tolerance) {
    return std::isfinite(actual) && std::abs(actual - expected) <= tolerance;
}

double bbox_tolerance(const Json& box_min, const Json& box_max) {
    double scale = 1;
    for (const Json& value : box_min.array) { scale = std::max(scale, std::abs(value.as_number())); }
    for (const Json& value : box_max.array) { scale = std::max(scale, std::abs(value.as_number())); }
    return std::max(1e-4, 1e-6 * scale);
}

bool shell_is_closed(const TopoDS_Shell& shell) {
    NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher> edges;
    TopExp::MapShapesAndAncestors(static_cast<const TopoDS_Shape&>(shell), TopAbs_EDGE, TopAbs_FACE, edges);
    if (edges.Extent() == 0) { return false; }
    for (int index = 1; index <= edges.Extent(); ++index) {
        if (edges(index).Extent() != 2) { return false; }
    }
    return true;
}

bool step_unit_is_mm(const std::string& text) {
    const bool milli_metre = text.find(".MILLI.") != std::string::npos && text.find(".METRE.") != std::string::npos;
    const bool named = text.find("MILLIMETRE") != std::string::npos || text.find("MILLIMETER") != std::string::npos;
    return milli_metre || named;
}

struct CadReport {
    bool read_ok = false;
    bool unit_mm = false;
    int solids = 0;
    int shells = 0;
    int faces = 0;
    bool shells_closed = false;
    double volume = 0;
    double box_min[3]{};
    double box_max[3]{};
    std::string detail;
};

CadReport inspect_shape(const TopoDS_Shape& shape, bool unit_mm) {
    CadReport report;
    report.read_ok = !shape.IsNull();
    report.unit_mm = unit_mm;
    if (shape.IsNull()) {
        report.detail = "null shape";
        return report;
    }
    for (TopExp_Explorer solid(shape, TopAbs_SOLID); solid.More(); solid.Next()) {
        ++report.solids;
        GProp_GProps props;
        BRepGProp::VolumeProperties(solid.Current(), props);
        report.volume += props.Mass();
    }
    report.shells_closed = true;
    for (TopExp_Explorer shell(shape, TopAbs_SHELL); shell.More(); shell.Next()) {
        ++report.shells;
        if (!shell_is_closed(TopoDS::Shell(shell.Current()))) { report.shells_closed = false; }
    }
    if (report.shells == 0) { report.shells_closed = false; }
    if (report.solids == 0) {
        GProp_GProps props;
        BRepGProp::VolumeProperties(shape, props);
        report.volume = props.Mass();
    }
    for (TopExp_Explorer face(shape, TopAbs_FACE); face.More(); face.Next()) { ++report.faces; }
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    if (!box.IsVoid()) { box.Get(report.box_min[0], report.box_min[1], report.box_min[2], report.box_max[0], report.box_max[1], report.box_max[2]); }
    return report;
}

CadReport read_step_file(const std::filesystem::path& path) {
    STEPControl_Reader reader;
    if (reader.ReadFile(path.u8string().c_str()) != IFSelect_RetDone) {
        CadReport report;
        report.detail = "STEP ReadFile failed";
        return report;
    }
    if (reader.TransferRoots() <= 0) {
        CadReport report;
        report.detail = "STEP TransferRoots failed";
        return report;
    }
    return inspect_shape(reader.OneShape(), step_unit_is_mm(read_text(path)));
}

CadReport read_iges_file(const std::filesystem::path& path) {
    IGESControl_Reader reader;
    if (reader.ReadFile(path.u8string().c_str()) != IFSelect_RetDone) {
        CadReport report;
        report.detail = "IGES ReadFile failed";
        return report;
    }
    bool unit_mm = false;
    const Handle(IGESData_IGESModel) model = reader.IGESModel();
    if (!model.IsNull()) {
        const IGESData_GlobalSection section = model->GlobalSection();
        const Handle(TCollection_HAsciiString) unit_name = section.UnitName();
        const std::string name = unit_name.IsNull() ? std::string() : unit_name->ToCString();
        unit_mm = section.UnitFlag() == 2 || name == "MM" || name == "mm";
    }
    if (!unit_mm) {
        const std::string text = read_text(path);
        unit_mm = text.find("2HMM") != std::string::npos;
    }
    if (reader.TransferRoots() <= 0) {
        CadReport report;
        report.detail = "IGES TransferRoots failed";
        report.unit_mm = unit_mm;
        return report;
    }
    return inspect_shape(reader.OneShape(), unit_mm);
}

struct Timing {
    std::string id;
    long long step_export_ms = -1;
    long long step_read_ms = -1;
    long long iges_export_ms = -1;
    long long iges_read_ms = -1;
    int step_status = 0;
    int iges_status = 0;
    std::string note;
};

using clock_type = std::chrono::steady_clock;

long long millis_since(clock_type::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock_type::now() - start).count();
}

struct FailureLog {
    std::vector<std::string> items;
    bool probe = false;
    void add(const std::string& message) {
        items.push_back(message);
        std::cout << "  FAIL " << message << '\n';
    }
};

void expect_eq(FailureLog& failures, const std::string& label, long long actual, long long expected) {
    if (actual != expected) {
        failures.add(label + " actual " + std::to_string(actual) + " expected " + std::to_string(expected));
    }
}

void expect_true(FailureLog& failures, const std::string& label, bool condition) {
    if (!condition) { failures.add(label); }
}

void check_bbox(FailureLog& failures, const std::string& label, const double* actual_min, const double* actual_max,
                const Json& expected_min, const Json& expected_max, double tolerance) {
    if (expected_min.kind != Json::Kind::array || expected_min.array.size() != 3) { return; }
    for (int axis = 0; axis < 3; ++axis) {
        if (!near(actual_min[axis], expected_min.array[static_cast<std::size_t>(axis)].as_number(), tolerance) ||
            !near(actual_max[axis], expected_max.array[static_cast<std::size_t>(axis)].as_number(), tolerance)) {
            failures.add(label + " axis " + std::to_string(axis) + " actual [" + std::to_string(actual_min[axis]) + "," +
                         std::to_string(actual_max[axis]) + "] expected [" +
                         std::to_string(expected_min.array[static_cast<std::size_t>(axis)].as_number()) + "," +
                         std::to_string(expected_max.array[static_cast<std::size_t>(axis)].as_number()) + "] tol " +
                         std::to_string(tolerance));
            return;
        }
    }
}

void check_format(FailureLog& failures, const std::string& label, const Json& expected, const CadReport& report,
                  const Json& bbox_min, const Json& bbox_max, const Json* volume, double volume_abs, double volume_rel) {
    expect_true(failures, label + " re-read failed: " + report.detail, report.read_ok);
    if (!report.read_ok) { return; }
    expect_true(failures, label + " length unit is not millimetres", report.unit_mm);
    expect_eq(failures, label + " solids", report.solids, expected.at("solids").as_int());
    expect_eq(failures, label + " faces", report.faces, expected.at("faces").as_int());
    if (expected.at("shell_closed").kind == Json::Kind::boolean) {
        expect_true(failures, label + (expected.at("shell_closed").boolean ? " shell is open" : " shell is closed"),
                    report.shells_closed == expected.at("shell_closed").boolean);
    }
    if (bbox_min.kind == Json::Kind::array) {
        check_bbox(failures, label + " bbox", report.box_min, report.box_max, bbox_min, bbox_max, bbox_tolerance(bbox_min, bbox_max));
    }
    if (volume && volume->kind == Json::Kind::number) {
        const double expected_volume = volume->as_number();
        const double tolerance = std::max(volume_abs, volume_rel * std::abs(expected_volume));
        if (!near(report.volume, expected_volume, tolerance)) {
            failures.add(label + " volume actual " + std::to_string(report.volume) + " expected " +
                         std::to_string(expected_volume) + " tol " + std::to_string(tolerance));
        }
    }
}

std::string status_name(int status) {
    switch (status) {
    case STL2STP_OK: return "OK";
    case STL2STP_WARNING_PARTIAL: return "PARTIAL";
    case STL2STP_WARNING_TARGET_NOT_REACHED: return "TARGET_NOT_REACHED";
    case STL2STP_WARNING_NOT_SOLID: return "NOT_SOLID";
    case STL2STP_WARNING_REPAIRED: return "REPAIRED";
    case STL2STP_WARNING_METADATA_CHANGED: return "METADATA_CHANGED";
    case STL2STP_ERROR_ARGUMENT: return "ARGUMENT";
    case STL2STP_ERROR_IO: return "IO";
    case STL2STP_ERROR_FORMAT: return "FORMAT";
    case STL2STP_ERROR_EMPTY: return "EMPTY";
    case STL2STP_ERROR_CANCELLED: return "CANCELLED";
    case STL2STP_ERROR_OCCT: return "OCCT";
    case STL2STP_ERROR_MEMORY: return "MEMORY";
    case STL2STP_ERROR_INTERNAL: return "INTERNAL";
    default: return std::to_string(status);
    }
}

}  // namespace

int main(int argc, char** argv) {
    bool probe = false;
    std::vector<std::string> args;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--probe") { probe = true; }
        else { args.push_back(arg); }
    }
    if (args.size() != 3) {
        std::cerr << "Usage: stl2stp-regression [--probe] FIXTURES LARGE_DIR WORK_DIR\n";
        return 2;
    }
    const std::filesystem::path fixtures = std::filesystem::u8path(args[0]);
    const std::filesystem::path large_dir = std::filesystem::u8path(args[1]);
    const std::filesystem::path work = std::filesystem::u8path(args[2]);
    try {
        std::filesystem::create_directories(work);
        const Json manifest = load_json(fixtures / "manifest.json");
        std::ofstream timings(work / "timings.txt");
        timings << "id\tload\ttriangles\tstep_status\tstep_export_ms\tstep_read_ms\tstep_solids\tstep_faces\tstep_volume\t"
                   "iges_status\tiges_export_ms\tiges_read_ms\tiges_solids\tiges_faces\tiges_volume\tnote\n";
        int failed_cases = 0;
        const Json& cases = manifest.at("cases");
        for (const Json& item : cases.array) {
            const std::string file = item.at("file").as_string();
            const std::filesystem::path stl = (item.at("generate").kind == Json::Kind::boolean && item.at("generate").boolean)
                ? large_dir / file
                : fixtures / file;
            std::cout << "CASE " << file << '\n';
            FailureLog failures;
            failures.probe = probe;
            Timing timing;
            timing.id = file;
            if (const Json* bug = item.find("known_bug"); bug && bug->kind == Json::Kind::string && !bug->text.empty()) {
                std::cout << "  KNOWN_BUG " << bug->text << '\n';
                timing.note = bug->text;
            }
            stl2stp_handle* handle = stl2stp_create();
            if (!handle) {
                failures.add("stl2stp_create returned null");
            } else if (!std::filesystem::is_regular_file(stl)) {
                failures.add("missing fixture " + stl.string());
            } else {
                const int load_status = stl2stp_load_stl(handle, stl.u8string().c_str(), nullptr);
                expect_eq(failures, "load status", load_status, item.at("load_status").as_int());
                if (load_status != STL2STP_OK) {
                    const char* message = stl2stp_last_error(handle);
                    expect_true(failures, "load status " + status_name(load_status) + " has an empty diagnostic",
                                message && message[0] != '\0');
                }
                const bool do_export = item.at("export").kind == Json::Kind::boolean && item.at("export").boolean;
                const bool assert_mesh = !item.has("assert_mesh") || item.at("assert_mesh").is_null() ||
                    (item.at("assert_mesh").kind == Json::Kind::boolean && item.at("assert_mesh").boolean);
                if (load_status < 0) {
                    expect_true(failures, "failed load must not be exported", !do_export);
                    timings << file << '\t' << load_status << '\t' << item.at("triangles").as_int()
                            << "\t\t\t\t\t\t\t\t\t\t\t\t" << timing.note << '\n';
                } else if (do_export) {
                    stl2stp_mesh_stats stats{};
                    const int stats_status = stl2stp_get_mesh_stats(handle, &stats);
                    expect_eq(failures, "mesh stats status", stats_status, load_status >= 0 ? STL2STP_OK : stats_status);
                    if (stats_status >= 0 && assert_mesh) {
                        expect_eq(failures, "triangles", static_cast<long long>(stats.faces), item.at("triangles").as_int());
                        expect_eq(failures, "vertices", static_cast<long long>(stats.vertices), item.at("vertices").as_int());
                        if (item.at("bbox_min").kind == Json::Kind::array) {
                            const double actual_min[3] = {stats.minimum[0], stats.minimum[1], stats.minimum[2]};
                            const double actual_max[3] = {stats.maximum[0], stats.maximum[1], stats.maximum[2]};
                            check_bbox(failures, "mesh bbox", actual_min, actual_max, item.at("bbox_min"), item.at("bbox_max"),
                                       bbox_tolerance(item.at("bbox_min"), item.at("bbox_max")));
                        }
                    }
                    const stl2stp_export_options options = stl2stp_default_export_options();
                    const double volume_abs = item.at("volume_abs_tol").as_number();
                    const double volume_rel = item.at("volume_rel_tol").as_number();
                    const Json* volume = item.find("volume");
                    const long long budget_ms = item.at("time_budget_s").is_null()
                        ? -1
                        : static_cast<long long>(item.at("time_budget_s").as_number() * 1000.0);
                    CadReport step_report;
                    CadReport iges_report;
                    for (const char* format : {"step", "iges"}) {
                        const Json& expected = item.at(format);
                        const auto output = work / (std::string(file) + "." + format);
                        std::error_code ignored;
                        std::filesystem::remove(output, ignored);
                        stl2stp_topology topology{};
                        const auto started = clock_type::now();
                        const int status = std::string(format) == "step"
                            ? stl2stp_export_step(handle, output.u8string().c_str(), &options, nullptr, &topology)
                            : stl2stp_export_iges(handle, output.u8string().c_str(), &options, nullptr, &topology);
                        const long long export_ms = millis_since(started);
                        if (std::string(format) == "step") { timing.step_export_ms = export_ms; timing.step_status = status; }
                        else { timing.iges_export_ms = export_ms; timing.iges_status = status; }
                        expect_eq(failures, std::string(format) + " export status", status, expected.at("status").as_int());
                        if (status != STL2STP_OK) {
                            const char* message = stl2stp_last_error(handle);
                            expect_true(failures, std::string(format) + " status has an empty diagnostic", message && message[0] != '\0');
                        }
                        if (budget_ms >= 0 && export_ms > budget_ms) {
                            failures.add(std::string(format) + " export " + std::to_string(export_ms) +
                                         " ms exceeds budget " + std::to_string(budget_ms) + " ms");
                        }
                        CadReport report;
                        if (status >= 0 && std::filesystem::is_regular_file(output)) {
                            const auto read_started = clock_type::now();
                            try {
                                report = std::string(format) == "step" ? read_step_file(output) : read_iges_file(output);
                            } catch (const std::exception& error) {
                                report.detail = error.what();
                            }
                            const long long read_ms = millis_since(read_started);
                            if (std::string(format) == "step") { timing.step_read_ms = read_ms; }
                            else { timing.iges_read_ms = read_ms; }
                            if (budget_ms >= 0 && read_ms > budget_ms) {
                                failures.add(std::string(format) + " re-read " + std::to_string(read_ms) +
                                             " ms exceeds budget " + std::to_string(budget_ms) + " ms");
                            }
                            if (assert_mesh) {
                                check_format(failures, format, expected, report, item.at("bbox_min"), item.at("bbox_max"),
                                             volume, volume_abs, volume_rel);
                            }
                            std::cout << "  " << format << " status=" << status_name(status) << " export_ms=" << export_ms
                                      << " read_ms=" << read_ms << " solids=" << report.solids << " shells=" << report.shells
                                      << " faces=" << report.faces << " closed=" << (report.shells_closed ? "yes" : "no")
                                      << " volume=" << report.volume << " unit_mm=" << (report.unit_mm ? "yes" : "no")
                                      << " export_faces=" << topology.faces << '\n';
                        } else if (status >= 0) {
                            failures.add(std::string(format) + " export reported success but wrote no file");
                        } else {
                            std::cout << "  " << format << " status=" << status_name(status) << " export_ms=" << export_ms
                                      << " (no re-read)\n";
                        }
                        if (std::string(format) == "step") { step_report = report; }
                        else { iges_report = report; }
                        std::filesystem::remove(output, ignored);
                    }
                    timings << file << '\t' << load_status << '\t' << item.at("triangles").as_int() << '\t'
                            << timing.step_status << '\t' << timing.step_export_ms << '\t' << timing.step_read_ms << '\t'
                            << step_report.solids << '\t' << step_report.faces << '\t' << step_report.volume << '\t'
                            << timing.iges_status << '\t' << timing.iges_export_ms << '\t' << timing.iges_read_ms << '\t'
                            << iges_report.solids << '\t' << iges_report.faces << '\t' << iges_report.volume << '\t'
                            << timing.note << '\n';
                } else {
                    timings << file << '\t' << load_status << '\t' << item.at("triangles").as_int()
                            << "\t\t\t\t\t\t\t\t\t\t\t\t" << timing.note << '\n';
                }
            }
            stl2stp_destroy(handle);
            if (!failures.items.empty()) {
                if (probe) {
                    std::cout << "  PROBE " << failures.items.size() << " mismatch(es), not failing the process\n";
                } else {
                    ++failed_cases;
                }
            } else {
                std::cout << "  PASS\n";
            }
            timings.flush();
        }
        timings.close();
        std::cout << "TIMINGS " << (work / "timings.txt").string() << '\n';
        if (!probe && failed_cases != 0) {
            std::cerr << failed_cases << " fixture(s) failed\n";
            return 1;
        }
        std::cout << "regression " << (probe ? "probe" : "passed") << " (" << cases.array.size() << " fixtures)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "regression aborted: " << error.what() << '\n';
        return 1;
    }
}
