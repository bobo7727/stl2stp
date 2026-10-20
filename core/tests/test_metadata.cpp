#include <stl2stp/stl2stp.h>

#include <APIHeaderSection_MakeHeader.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESData_GlobalSection.hxx>
#include <IGESData_IGESModel.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_Controller.hxx>
#include <STEPControl_Reader.hxx>
#include <StepBasic_Product.hxx>
#include <StepData_StepModel.hxx>
#include <TCollection_HAsciiString.hxx>

#include <array>
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

void verify(const std::filesystem::path& path, bool step, const Metadata& expected) {
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
        require(value(header.PreprocessorVersion()).empty() && value(header.Authorisation()).empty() &&
                value(header.TimeStamp()).empty(), "STEP optional fields must be empty");
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
        if (expected.author == "致知屋") {
            require(file.find("'致知屋'") != std::string::npos && file.find("'测试公司'") != std::string::npos &&
                    file.find("\\X2\\") == std::string::npos, "Expected raw UTF-8 STEP encoding");
        }
    } else {
        const auto parameters = iges_parameters(file);
        require(parameters[2] == expected.product && parameters[4] == expected.product, "IGES send/system differs");
        require(parameters[3] == path.filename().u8string(), "IGES filename differs");
        require(parameters[20] == expected.author && parameters[21] == expected.organization,
                "IGES author/company differs: " + std::to_string(parameters[20].size()) + "/" +
                std::to_string(expected.author.size()) + " bytes; company=" + parameters[21]);
        for (const auto index : {5, 11, 17, 24, 25}) {
            require(parameters[index].empty(), "IGES optional field is not empty: " + std::to_string(index + 1));
        }
        if (expected.author == "致知屋") {
            require(file.find("9H致知屋") != std::string::npos && file.find("12H测试公司") != std::string::npos,
                    "IGES must count UTF-8 bytes, not Unicode characters");
        }
        IGESControl_Reader reader;
        require(reader.ReadFile(path.u8string().c_str()) == IFSelect_RetDone, "IGES read failed");
        const auto model = reader.IGESModel();
        const auto& section = model->GlobalSection();
        require(value(section.AuthorName()) == expected.author && value(section.CompanyName()) == expected.organization,
                "OCCT IGES author/company readback differs");
        require(value(section.SendName()) == expected.product && value(section.SystemId()) == expected.product,
                "OCCT IGES send/system readback differs");
        require(value(section.FileName()) == path.filename().u8string(), "OCCT IGES filename readback differs");
        std::string description;
        for (int index = 1; index <= model->NbStartLines(); ++index) { description += model->StartLine(index); }
        const auto last = description.find_last_not_of(' ');
        description = last == std::string::npos ? "" : description.substr(0, last + 1);
        if (expected.description == "测试描述") {
            require(file.substr(0, expected.description.size()) == expected.description,
                    "IGES Start must preserve raw UTF-8 description");
            require(description.empty(), "Expected OCCT signed-char Start trimming limitation for Chinese");
        } else {
            require(description == expected.description, "OCCT IGES Start description differs");
        }
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
    bool changed = false;
};

void change_source(double, void* context) {
    auto& state = *static_cast<CopyState*>(context);
    if (!state.changed) {
        *state.source = {"changed author", "changed organization", "changed product", "changed description"};
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
        std::filesystem::create_directories(output);
        auto handle = load(std::filesystem::u8path(argv[4]));
        AmbientSettings settings;
        auto path = [&](const std::string& name) { return output / (name + (step ? ".step" : ".igs")); };
        if (scenario == "default") {
            auto options = stl2stp_default_export_options();
            options.mode = mode;
            export_file(handle.get(), path("default"), step, &options);
            verify(path("default"), step, {});
        } else if (scenario == "custom" || scenario == "copy") {
            auto source = custom;
            const auto options = source.options(mode);
            CopyState state{&source};
            const stl2stp_callbacks callbacks{&change_source, nullptr, &state};
            export_file(handle.get(), path("custom"), step, &options, scenario == "copy" ? &callbacks : nullptr);
            verify(path("custom"), step, custom);
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
