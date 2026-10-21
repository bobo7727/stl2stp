#include <stl2stp/stl2stp.h>

#include <APIHeaderSection_MakeHeader.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESData_GlobalSection.hxx>
#include <IGESData_IGESModel.hxx>
#include <Interface_Static.hxx>
#include <Interface_Check.hxx>
#include <STEPControl_Controller.hxx>
#include <STEPControl_Reader.hxx>
#include <StepBasic_Product.hxx>
#include <StepData_StepModel.hxx>
#include <TCollection_HAsciiString.hxx>

#include <array>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <regex>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Handle = std::unique_ptr<stl2stp_handle, decltype(&stl2stp_destroy)>;

void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}

std::string contents(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    require(static_cast<bool>(stream), "Cannot open " + path.u8string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::string value(const Handle(TCollection_HAsciiString)& text) {
    return text.IsNull() ? "" : text->ToCString();
}

struct Metadata {
    std::string author;
    std::string organization;
    std::string product;
    std::string description;

    stl2stp_export_options options(stl2stp_mode mode) const {
        auto result = stl2stp_default_export_options();
        result.mode = mode;
        result.author = author.c_str();
        result.organization = organization.c_str();
        result.product = product.c_str();
        result.description = description.c_str();
        return result;
    }
};

// Mirror of the library rule: IGES metadata is ASCII; each non-ASCII code point becomes '?'.
std::string iges_ascii(const std::string& utf8) {
    std::string result;
    for (std::size_t cursor = 0; cursor < utf8.size();) {
        const auto first = static_cast<unsigned char>(utf8[cursor]);
        if (first < 0x80) { result += static_cast<char>(first); ++cursor; continue; }
        std::size_t length = first >= 0xf5 ? 1 : first >= 0xf0 ? 4 : first >= 0xe0 ? 3 : first >= 0xc2 ? 2 : 1;
        std::size_t consumed = 1;
        while (consumed < length && cursor + consumed < utf8.size() &&
               (static_cast<unsigned char>(utf8[cursor + consumed]) & 0xc0) == 0x80) { ++consumed; }
        cursor += consumed;
        result += '?';
    }
    return result;
}

bool ascii_only(const std::string& text) {
    return std::none_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; });
}

std::vector<std::string> iges_parameters(const std::string& file) {
    std::istringstream records(file);
    std::string record;
    std::string global;
    while (std::getline(records, record)) {
        require(record.size() == 80, "IGES record is not 80 bytes");
        if (record[72] == 'G') { global += record.substr(0, 72); }
    }
    std::vector<std::string> result;
    std::size_t cursor = 0;
    while (cursor < global.size()) {
        while (cursor < global.size() && global[cursor] == ' ') { ++cursor; }
        const auto begin = cursor;
        while (cursor < global.size() && global[cursor] >= '0' && global[cursor] <= '9') { ++cursor; }
        std::string parameter;
        if (cursor > begin && cursor < global.size() && global[cursor] == 'H') {
            const auto length = std::stoull(global.substr(begin, cursor - begin));
            ++cursor;
            require(length <= global.size() - cursor, "Truncated Hollerith");
            parameter = global.substr(cursor, length);
            cursor += length;
        } else {
            cursor = begin;
            while (cursor < global.size() && global[cursor] != ',' && global[cursor] != ';') { ++cursor; }
            parameter = global.substr(begin, cursor - begin);
        }
        require(cursor < global.size() && (global[cursor] == ',' || global[cursor] == ';'),
                "Bad IGES parameter delimiter");
        result.push_back(parameter);
        if (global[cursor++] == ';') { break; }
    }
    require(result.size() == 26, "Expected 26 IGES global parameters");
    return result;
}

void verify_timestamp(const std::string& timestamp, bool step, const std::string& expected = "") {
    if (!expected.empty()) {
        require(timestamp == expected, "Timestamp override differs: " + timestamp + " != " + expected);
        return;
    }
    const std::regex format(step ? R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$)" :
                                  R"(^[0-9]{8}\.[0-9]{6}$)");
    require(std::regex_match(timestamp, format), "Malformed default timestamp: " + timestamp);
    std::tm parsed{};
    std::istringstream stream(timestamp);
    stream >> std::get_time(&parsed, step ? "%Y-%m-%dT%H:%M:%SZ" : "%Y%m%d.%H%M%S");
    require(!stream.fail(), "Cannot parse default timestamp");
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto utc = *std::gmtime(&now);
    auto current = utc;
    const auto difference = std::difftime(std::mktime(&parsed), std::mktime(&current));
    require(std::abs(difference) < 180, "Default timestamp is not within three minutes of current UTC");
}

std::string escaped(const std::string& text) {
    const std::vector<std::pair<std::string, std::string>> encodings{
        {"致知屋", "\\X2\\81F477E55C4B\\X0\\"}, {"测试公司", "\\X2\\6D4B8BD5516C53F8\\X0\\"},
        {"测试产品", "\\X2\\6D4B8BD54EA754C1\\X0\\"}, {"测试描述", "\\X2\\6D4B8BD563CF8FF0\\X0\\"},
        {"中文", "\\X2\\4E2D6587\\X0\\"}, {"😀𠮷", "\\X4\\0001F60000020BB7\\X0\\"}};
    std::string result;
    for (std::size_t cursor = 0; cursor < text.size();) {
        bool replaced = false;
        for (const auto& encoding : encodings) {
            if (text.compare(cursor, encoding.first.size(), encoding.first) == 0) {
                result += encoding.second;
                cursor += encoding.first.size();
                replaced = true;
                break;
            }
        }
        if (replaced) { continue; }
        const char character = text[cursor++];
        if (character == '\'' || character == '\\') { result += character; }
        result += character;
    }
    return result;
}

void verify(const std::filesystem::path& path, bool step, const Metadata& expected, const std::string& timestamp = "",
            const std::string& iges_timestamp = "") {
    const auto file = contents(path);
    for (const auto* forbidden : {"VINCENT", "STL2STP", "STL TO STEP FILE", "Open CASCADE STEP processor",
                                  "Open CASCADE IGES processor"}) {
        require(file.find(forbidden) == std::string::npos, std::string("Unexpected metadata: ") + forbidden);
    }
    if (step) {
        STEPControl_Reader reader;
        require(reader.ReadFile(path.u8string().c_str()) == IFSelect_RetDone, "STEP read failed");
        const auto model = reader.StepModel();
        APIHeaderSection_MakeHeader header(model);
        require(value(header.Name()) == path.filename().u8string(), "STEP filename differs");
        require(header.NbAuthor() == 1 && value(header.AuthorValue(1)) == expected.author, "STEP author differs");
        require(header.NbOrganization() == 1 && value(header.OrganizationValue(1)) == expected.organization,
                "STEP organization differs");
        require(value(header.OriginatingSystem()) == expected.product, "STEP originating system differs");
        require(header.NbDescription() == 1 && value(header.DescriptionValue(1)) == expected.description,
                "STEP description differs");
        require(value(header.PreprocessorVersion()).empty() && value(header.Authorisation()).empty(),
                "STEP optional fields must be empty");
        verify_timestamp(value(header.TimeStamp()), true, timestamp);
        int products = 0;
        for (int index = 1; index <= model->NbEntities(); ++index) {
            const auto product = Handle(StepBasic_Product)::DownCast(model->Value(index));
            if (!product.IsNull()) {
                ++products;
                require(value(product->Name()) == expected.product && value(product->Id()) == expected.product,
                        "STEP PRODUCT name/id differs (including empty fallback)");
            }
        }
        require(products > 0, "STEP has no PRODUCT");
        if (expected.author.empty()) {
            require(file.find("(''),('')") != std::string::npos, "STEP author/org are not serialized empty");
        }
        std::string compact = file;
        compact.erase(std::remove(compact.begin(), compact.end(), '\n'), compact.end());
        compact.erase(std::remove(compact.begin(), compact.end(), '\r'), compact.end());
        for (const auto* field : {&expected.author, &expected.organization, &expected.product, &expected.description}) {
            require(compact.find("'" + escaped(*field) + "'") != std::string::npos, "STEP exact string bytes differ: " + *field);
        }
        if (!expected.product.empty()) {
            const std::string prefix = " = PRODUCT(";
            const auto begin = compact.find(prefix);
            require(begin != std::string::npos, "STEP has no serialized PRODUCT");
            const std::string name = "'" + escaped(expected.product) + "'";
            const auto first = compact.find_first_not_of(" \t", begin + prefix.size());
            require(compact.compare(first, name.size(), name) == 0, "STEP exact PRODUCT id bytes differ");
            require(compact[first + name.size()] == ',', "STEP PRODUCT parameter delimiter differs");
            const auto next = compact.find_first_not_of(" \t", first + name.size() + 1);
            require(compact.compare(next, name.size(), name) == 0, "STEP exact PRODUCT name bytes differ");
        }
        if (expected.author == "致知屋" || expected.author.find("中文") != std::string::npos) {
            require(file.find(expected.author) == std::string::npos, "STEP leaked raw UTF-8 metadata");
        }
    } else {
        const auto parameters = iges_parameters(file);
        const Metadata original_expected = expected;
        const Metadata ascii{iges_ascii(expected.author), iges_ascii(expected.organization), iges_ascii(expected.product),
                             iges_ascii(expected.description)};
        const auto& expected = ascii;
        const std::string filename = iges_ascii(path.filename().u8string());
        require(ascii_only(file), "IGES file must be pure ASCII");
        require(parameters[2] == expected.product && parameters[4] == expected.product, "IGES send/system differs");
        require(parameters[3] == filename, "IGES filename differs");
        require(parameters[20] == expected.author && parameters[21] == expected.organization,
                "IGES author/company differs: " + std::to_string(parameters[20].size()) + "/" +
                std::to_string(expected.author.size()) + " bytes; company=" + parameters[21]);
        for (const auto index : {5, 11, 25}) {
            require(parameters[index].empty(), "IGES optional field is not empty: " + std::to_string(index + 1));
        }
        require(timestamp.empty() == iges_timestamp.empty(), "Missing explicit IGES timestamp expectation");
        verify_timestamp(parameters[17], false, iges_timestamp);
        require(parameters[24] == parameters[17], "IGES generation and modification dates differ");
        std::string global;
        std::istringstream records(file);
        std::string record;
        while (std::getline(records, record)) { if (record[72] == 'G') { global += record.substr(0, 72); } }
        require(global.find("15H" + parameters[17]) != std::string::npos &&
                global.rfind("15H" + parameters[17]) != global.find("15H" + parameters[17]),
                "IGES dates must both be four-digit-year Hollerith strings");
        if (original_expected.author == "致知屋") {
            require(file.find("3H???") != std::string::npos && file.find("4H????") != std::string::npos,
                    "IGES must replace non-ASCII metadata with ASCII '?' and count bytes");
        }
        IGESControl_Reader reader;
        require(reader.ReadFile(path.u8string().c_str()) == IFSelect_RetDone, "IGES read failed");
        const auto model = reader.IGESModel();
        Handle(Interface_Check) check = new Interface_Check;
        model->VerifyCheck(check);
        require(check->NbFails() == 0, "OCCT IGES global-section validation failed");
        const auto& section = model->GlobalSection();
        require(value(section.Date()) == parameters[17] && value(section.LastChangeDate()) == parameters[24],
                "OCCT IGES date readback differs");
        require(value(section.AuthorName()) == expected.author && value(section.CompanyName()) == expected.organization,
                "OCCT IGES author/company readback differs");
        require(value(section.SendName()) == expected.product && value(section.SystemId()) == expected.product,
                "OCCT IGES send/system readback differs");
        require(value(section.FileName()) == filename, "OCCT IGES filename readback differs");
        std::string description;
        for (int index = 1; index <= model->NbStartLines(); ++index) { description += model->StartLine(index); }
        const auto last = description.find_last_not_of(' ');
        description = last == std::string::npos ? "" : description.substr(0, last + 1);
        require(description == expected.description, "OCCT IGES Start description differs");
    }
}

Handle load(const std::filesystem::path& input) {
    Handle handle(stl2stp_create(), &stl2stp_destroy);
    require(handle != nullptr, "Cannot create handle");
    require(stl2stp_load_stl(handle.get(), input.u8string().c_str(), nullptr) == STL2STP_OK, "Cannot load cube");
    return handle;
}

void export_file(stl2stp_handle* handle, const std::filesystem::path& path, bool step,
                 const stl2stp_export_options* options, const stl2stp_callbacks* callbacks = nullptr,
                 stl2stp_status expected = STL2STP_OK) {
    if (!step && expected == STL2STP_OK) {
        bool non_ascii = !ascii_only(path.filename().u8string());
        if (options) {
            for (const char* field : {options->author, options->organization, options->product, options->description}) {
                non_ascii = non_ascii || (field && !ascii_only(field));
            }
        }
        if (non_ascii) { expected = STL2STP_WARNING_METADATA_CHANGED; }
    }
    const auto status = step ? stl2stp_export_step(handle, path.u8string().c_str(), options, callbacks, nullptr) :
        stl2stp_export_iges(handle, path.u8string().c_str(), options, callbacks, nullptr);
    require(status == expected, "Export status " + std::to_string(status) + ": " + stl2stp_last_error(handle));
}

class AmbientSettings {
public:
    AmbientSettings() {
        STEPControl_Controller::Init();
        IGESControl_Controller::Init();
        for (const auto* name : {"write.step.product.name", "write.iges.header.author", "write.iges.header.company",
                                "write.iges.header.product", "write.iges.header.receiver"}) {
            names_.push_back(name);
            previous_.emplace_back(Interface_Static::CVal(name));
            require(Interface_Static::SetCVal(name, "ambient metadata"), "Cannot seed global setting");
        }
        for (const auto* name : {"write.step.schema", "write.step.unit"}) {
            names_.push_back(name);
            previous_.emplace_back(Interface_Static::CVal(name));
        }
    }

    ~AmbientSettings() {
        for (std::size_t index = 0; index < names_.size(); ++index) {
            Interface_Static::SetCVal(names_[index].c_str(), previous_[index].c_str());
        }
    }

    void verify_restored() const {
        for (std::size_t index = 0; index < names_.size(); ++index) {
            const auto expected = index < 5 ? "ambient metadata" : previous_[index];
            require(Interface_Static::CVal(names_[index].c_str()) == expected, "Global setting leaked: " + names_[index]);
        }
    }

private:
    std::vector<std::string> names_;
    std::vector<std::string> previous_;
};

struct CopyState {
    Metadata* source;
    std::string* timestamp;
    bool changed = false;
};

void change_source(double, void* context) {
    auto& state = *static_cast<CopyState*>(context);
    if (!state.changed) {
        *state.source = {"changed author", "changed organization", "changed product", "changed description"};
        *state.timestamp = "invalid timestamp after callback";
        state.changed = true;
    }
}

void track_progress(double fraction, void* context) { *static_cast<double*>(context) = fraction; }
int cancel_after_transfer(void* context) { return *static_cast<double*>(context) >= 0.88; }

}

int main(int argc, char** argv) {
    try {
        require(argc == 6, "Expected scenario, mode, format, input, output");
        const std::string scenario = argv[1];
        const auto mode = std::string(argv[2]) == "original" ? STL2STP_MODE_ORIGINAL : STL2STP_MODE_SEWING;
        const bool step = std::string(argv[3]) == "step";
        const auto output = std::filesystem::u8path(argv[5]);
        const Metadata custom{"致知屋", "测试公司", "测试产品", "测试描述"};
        if (scenario == "cli") {
            verify(output, step, custom);
            return 0;
        }
        if (scenario == "cli_timestamp") {
            verify(output, step, custom, "2024-02-29T10:52:00.123Z", "20240229.105200");
            return 0;
        }
        std::filesystem::create_directories(output);
        auto handle = load(std::filesystem::u8path(argv[4]));
        AmbientSettings settings;
        auto path = [&](const std::string& name) { return output / (name + (step ? ".step" : ".igs")); };
        if (scenario == "default") {
            auto options = stl2stp_default_export_options();
            options.mode = mode;
            export_file(handle.get(), path("default"), step, &options);
            verify(path("default"), step, {});
        } else if (scenario == "timestamp") {
            const struct { const char* iso; const char* iges; const char* step; } cases[] = {
                {"2024-02-29T18:52:00.123+08:00", "20240229.105200", "2024-02-29T10:52:00.123Z"},
                {nullptr, "", ""},
                {"2026-10-20T18:52:00+08:00", "20261020.105200", "2026-10-20T10:52:00Z"},
                {"2026-10-20T01:00:00+08:00", "20261019.170000", "2026-10-19T17:00:00Z"},
                {"2024-03-01T02:00:00+05:00", "20240229.210000", "2024-02-29T21:00:00Z"},
                {"2026-12-31T22:00:00-05:00", "20270101.030000", "2027-01-01T03:00:00Z"},
                {"2026-10-20T18:52:00-05:30", "20261021.002200", "2026-10-21T00:22:00Z"},
                {"2026-10-20T18:52:00", "20261020.185200", "2026-10-20T18:52:00Z"},
                {"2026-10-20T18:52:00Z", "20261020.185200", "2026-10-20T18:52:00Z"},
                {"2026-10-20T18:52:00.999", "20261020.185200", "2026-10-20T18:52:00.999Z"},
                {"2026-10-20T18:52:00.999Z", "20261020.185200", "2026-10-20T18:52:00.999Z"},
                {"2026-12-31T23:59:59.999-00:01", "20270101.000059", "2027-01-01T00:00:59.999Z"},
                {"1900-03-01T00:00:00+00:01", "19000228.235900", "1900-02-28T23:59:00Z"},
                {"2000-03-01T00:00:00+00:01", "20000229.235900", "2000-02-29T23:59:00Z"},
                {"2000-01-01T00:00:00Z", "20000101.000000", "2000-01-01T00:00:00Z"},
                {"0001-01-01T23:59:00+23:59", "00010101.000000", "0001-01-01T00:00:00Z"},
                {"9999-12-31T00:00:00-23:59", "99991231.235900", "9999-12-31T23:59:00Z"},
                {"2026-10-21T10:30:00+08:00", "20261021.023000", "2026-10-21T02:30:00Z"},
                {"2026-10-21T10:30:00-05:30", "20261021.160000", "2026-10-21T16:00:00Z"},
                {"2026-10-21T10:30:00Z", "20261021.103000", "2026-10-21T10:30:00Z"},
                {"2026-10-21T10:30:00+00:00", "20261021.103000", "2026-10-21T10:30:00Z"},
                {"2026-10-21T10:30:00-00:00", "20261021.103000", "2026-10-21T10:30:00Z"},
                {"2026-10-21T10:30:00", "20261021.103000", "2026-10-21T10:30:00Z"},
                {"2026-10-21T03:15:00.5+08:00", "20261020.191500", "2026-10-20T19:15:00.5Z"},
                {nullptr, "", ""}
            };
            for (const auto& entry : cases) {
                auto options = stl2stp_default_export_options();
                options.mode = mode;
                options.timestamp = entry.iso;
                export_file(handle.get(), path("timestamp"), step, &options);
                verify(path("timestamp"), step, {}, entry.step, entry.iges);
                settings.verify_restored();
            }
            auto options = stl2stp_default_export_options();
            options.mode = mode;
            for (const char* stamp : {"", "2026-10-20", "2026-13-01T00:00:00Z",
                    "2026-00-01T00:00:00Z", "2026-02-29T00:00:00Z", "1900-02-29T00:00:00Z",
                    "2026-04-31T00:00:00Z", "2026-01-00T00:00:00Z", "0000-01-01T00:00:00Z",
                    "2026-01-01T24:00:00Z", "2026-01-01T00:60:00Z", "2026-01-01T00:00:60Z",
                    "2026-01-01T00:00:00+24:00", "2026-01-01T00:00:00-08:60", "2026-01-01T00:00:00.Z",
                    "2026-01-01T00:00:00Zjunk", "2026-01-01T00:00:00'Z", "2026-1-1T00:00:00Z",
                    "2026-10-20T18:52:00+0800", "2026-10-20T18:52:00.",
                    "0001-01-01T00:00:00+00:01", "9999-12-31T23:59:00-00:01"}) {
                options.timestamp = stamp;
                const auto rejected = path("invalid-timestamp");
                { std::ofstream stream(rejected); stream << "unchanged"; }
                export_file(handle.get(), rejected, step, &options, nullptr, STL2STP_ERROR_ARGUMENT);
                require(contents(rejected) == "unchanged", "Invalid timestamp replaced destination");
                settings.verify_restored();
            }
        } else if (scenario == "encoding") {
            const std::string mixed = "中文😀𠮷'\\";
            for (const auto& metadata : {Metadata{mixed, mixed, mixed, mixed},
                    Metadata{"ASCII author'\\", "ASCII org'\\", "ASCII product'\\", "ASCII description'\\"},
                    Metadata{"literal \\X2\\0041\\X0\\", "literal \\X4\\0001F600\\X0\\",
                             "literal \\N\\ and \\T\\", "literal \\S\\ and \\PA\\"}}) {
                const auto options = metadata.options(mode);
                export_file(handle.get(), path("encoding"), step, &options);
                verify(path("encoding"), step, metadata);
            }
            const Metadata final{mixed, mixed, mixed, mixed};
            const auto final_options = final.options(mode);
            export_file(handle.get(), path("mixed"), step, &final_options);
            verify(path("mixed"), step, final);
            if (step) {
                std::string long_text;
                for (int index = 0; index < 10; ++index) { long_text += "中文😀𠮷'\\X2\\0041\\X0\\"; }
                const Metadata wrapped{long_text, long_text, long_text, long_text};
                const auto wrapped_options = wrapped.options(mode);
                export_file(handle.get(), path("wrapped"), step, &wrapped_options);
                verify(path("wrapped"), step, wrapped);
                for (const std::string& invalid : {std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
                                                  std::string("\xf4\x90\x80\x80"), std::string("\xe4\xb8")}) {
                    const Metadata bad{invalid, "", "", ""};
                    const auto bad_options = bad.options(mode);
                    export_file(handle.get(), path("invalid-utf8"), step, &bad_options, nullptr, STL2STP_ERROR_ARGUMENT);
                }
            }
        } else if (scenario == "custom" || scenario == "copy") {
            auto source = custom;
            std::string timestamp = "2024-02-29T18:52:00.123+08:00";
            auto options = source.options(mode);
            if (scenario == "copy") { options.timestamp = timestamp.c_str(); }
            CopyState state{&source, &timestamp};
            const stl2stp_callbacks callbacks{&change_source, nullptr, &state};
            export_file(handle.get(), path("custom"), step, &options, scenario == "copy" ? &callbacks : nullptr);
            verify(path("custom"), step, custom, scenario == "copy" ? "2024-02-29T10:52:00.123Z" : "",
                   scenario == "copy" ? "20240229.105200" : "");
            if (scenario == "copy") { require(state.changed, "Copy callback did not run"); }
        } else if (scenario == "isolation") {
            for (const auto& metadata : {custom, Metadata{"6Hliteral", "Other;Company,Inc", "Second'Product", "Other description"},
                                         Metadata{}}) {
                const auto options = metadata.options(mode);
                const auto destination = path(metadata.author.empty() ? "empty" : metadata.author);
                export_file(handle.get(), destination, step, &options);
                settings.verify_restored();
                verify(destination, step, metadata);
            }
            const auto options = custom.options(mode);
            export_file(handle.get(), path("custom"), step, &options);
            auto defaults = stl2stp_default_export_options();
            defaults.mode = mode;
            export_file(handle.get(), path("defaults"), step, &defaults);
            verify(path("defaults"), step, {});
            export_file(handle.get(), path("null-options"), step, nullptr);
            verify(path("null-options"), step, {});
        } else if (scenario == "cancel") {
            const auto options = custom.options(mode);
            const auto destination = path("cancelled");
            { std::ofstream stream(destination); stream << "unchanged"; }
            double fraction = 0;
            const stl2stp_callbacks callbacks{&track_progress, &cancel_after_transfer, &fraction};
            export_file(handle.get(), destination, step, &options, &callbacks, STL2STP_ERROR_CANCELLED);
            require(contents(destination) == "unchanged", "Cancelled export replaced target");
        } else if (scenario == "limits") {
            auto metadata = custom;
            metadata.author.assign(195, 'a');
            auto options = metadata.options(mode);
            export_file(handle.get(), path("boundary"), step, &options);
            verify(path("boundary"), step, metadata);
            metadata.author.push_back('a');
            options = metadata.options(mode);
            export_file(handle.get(), path("too-long"), step, &options, nullptr, STL2STP_ERROR_ARGUMENT);
            require(!std::filesystem::exists(path("too-long")), "Rejected export created output");
            for (int field = 0; field < 4; ++field) {
                metadata = custom;
                const auto fields = std::array<std::string*, 4>{&metadata.author, &metadata.organization,
                                                               &metadata.product, &metadata.description};
                *fields[field] = "line one\nline two";
                options = metadata.options(mode);
                export_file(handle.get(), path("newline"), step, &options, nullptr, STL2STP_ERROR_ARGUMENT);
            }
        } else if (scenario == "parallel") {
            auto second = load(std::filesystem::u8path(argv[4]));
            const Metadata other{"Other author", "Other company", "Other product", "Other description"};
            const auto first_options = custom.options(STL2STP_MODE_ORIGINAL);
            const auto second_options = other.options(STL2STP_MODE_SEWING);
            std::array<std::exception_ptr, 2> failures{};
            std::thread first([&] {
                try { export_file(handle.get(), path("first"), step, &first_options); }
                catch (...) { failures[0] = std::current_exception(); }
            });
            std::thread second_thread([&] {
                try { export_file(second.get(), path("second"), step, &second_options); }
                catch (...) { failures[1] = std::current_exception(); }
            });
            first.join();
            second_thread.join();
            for (const auto& failure : failures) { if (failure) { std::rethrow_exception(failure); } }
            verify(path("first"), step, custom);
            verify(path("second"), step, other);
        } else {
            throw std::runtime_error("Unknown metadata scenario: " + scenario);
        }
        settings.verify_restored();
        std::cout << "metadata " << scenario << " passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
