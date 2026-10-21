#include "internal.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepLib.hxx>
#include <BRepTools.hxx>
#include <APIHeaderSection_MakeHeader.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Writer.hxx>
#include <IGESData_IGESModel.hxx>
#include <IGESData_GlobalSection.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <Message_ProgressIndicator.hxx>
#include <Message_ProgressScope.hxx>
#include <Poly_MergeNodesTool.hxx>
#include <Poly_Triangulation.hxx>
#include <NCollection_IndexedMap.hxx>
#include <STEPControl_Controller.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
#include <StepData_StepModel.hxx>
#include <StepBasic_Product.hxx>
#include <TCollection_HAsciiString.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <locale>
#include <exception>
#include <fstream>
#include <memory>
#include <mutex>
#include <map>
#include <streambuf>
#include <unordered_map>

namespace stl2stp {
namespace {

std::timed_mutex exchange_mutex;

struct Timestamp {
    std::string iso;
    std::string iges;
};

Timestamp timestamp_value(const char* supplied) {
    std::string iso;
    if (supplied) {
        iso = supplied;
    } else {
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto* utc = std::gmtime(&now);
        if (!utc) { throw Error(STL2STP_ERROR_INTERNAL, "Cannot obtain current UTC time"); }
        std::ostringstream formatted;
        formatted.imbue(std::locale::classic());
        formatted << std::put_time(utc, "%Y-%m-%dT%H:%M:%SZ");
        iso = formatted.str();
    }
    auto digits = [&](std::size_t begin, std::size_t length) {
        if (begin > iso.size() || length > iso.size() - begin) { return false; }
        for (std::size_t index = begin; index < begin + length; ++index) {
            if (iso[index] < '0' || iso[index] > '9') { return false; }
        }
        return true;
    };
    if (iso.size() < 19 || iso[4] != '-' || iso[7] != '-' || iso[10] != 'T' || iso[13] != ':' ||
        iso[16] != ':' || !digits(0, 4) || !digits(5, 2) || !digits(8, 2) ||
        !digits(11, 2) || !digits(14, 2) || !digits(17, 2)) {
        throw Error(STL2STP_ERROR_ARGUMENT, "Timestamp must be an ISO 8601 extended date-time");
    }
    std::size_t zone_begin = 19;
    if (zone_begin < iso.size() && iso[zone_begin] == '.') {
        const auto fraction_begin = ++zone_begin;
        while (zone_begin < iso.size() && digits(zone_begin, 1)) { ++zone_begin; }
        if (zone_begin == fraction_begin) { throw Error(STL2STP_ERROR_ARGUMENT, "Timestamp fraction is empty"); }
    }
    const std::string zone = iso.substr(zone_begin);
    if (!zone.empty() && zone != "Z" && (zone.size() != 6 || (zone[0] != '+' && zone[0] != '-') || zone[3] != ':' ||
                       !digits(zone_begin + 1, 2) || !digits(zone_begin + 4, 2))) {
        throw Error(STL2STP_ERROR_ARGUMENT, "Timestamp timezone must be Z or +/-HH:MM");
    }
    int year = std::stoi(iso.substr(0, 4));
    int month = std::stoi(iso.substr(5, 2));
    int day = std::stoi(iso.substr(8, 2));
    const int hour = std::stoi(iso.substr(11, 2));
    const int minute = std::stoi(iso.substr(14, 2));
    const int second = std::stoi(iso.substr(17, 2));
    const std::array<int, 12> days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    auto month_days = [&](int calendar_year, int calendar_month) {
        const bool leap = calendar_year % 4 == 0 && (calendar_year % 100 != 0 || calendar_year % 400 == 0);
        return days[calendar_month - 1] + (calendar_month == 2 && leap ? 1 : 0);
    };
    const bool has_offset = !zone.empty() && zone != "Z";
    const int offset_hour = has_offset ? std::stoi(zone.substr(1, 2)) : 0;
    const int offset_minute = has_offset ? std::stoi(zone.substr(4, 2)) : 0;
    if (year == 0 || month < 1 || month > 12 || day < 1 ||
        day > month_days(year, month) ||
        hour > 23 || minute > 59 || second > 59 ||
        offset_hour > 23 || offset_minute > 59) {
        throw Error(STL2STP_ERROR_ARGUMENT, "Timestamp has an invalid calendar date, time or offset");
    }
    const int offset = (has_offset && zone[0] == '-' ? -1 : 1) * (offset_hour * 60 + offset_minute);
    int utc_minutes = hour * 60 + minute - offset;
    if (utc_minutes < 0) {
        utc_minutes += 24 * 60;
        if (--day == 0) {
            if (--month == 0) { month = 12; --year; }
            day = month_days(year, month);
        }
    } else if (utc_minutes >= 24 * 60) {
        utc_minutes -= 24 * 60;
        if (++day > month_days(year, month)) {
            day = 1;
            if (++month == 13) { month = 1; ++year; }
        }
    }
    if (year < 1 || year > 9999) {
        throw Error(STL2STP_ERROR_ARGUMENT, "Timestamp UTC year must be between 0001 and 9999");
    }
    std::ostringstream formatted;
    formatted.imbue(std::locale::classic());
    formatted << std::setfill('0') << std::setw(4) << year << std::setw(2) << month << std::setw(2) << day
              << '.' << std::setw(2) << utc_minutes / 60 << std::setw(2) << utc_minutes % 60
              << std::setw(2) << second;
    // STEP: same UTC instant in ISO 8601 extended form with Z; fractional seconds are kept verbatim.
    std::ostringstream step;
    step.imbue(std::locale::classic());
    step << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2) << month << '-' << std::setw(2) << day
         << 'T' << std::setw(2) << utc_minutes / 60 << ':' << std::setw(2) << utc_minutes % 60 << ':'
         << std::setw(2) << second << iso.substr(19, zone_begin - 19) << 'Z';
    return {step.str(), formatted.str()};
}

std::string step_text(const std::string& utf8) {
    std::string encoded;
    std::string directive;
    auto close = [&] {
        if (!directive.empty()) { encoded += "\\X0\\"; directive.clear(); }
    };
    for (std::size_t cursor = 0; cursor < utf8.size();) {
        const auto first = static_cast<unsigned char>(utf8[cursor++]);
        uint32_t codepoint = first;
        int remaining = 0;
        uint32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { remaining = 1; codepoint &= 0x1f; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { remaining = 2; codepoint &= 0x0f; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; codepoint &= 0x07; minimum = 0x10000; }
        else if (first >= 0x80) { throw Error(STL2STP_ERROR_ARGUMENT, "STEP metadata must be valid UTF-8"); }
        for (int index = 0; index < remaining; ++index) {
            if (cursor == utf8.size() || (static_cast<unsigned char>(utf8[cursor]) & 0xc0) != 0x80) {
                throw Error(STL2STP_ERROR_ARGUMENT, "STEP metadata must be valid UTF-8");
            }
            codepoint = (codepoint << 6) | (static_cast<unsigned char>(utf8[cursor++]) & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            throw Error(STL2STP_ERROR_ARGUMENT, "STEP metadata must contain Unicode scalar values");
        }
        if (codepoint == '\\') {
            close();
            encoded += "\\X2\\005C\\X0\\";
        } else if (codepoint >= 0x20 && codepoint <= 0x7e) {
            close();
            encoded += static_cast<char>(codepoint);
        } else {
            const std::string next = codepoint <= 0xffff ? "\\X2\\" : "\\X4\\";
            if (directive != next) { close(); encoded += next; directive = next; }
            for (int shift = codepoint <= 0xffff ? 12 : 28; shift >= 0; shift -= 4) {
                encoded += "0123456789ABCDEF"[(codepoint >> shift) & 0xf];
            }
        }
    }
    close();
    return encoded;
}

// OCCT writes "\" inside STEP strings as \X2\005C\X0\ and wraps long strings across lines.
// This filter sits between STEPControl_Writer::WriteStream and the file and, in a single pass,
// rewrites \X2\005C\X0\ to the ISO 10303-21 "\\" escape and joins wrapped string lines.
// (Previously the whole file was re-read and re-written character by character.)
class StepStringFilter : public std::streambuf {
public:
    StepStringFilter(std::ostream& sink, Progress& progress) : sink_(sink), progress_(progress) {
        setp(buffer_.data(), buffer_.data() + buffer_.size());
        out_.reserve(chunk + 64);
    }

    void finish() {
        if (failure_) { std::rethrow_exception(failure_); }
        drain();
        out_ += pending_;
        pending_.clear();
        if (quote_pending_) { out_ += '\''; }
        if (in_string_ && !quote_pending_) { throw Error(STL2STP_ERROR_IO, "Unterminated STEP string"); }
        flush_out();
    }

protected:
    // Exceptions must not escape into std::ostream (it would swallow them and only set badbit),
    // so failures (I/O, cancellation) are stored and rethrown by finish().
    int_type overflow(int_type character) override {
        if (failure_) { return traits_type::eof(); }
        try {
            drain();
            if (!traits_type::eq_int_type(character, traits_type::eof())) { process(traits_type::to_char_type(character)); }
            return traits_type::not_eof(character);
        } catch (...) {
            failure_ = std::current_exception();
            return traits_type::eof();
        }
    }

    int sync() override {
        if (failure_) { return -1; }
        try {
            drain();
            return 0;
        } catch (...) {
            failure_ = std::current_exception();
            return -1;
        }
    }

private:
    static constexpr std::size_t chunk = 1u << 20;

    void drain() {
        for (char* cursor = pbase(); cursor != pptr(); ++cursor) { process(*cursor); }
        setp(buffer_.data(), buffer_.data() + buffer_.size());
        if (out_.size() >= chunk) { flush_out(); }
    }

    void flush_out() {
        sink_.write(out_.data(), static_cast<std::streamsize>(out_.size()));
        out_.clear();
        if (!sink_) { throw Error(STL2STP_ERROR_IO, "STEP write failed"); }
        progress_.check();
    }

    void emit(char byte) {
        static const std::string marker = "\\X2\\005C\\X0\\";
        pending_ += byte;
        while (!pending_.empty()) {
            if (pending_ == marker) { out_ += "\\\\"; pending_.clear(); break; }
            if (marker.compare(0, pending_.size(), pending_) == 0) { break; }
            out_ += pending_.front();
            pending_.erase(0, 1);
        }
    }

    void process(char character) {
        if (in_string_ && (character == '\n' || character == '\r')) { return; }
        if (quote_pending_) {
            quote_pending_ = false;
            if (character == '\'') { emit('\''); emit('\''); return; }
            out_ += pending_;
            out_ += '\'';
            pending_.clear();
            in_string_ = false;
        }
        if (!in_string_) {
            out_ += character;
            in_string_ = character == '\'';
        } else if (character == '\'') {
            quote_pending_ = true;
        } else {
            emit(character);
        }
    }

    std::ostream& sink_;
    Progress& progress_;
    std::array<char, 1u << 16> buffer_{};
    std::string out_;
    std::string pending_;
    bool in_string_ = false;
    bool quote_pending_ = false;
    std::exception_ptr failure_;
};

// IGES 5.3 string constants are ASCII; non-ASCII readers mis-count Hollerith lengths.
// Every non-ASCII code point (and every invalid UTF-8 byte) becomes '?'.
std::string iges_ascii(const std::string& utf8, bool& changed) {
    std::string result;
    result.reserve(utf8.size());
    for (std::size_t cursor = 0; cursor < utf8.size();) {
        const auto first = static_cast<unsigned char>(utf8[cursor]);
        if (first < 0x80) { result += static_cast<char>(first); ++cursor; continue; }
        std::size_t length = first >= 0xf0 && first <= 0xf4 ? 4 : first >= 0xe0 ? 3 : first >= 0xc2 && first <= 0xdf ? 2 : 1;
        if (first >= 0xf5) { length = 1; }
        std::size_t consumed = 1;
        while (consumed < length && cursor + consumed < utf8.size() &&
               (static_cast<unsigned char>(utf8[cursor + consumed]) & 0xc0) == 0x80) { ++consumed; }
        cursor += consumed;
        result += '?';
        changed = true;
    }
    return result;
}

struct Notes {
    bool not_solid = false;
    bool repaired = false;
    bool metadata = false;
    std::string text;

    void add(bool Notes::*flag, const std::string& message) {
        this->*flag = true;
        if (text.find(message) != std::string::npos) { return; }
        if (!text.empty()) { text += "; "; }
        text += message;
    }

    stl2stp_status status() const {
        return not_solid ? STL2STP_WARNING_NOT_SOLID : repaired ? STL2STP_WARNING_REPAIRED :
               metadata ? STL2STP_WARNING_METADATA_CHANGED : STL2STP_OK;
    }
};

struct Scale {
    double diagonal = 0;
    double magnitude = 0;
};

Scale model_scale(const Mesh& mesh) {
    Scale scale;
    if (mesh.points.empty()) { return scale; }
    Point minimum = mesh.points.front(), maximum = minimum;
    for (const auto& point : mesh.points) {
        minimum = {std::min(minimum.coordinate_x, point.coordinate_x), std::min(minimum.coordinate_y, point.coordinate_y),
                   std::min(minimum.coordinate_z, point.coordinate_z)};
        maximum = {std::max(maximum.coordinate_x, point.coordinate_x), std::max(maximum.coordinate_y, point.coordinate_y),
                   std::max(maximum.coordinate_z, point.coordinate_z)};
        scale.magnitude = std::max({scale.magnitude, std::abs(point.coordinate_x), std::abs(point.coordinate_y),
                                    std::abs(point.coordinate_z)});
    }
    const auto extent = subtract(maximum, minimum);
    scale.diagonal = std::sqrt(dot(extent, extent));
    return scale;
}

bool shape_valid(const TopoDS_Shape& shape) { return !shape.IsNull() && BRepCheck_Analyzer(shape).IsValid(); }

class Indicator : public Message_ProgressIndicator {
public:
    Indicator(Progress& progress, double base, double span)
        : progress_(progress), base_(base), span_(span) {}

    void checkpoint() {
        if (failure_) { std::rethrow_exception(failure_); }
        progress_.check();
    }

protected:
    bool UserBreak() override {
        if (failure_) { return true; }
        try {
            progress_.check();
            return false;
        } catch (...) {
            failure_ = std::current_exception();
            return true;
        }
    }

    void Show(const Message_ProgressScope&, const bool) override {
        if (failure_) { return; }
        try {
            progress_.report(base_ + span_ * GetPosition());
        } catch (...) {
            failure_ = std::current_exception();
        }
    }

private:
    Progress& progress_;
    double base_;
    double span_;
    std::exception_ptr failure_;
};

class StaticSetting {
public:
    StaticSetting(const char* name, const char* value) : name_(name) {
        const char* previous = Interface_Static::CVal(name);
        if (previous) { previous_ = previous; }
        if (!Interface_Static::SetCVal(name, value)) {
            throw Error(STL2STP_ERROR_OCCT, "Cannot configure OCCT parameter: " + name_);
        }
    }

    ~StaticSetting() {
        try { Interface_Static::SetCVal(name_.c_str(), previous_.c_str()); } catch (...) {}
    }

private:
    std::string name_;
    std::string previous_;
};

class IntegerSetting {
public:
    IntegerSetting(const char* name, int value) : name_(name), previous_(Interface_Static::IVal(name)) {
        if (!Interface_Static::SetIVal(name, value)) {
            throw Error(STL2STP_ERROR_OCCT, "Cannot configure OCCT parameter: " + name_);
        }
    }

    ~IntegerSetting() {
        try { Interface_Static::SetIVal(name_.c_str(), previous_); } catch (...) {}
    }

private:
    std::string name_;
    int previous_;
};

std::unique_lock<std::timed_mutex> exchange_lock(Progress& progress) {
    std::unique_lock<std::timed_mutex> lock(exchange_mutex, std::defer_lock);
    while (!lock.try_lock_for(std::chrono::milliseconds(10))) { progress.check(); }
    progress.check();
    return lock;
}

stl2stp_topology count_topology(const TopoDS_Shape& shape) {
    stl2stp_topology result{};
    auto count = [&](TopAbs_ShapeEnum type) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> unique;
        TopExp::MapShapes(shape, type, unique);
        return static_cast<uint64_t>(unique.Extent());
    };
    result.solids = count(TopAbs_SOLID);
    result.shells = count(TopAbs_SHELL);
    result.faces = count(TopAbs_FACE);
    result.edges = count(TopAbs_EDGE);
    result.vertices = count(TopAbs_VERTEX);
    return result;
}

// Coplanar merge. Tolerances are relative to the model: OCCT's default angular tolerance (1e-12 rad)
// never matches float32 STL facets of a rotated plane. The merged shape is validated and the
// unmerged shape is kept if merging made it invalid.
TopoDS_Shape merge_shape(const TopoDS_Shape& shape, const stl2stp_export_options& options, const Scale& scale,
                         Notes& notes, Progress& progress) {
    if (!options.merge_coplanar) { return shape; }
    const double angular = options.merge_angular_tolerance > 0 ? options.merge_angular_tolerance : 1e-4;
    const double linear = std::max({options.sewing_tolerance, 1e-6 * scale.diagonal,
                                    4.0 * double(std::numeric_limits<float>::epsilon()) * scale.magnitude});
    const bool valid_before = shape_valid(shape);
    progress.check();
    ShapeUpgrade_UnifySameDomain unify(shape, true, true, false);
    unify.SetLinearTolerance(linear);
    unify.SetAngularTolerance(angular);
    unify.Build();
    progress.check();
    if (unify.Shape().IsNull()) {
        throw Error(STL2STP_ERROR_OCCT, "Coplanar merge produced no shape");
    }
    ShapeFix_Shape fix(unify.Shape());
    fix.Perform();
    progress.check();
    const TopoDS_Shape merged = fix.Shape().IsNull() ? unify.Shape() : fix.Shape();
    if (valid_before && !shape_valid(merged)) {
        notes.add(&Notes::repaired, "coplanar merge skipped because the merged shape was invalid");
        return shape;
    }
    return merged;
}

// ORIGINAL mode (default). Faithful port of the desktop RebuildShapeFromMesh: one planar face per
// triangle, shared edges, no sewing. Unless strict_original is set, the result is made safe:
// zero-area triangles are dropped, each edge-connected component is consistently oriented
// (inverted components are reversed), each closed component becomes its own solid, open components
// are written as open shells, and the result is validated with BRepCheck. Returns a null shape when
// the input needs SEWING mode instead (non-manifold or non-orientable mesh, or invalid result).
// For a valid, closed, consistently oriented, single-component mesh the output is unchanged.
TopoDS_Shape make_original_shape(const Mesh& mesh, const stl2stp_export_options& options, const Scale& scale,
                                 Notes& notes, Progress& progress) {
    const auto& input = mesh.original_triangles.empty() ? mesh.triangles : mesh.original_triangles;
    if (input.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        mesh.points.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw Error(STL2STP_ERROR_FORMAT, "Original triangulation exceeds OCCT index range");
    }
    Handle(Poly_Triangulation) triangulation;
    if (mesh.original_weld_required) {
        Poly_MergeNodesTool weld(std::acos(-1.0) / 2.0, 0.0);
        std::size_t processed = 0;
        for (const auto& triangle : input) {
            gp_XYZ points[3];
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto& point = mesh.points[triangle[corner]];
                if (std::max({std::abs(point.coordinate_x), std::abs(point.coordinate_y), std::abs(point.coordinate_z)}) >
                    std::numeric_limits<float>::max()) {
                    throw Error(STL2STP_ERROR_FORMAT, "Original STL welding exceeds float32 range");
                }
                points[corner] = gp_XYZ(point.coordinate_x, point.coordinate_y, point.coordinate_z);
            }
            weld.AddTriangle(points);
            if (++processed % 64 == 0) { progress.report(0.05 * double(processed) / double(input.size())); }
        }
        triangulation = weld.Result();
    } else {
        triangulation = new Poly_Triangulation(static_cast<int>(mesh.points.size()), static_cast<int>(input.size()), false);
        for (std::size_t index = 0; index < mesh.points.size(); ++index) {
            const auto& point = mesh.points[index];
            triangulation->SetNode(static_cast<int>(index + 1), gp_Pnt(point.coordinate_x, point.coordinate_y, point.coordinate_z));
            progress.check();
        }
        for (std::size_t index = 0; index < input.size(); ++index) {
            const auto& triangle = input[index];
            triangulation->SetTriangle(static_cast<int>(index + 1), Poly_Triangle(static_cast<int>(triangle[0] + 1),
                static_cast<int>(triangle[1] + 1), static_cast<int>(triangle[2] + 1)));
            progress.check();
        }
    }
    if (triangulation.IsNull() || triangulation->NbTriangles() == 0) {
        throw Error(STL2STP_ERROR_FORMAT, "Original STL welding produced no triangles");
    }
    std::vector<Triangle> source;
    source.reserve(triangulation->NbTriangles());
    for (int index = 1; index <= triangulation->NbTriangles(); ++index) {
        const auto triangle = triangulation->Triangle(index);
        source.push_back({static_cast<std::size_t>(triangle.Value(1) - 1),
                          static_cast<std::size_t>(triangle.Value(2) - 1),
                          static_cast<std::size_t>(triangle.Value(3) - 1)});
        progress.check();
    }
    const bool strict = options.strict_original != 0;
    std::vector<std::size_t> component(source.size(), 0);
    std::vector<char> component_closed{1};
    if (!strict) {
        const double minimum_cross = 2e-12 * scale.diagonal * scale.diagonal;
        auto node = [&](std::size_t index) { return triangulation->Node(static_cast<int>(index + 1)).XYZ(); };
        std::vector<Triangle> kept;
        kept.reserve(source.size());
        for (const auto& triangle : source) {
            const gp_XYZ normal = (node(triangle[1]) - node(triangle[0])).Crossed(node(triangle[2]) - node(triangle[0]));
            if (normal.SquareModulus() > minimum_cross * minimum_cross) { kept.push_back(triangle); }
        }
        if (kept.size() != source.size()) {
            notes.add(&Notes::repaired, std::to_string(source.size() - kept.size()) + " zero-area triangle(s) dropped");
            source.swap(kept);
        }
        if (source.empty()) { throw Error(STL2STP_ERROR_FORMAT, "No non-degenerate triangles to convert"); }
        progress.check();
        struct Use { std::size_t triangle; bool forward; };
        struct EdgeUses { Use first{}; Use second{}; std::size_t count = 0; };
        std::unordered_map<uint64_t, EdgeUses> uses;
        uses.reserve(source.size() * 2);
        auto key = [](std::size_t first, std::size_t second) {
            return (uint64_t(std::min(first, second)) << 32) | uint64_t(std::max(first, second));
        };
        for (std::size_t index = 0; index < source.size(); ++index) {
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto first = source[index][corner], second = source[index][(corner + 1) % 3];
                auto& entry = uses[key(first, second)];
                const Use use{index, first < second};
                if (entry.count == 0) { entry.first = use; } else if (entry.count == 1) { entry.second = use; }
                if (++entry.count > 2) {
                    notes.add(&Notes::repaired, "non-manifold edges: converted with sewing mode");
                    return {};
                }
            }
        }
        progress.check();
        const std::size_t unvisited = std::numeric_limits<std::size_t>::max();
        std::fill(component.begin(), component.end(), unvisited);
        std::vector<char> flipped(source.size(), 0);
        component_closed.clear();
        std::vector<std::size_t> queue;
        for (std::size_t seed = 0; seed < source.size(); ++seed) {
            if (component[seed] != unvisited) { continue; }
            const std::size_t current = component_closed.size();
            component_closed.push_back(1);
            component[seed] = current;
            queue.assign(1, seed);
            while (!queue.empty()) {
                const auto triangle = queue.back();
                queue.pop_back();
                for (std::size_t corner = 0; corner < 3; ++corner) {
                    const auto& entry = uses[key(source[triangle][corner], source[triangle][(corner + 1) % 3])];
                    if (entry.count < 2) { component_closed[current] = 0; continue; }
                    const bool mine_is_first = entry.first.triangle == triangle;
                    const Use& mine = mine_is_first ? entry.first : entry.second;
                    const Use& other = mine_is_first ? entry.second : entry.first;
                    const char required = static_cast<char>(other.forward == (mine.forward != (flipped[triangle] != 0)));
                    if (component[other.triangle] == unvisited) {
                        component[other.triangle] = current;
                        flipped[other.triangle] = required;
                        queue.push_back(other.triangle);
                    } else if (flipped[other.triangle] != required) {
                        notes.add(&Notes::repaired, "non-orientable mesh: converted with sewing mode");
                        return {};
                    }
                }
            }
            progress.check();
        }
        std::vector<double> volume(component_closed.size(), 0.0);
        for (std::size_t index = 0; index < source.size(); ++index) {
            const auto& t = source[index];
            const gp_XYZ first = node(t[0]), second = node(flipped[index] ? t[2] : t[1]), third = node(flipped[index] ? t[1] : t[2]);
            volume[component[index]] += first.Dot(second.Crossed(third));
        }
        std::size_t reoriented = 0;
        for (std::size_t index = 0; index < source.size(); ++index) {
            const auto part = component[index];
            if (component_closed[part] && volume[part] < 0) { flipped[index] = !flipped[index]; }
            if (flipped[index]) { std::swap(source[index][1], source[index][2]); ++reoriented; }
        }
        if (reoriented) {
            notes.add(&Notes::repaired, std::to_string(reoriented) + " triangle(s) re-oriented (inconsistent or inverted normals)");
        }
    }
    BRep_Builder builder;
    std::vector<TopoDS_Vertex> vertices;
    vertices.reserve(triangulation->NbNodes());
    for (int index = 1; index <= triangulation->NbNodes(); ++index) {
        vertices.push_back(BRepBuilderAPI_MakeVertex(triangulation->Node(index)).Vertex());
        if (vertices.size() % 64 == 0) { progress.report(0.05 + 0.1 * double(vertices.size()) / double(triangulation->NbNodes())); }
    }
    using EdgeNodes = std::pair<std::size_t, std::size_t>;
    struct SignedEdge { std::size_t index; bool reversed; };
    std::map<EdgeNodes, std::size_t> lookup;
    std::vector<EdgeNodes> edges;
    std::vector<std::array<SignedEdge, 3>> triangles;
    triangles.reserve(source.size());
    for (const auto& triangle : source) {
        std::array<SignedEdge, 3> signed_edges{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const EdgeNodes forward{triangle[corner], triangle[(corner + 1) % 3]};
            auto found = lookup.find(forward);
            bool reversed = false;
            if (found == lookup.end()) {
                found = lookup.find({forward.second, forward.first});
                reversed = found != lookup.end();
            }
            if (found == lookup.end()) {
                found = lookup.emplace(forward, edges.size()).first;
                edges.push_back(forward);
            }
            signed_edges[corner] = {found->second, reversed};
        }
        triangles.push_back(signed_edges);
        if (triangles.size() % 64 == 0) { progress.report(0.15 + 0.15 * double(triangles.size()) / double(source.size())); }
    }
    std::vector<TopoDS_Edge> edge_shapes;
    edge_shapes.reserve(edges.size());
    for (const auto& edge : edges) {
        BRepBuilderAPI_MakeEdge maker(vertices[edge.first], vertices[edge.second]);
        if (!maker.IsDone()) { throw Error(STL2STP_ERROR_OCCT, "Original mode cannot construct shared edge"); }
        edge_shapes.push_back(maker.Edge());
        if (edge_shapes.size() % 64 == 0) { progress.report(0.3 + 0.1 * double(edge_shapes.size()) / double(edges.size())); }
    }
    std::vector<TopoDS_Face> faces;
    faces.reserve(triangles.size());
    for (const auto& triangle : triangles) {
        TopoDS_Edge first = edge_shapes[triangle[0].index];
        if (triangle[0].reversed) { first.Reverse(); }
        BRepBuilderAPI_MakeWire wire(first, edge_shapes[triangle[1].index], edge_shapes[triangle[2].index]);
        if (!wire.IsDone()) { throw Error(STL2STP_ERROR_OCCT, "Original mode cannot construct triangle wire"); }
        BRepBuilderAPI_MakeFace face(wire.Wire());
        if (!face.IsDone()) { throw Error(STL2STP_ERROR_OCCT, "Original mode cannot construct triangle face (possibly collinear)"); }
        faces.push_back(face.Face());
        if (faces.size() % 64 == 0) { progress.report(0.4 + 0.22 * double(faces.size()) / double(triangles.size())); }
    }
    progress.report(0.62);
    std::vector<TopoDS_Shell> shells(component_closed.size());
    for (auto& shell : shells) { builder.MakeShell(shell); }
    for (std::size_t index = 0; index < faces.size(); ++index) {
        builder.Add(shells[component[index]], faces[index]);
        progress.check();
    }
    std::vector<TopoDS_Shape> parts;
    for (std::size_t index = 0; index < shells.size(); ++index) {
        BRepTools::Update(shells[index]);
        if (strict || component_closed[index]) {
            TopoDS_Solid solid;
            builder.MakeSolid(solid);
            builder.Add(solid, shells[index]);
            parts.push_back(solid);
        } else {
            parts.push_back(shells[index]);
        }
    }
    TopoDS_Shape shape = parts.front();
    if (parts.size() > 1) {
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        for (const auto& part : parts) { builder.Add(compound, part); }
        shape = compound;
    }
    if (!strict) {
        progress.check();
        if (!shape_valid(shape)) {
            notes.add(&Notes::repaired, "original-mode result failed BRep validation: converted with sewing mode");
            return {};
        }
        if (std::find(component_closed.begin(), component_closed.end(), 0) != component_closed.end()) {
            notes.add(&Notes::not_solid, "mesh is not closed: open parts exported as shell/surfaces, not as solids");
        }
    }
    shape = merge_shape(shape, options, scale, notes, progress);
    progress.report(0.7);
    return shape;
}

TopoDS_Shape make_sewing_shape(const Mesh& mesh, const stl2stp_export_options& options, const Scale& scale,
                               Notes& notes, Progress& progress) {
    BRepBuilderAPI_Sewing sewing(options.sewing_tolerance, true, true, true, false);
    std::size_t index = 0;
    for (const auto& triangle : mesh.triangles) {
        BRepBuilderAPI_MakePolygon polygon;
        for (const auto vertex : triangle) {
            const auto& point = mesh.points[vertex];
            polygon.Add(gp_Pnt(point.coordinate_x, point.coordinate_y, point.coordinate_z));
        }
        polygon.Close();
        if (!polygon.IsDone()) {
            throw Error(STL2STP_ERROR_OCCT, "Cannot construct triangle wire");
        }
        BRepBuilderAPI_MakeFace face(polygon.Wire());
        if (!face.IsDone()) {
            throw Error(STL2STP_ERROR_OCCT, "Cannot construct planar triangle face");
        }
        sewing.Add(face.Face());
        if (++index % 64 == 0) {
            progress.report(0.4 * double(index) / double(mesh.triangles.size()));
        }
    }
    progress.report(0.4);
    Handle(Indicator) sew_progress = new Indicator(progress, 0.4, 0.22);
    sewing.Perform(sew_progress->Start());
    sew_progress->checkpoint();
    TopoDS_Shape sewn = sewing.SewedShape();
    if (sewn.IsNull()) {
        throw Error(STL2STP_ERROR_OCCT, "Sewing produced no shape");
    }
    progress.report(0.62);
    sewn = merge_shape(sewn, options, scale, notes, progress);
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    TopoDS_Shape only;
    std::size_t components = 0;
    auto add = [&](const TopoDS_Shape& shape) {
        builder.Add(compound, shape);
        only = shape;
        ++components;
    };
    for (TopExp_Explorer explorer(sewn, TopAbs_SHELL); explorer.More(); explorer.Next()) {
        progress.check();
        const auto shell = TopoDS::Shell(explorer.Current());
        if (BRep_Tool::IsClosed(shell)) {
            BRepBuilderAPI_MakeSolid maker(shell);
            TopoDS_Solid solid = maker.Solid();
            if (BRepLib::OrientClosedSolid(solid)) {
                add(solid);
            } else {
                add(shell);
            }
        } else {
            add(shell);
        }
    }
    for (TopExp_Explorer explorer(sewn, TopAbs_FACE, TopAbs_SHELL); explorer.More(); explorer.Next()) {
        progress.check();
        add(explorer.Current());
    }
    progress.report(0.7);
    std::size_t solids = 0;
    for (TopExp_Explorer explorer(compound, TopAbs_SOLID); explorer.More(); explorer.Next()) { ++solids; }
    if (solids < components) {
        notes.add(&Notes::not_solid, "mesh is not closed: open parts exported as shell/surfaces, not as solids");
    }
    if (components == 0) { return sewn; }
    if (components == 1) { return only; }
    return compound;
}

template <typename Function>
stl2stp_status protect_occt(stl2stp_handle* handle, Function&& function) {
    return protect(handle, [&] {
        try {
            return function();
        } catch (const Standard_Failure& failure) {
#if OCC_VERSION_MAJOR >= 8
            const char* message = failure.what();
#else
            const char* message = failure.GetMessageString();
#endif
            throw Error(STL2STP_ERROR_OCCT, message ? message : "OCCT operation failed");
        }
    });
}

stl2stp_status export_mesh(stl2stp_handle* handle, const char* path,
                           const stl2stp_export_options* supplied, const stl2stp_callbacks* callbacks,
                           stl2stp_topology* topology, bool step) {
    return protect_occt(handle, [&] {
        require_path(path);
        require_mesh(handle->mesh);
        const auto options = supplied ? *supplied : stl2stp_default_export_options();
        Notes notes;
        std::string author = options.author ? options.author : "";
        std::string organization = options.organization ? options.organization : "";
        std::string product_name = options.product ? options.product : "";
        std::string description = options.description ? options.description : "";
        const std::string supplied_timestamp = options.timestamp ? options.timestamp : "";
        Timestamp timestamp;
        std::string filename = std::filesystem::u8path(path).filename().u8string();
        auto text = [](const std::string& value) {
            return Handle(TCollection_HAsciiString)(new TCollection_HAsciiString(value.c_str()));
        };
        auto step_string = [&](const std::string& value) { return text(step_text(value)); };
        auto iges_text = [&](const std::string& value) {
            return text(value.empty() ? "" : std::to_string(value.size()) + "H" + value);
        };
        if (!step) {
            for (const auto* value : {&author, &organization, &product_name, &filename, &description}) {
                if (value->find_first_of("\r\n") != std::string::npos) {
                    throw Error(STL2STP_ERROR_ARGUMENT, "IGES metadata must not contain CR or LF");
                }
            }
            bool changed = false;
            for (auto* value : {&author, &organization, &product_name, &filename, &description}) {
                *value = iges_ascii(*value, changed);
            }
            if (changed) {
                notes.add(&Notes::metadata, "IGES metadata must be ASCII: non-ASCII characters were replaced with '?'");
            }
            for (const auto* value : {&author, &organization, &product_name, &filename}) {
                if (value->size() > 195) {
                    throw Error(STL2STP_ERROR_ARGUMENT, "IGES global strings cannot exceed 195 bytes");
                }
            }
        }
        if (!std::isfinite(options.sewing_tolerance) || options.sewing_tolerance <= 0) {
            throw Error(STL2STP_ERROR_ARGUMENT, "Sewing tolerance must be finite and positive");
        }
        if (!std::isfinite(options.merge_angular_tolerance) || options.merge_angular_tolerance < 0 ||
            options.merge_angular_tolerance > 0.1) {
            throw Error(STL2STP_ERROR_ARGUMENT, "Merge angular tolerance must be in [0, 0.1] radians (0 = default)");
        }
        if (options.mode != STL2STP_MODE_ORIGINAL && options.mode != STL2STP_MODE_SEWING) {
            throw Error(STL2STP_ERROR_ARGUMENT, "Unknown conversion mode");
        }
        if (options.mode == STL2STP_MODE_SEWING && handle->mesh.triangles.empty()) {
            throw Error(STL2STP_ERROR_FORMAT, "No non-collinear triangles for sewing");
        }
        Progress progress(callbacks);
        progress.report(0);
        auto lock = exchange_lock(progress);
        timestamp = timestamp_value(options.timestamp ? supplied_timestamp.c_str() : nullptr);
        const bool original = options.mode == STL2STP_MODE_ORIGINAL;
        const auto scale = model_scale(handle->mesh);
        TopoDS_Shape shape;
        if (original) { shape = make_original_shape(handle->mesh, options, scale, notes, progress); }
        if (shape.IsNull()) {
            if (handle->mesh.triangles.empty()) {
                throw Error(STL2STP_ERROR_FORMAT, "No non-collinear triangles for sewing");
            }
            shape = make_sewing_shape(handle->mesh, options, scale, notes, progress);
        }
        const auto counts = count_topology(shape);
        OutputFile output(path);
        const std::string temporary = output.temporary().u8string();
        Handle(Indicator) transfer_progress = new Indicator(progress, 0.7, 0.18);
        if (step) {
            STEPControl_Controller::Init();
            std::unique_ptr<StaticSetting> schema, unit;
            if (!original) {
                schema = std::make_unique<StaticSetting>("write.step.schema", "AP214IS");
                unit = std::make_unique<StaticSetting>("write.step.unit", "MM");
            }
            // After a coplanar merge vertex/edge tolerances vary; write the greatest one as the file
            // uncertainty so readers that trust it (no healing) accept the merged faces. Without merge all
            // tolerances are equal, so the default (Average) is kept and the output is unchanged.
            std::unique_ptr<IntegerSetting> precision;
            if (options.merge_coplanar) { precision = std::make_unique<IntegerSetting>("write.precision.mode", 1); }
            STEPControl_Writer writer;
            StaticSetting product("write.step.product.name", product_name.c_str());
            const auto transferred = writer.Transfer(shape, STEPControl_AsIs, true, transfer_progress->Start());
            transfer_progress->checkpoint();
            if (transferred != IFSelect_RetDone) {
                throw Error(STL2STP_ERROR_OCCT, "STEP transfer failed");
            }
            progress.report(0.88);
            const auto model = writer.Model(false);
            APIHeaderSection_MakeHeader header(model);
            header.SetName(step_string(filename));
            header.SetAuthorValue(1, step_string(author));
            header.SetOrganizationValue(1, step_string(organization));
            header.SetOriginatingSystem(step_string(product_name));
            header.SetDescriptionValue(1, step_string(description));
            header.SetPreprocessorVersion(text(""));
            header.SetAuthorisation(text(""));
            header.SetTimeStamp(text(timestamp.iso));
            for (int index = 1; index <= model->NbEntities(); ++index) {
                const auto product_entity = Handle(StepBasic_Product)::DownCast(model->Value(index));
                if (!product_entity.IsNull()) {
                    product_entity->SetId(step_string(product_name));
                    product_entity->SetName(step_string(product_name));
                }
            }
            std::ofstream file(output.temporary(), std::ios::binary);
            if (!file) { throw Error(STL2STP_ERROR_IO, "Cannot open STEP output"); }
            StepStringFilter filter(file, progress);
            std::ostream stream(&filter);
            const auto written = writer.WriteStream(stream);
            stream.flush();
            filter.finish();
            if (written != IFSelect_RetDone || !stream) { throw Error(STL2STP_ERROR_IO, "STEP write failed"); }
            file.close();
            if (!file) { throw Error(STL2STP_ERROR_IO, "STEP write failed"); }
        } else {
            IGESControl_Controller::Init();
            StaticSetting author_setting("write.iges.header.author", author.c_str());
            StaticSetting company_setting("write.iges.header.company", organization.c_str());
            StaticSetting product_setting("write.iges.header.product", product_name.c_str());
            StaticSetting receiver_setting("write.iges.header.receiver", "");
            IGESControl_Writer writer(original ? Interface_Static::CVal("write.iges.unit") : "MM",
                                     Interface_Static::IVal("write.brep.mode"));
            const bool added = writer.AddShape(shape, transfer_progress->Start());
            transfer_progress->checkpoint();
            if (!added) { throw Error(STL2STP_ERROR_OCCT, "IGES transfer failed"); }
            progress.report(0.88);
            writer.ComputeModel();
            progress.check();
            IGESData_GlobalSection section(writer.Model()->GlobalSection());
            section.SetAuthorName(iges_text(author));
            section.SetCompanyName(iges_text(organization));
            section.SetFileName(iges_text(filename));
            section.SetInterfaceVersion(text(""));
            section.SetSendName(iges_text(product_name));
            section.SetSystemId(iges_text(product_name));
            section.SetReceiveName(text(""));
            section.SetDate(text(timestamp.iges));
            section.SetLastChangeDate(text(timestamp.iges));
            section.SetApplicationProtocol(text(""));
            writer.Model()->SetGlobalSection(section);
            writer.Model()->ClearStartSection();
            if (!description.empty()) { writer.Model()->AddStartLine(description.c_str()); }
            if (!writer.Write(temporary.c_str(), false)) {
                throw Error(STL2STP_ERROR_IO, "IGES write failed");
            }
        }
        progress.report(1);
        output.commit();
        if (topology) { *topology = counts; }
        handle->error = notes.text;
        return notes.status();
    });
}

}
}

extern "C" {

stl2stp_status stl2stp_export_step(stl2stp_handle* handle, const char* path,
                                 const stl2stp_export_options* options,
                                 const stl2stp_callbacks* callbacks, stl2stp_topology* topology) {
    return stl2stp::export_mesh(handle, path, options, callbacks, topology, true);
}

stl2stp_status stl2stp_export_iges(stl2stp_handle* handle, const char* path,
                                 const stl2stp_export_options* options,
                                 const stl2stp_callbacks* callbacks, stl2stp_topology* topology) {
    return stl2stp::export_mesh(handle, path, options, callbacks, topology, false);
}

stl2stp_status stl2stp_read_step(stl2stp_handle* handle, const char* path,
                               const stl2stp_callbacks* callbacks, stl2stp_topology* topology) {
    return stl2stp::protect_occt(handle, [&] {
        stl2stp::require_path(path);
        if (!topology) { throw stl2stp::Error(STL2STP_ERROR_ARGUMENT, "Null topology output"); }
        stl2stp::Progress progress(callbacks);
        progress.report(0);
        std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
        if (!input) { throw stl2stp::Error(STL2STP_ERROR_IO, "Cannot open STEP input"); }
        input.close();
        auto lock = stl2stp::exchange_lock(progress);
        STEPControl_Reader reader;
        if (reader.ReadFile(path) != IFSelect_RetDone) {
            throw stl2stp::Error(STL2STP_ERROR_FORMAT, "Cannot parse STEP input");
        }
        progress.report(0.2);
        Handle(stl2stp::Indicator) transfer_progress = new stl2stp::Indicator(progress, 0.2, 0.79);
        const auto roots = reader.TransferRoots(transfer_progress->Start());
        transfer_progress->checkpoint();
        if (roots <= 0 || reader.OneShape().IsNull()) {
            throw stl2stp::Error(STL2STP_ERROR_FORMAT, "STEP contains no transferable shape");
        }
        const auto counts = stl2stp::count_topology(reader.OneShape());
        progress.report(1);
        *topology = counts;
        return STL2STP_OK;
    });
}

}
