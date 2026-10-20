#include "internal.hpp"

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
#include <TCollection_HAsciiString.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
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
#include <exception>
#include <fstream>
#include <memory>
#include <mutex>
#include <map>

namespace stl2stp {
namespace {

std::timed_mutex exchange_mutex;

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

TopoDS_Shape merge_shape(const TopoDS_Shape& shape, const stl2stp_export_options& options, Progress& progress) {
    if (!options.merge_coplanar) { return shape; }
    ShapeUpgrade_UnifySameDomain unify(shape, true, true, false);
    unify.SetLinearTolerance(options.sewing_tolerance);
    unify.Build();
    progress.check();
    if (unify.Shape().IsNull()) {
        throw Error(STL2STP_ERROR_OCCT, "Coplanar merge produced no shape");
    }
    return unify.Shape();
}

TopoDS_Shape make_original_shape(const Mesh& mesh, const stl2stp_export_options& options, Progress& progress) {
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
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    for (const auto& face : faces) {
        builder.Add(shell, face);
        progress.check();
    }
    BRepTools::Update(shell);
    TopoDS_Solid solid;
    builder.MakeSolid(solid);
    builder.Add(solid, shell);
    const auto shape = merge_shape(solid, options, progress);
    progress.report(0.7);
    return shape;
}

TopoDS_Shape make_sewing_shape(const Mesh& mesh, const stl2stp_export_options& options, Progress& progress) {
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
    sewn = merge_shape(sewn, options, progress);
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
        if (!std::isfinite(options.sewing_tolerance) || options.sewing_tolerance <= 0) {
            throw Error(STL2STP_ERROR_ARGUMENT, "Sewing tolerance must be finite and positive");
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
        const bool original = options.mode == STL2STP_MODE_ORIGINAL;
        const auto shape = original ? make_original_shape(handle->mesh, options, progress)
                                    : make_sewing_shape(handle->mesh, options, progress);
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
            STEPControl_Writer writer;
            StaticSetting product("write.step.product.name", original ? "STL2STP 1.0.0" : "STL2STP");
            const auto transferred = writer.Transfer(shape, STEPControl_AsIs, true, transfer_progress->Start());
            transfer_progress->checkpoint();
            if (transferred != IFSelect_RetDone) {
                throw Error(STL2STP_ERROR_OCCT, "STEP transfer failed");
            }
            progress.report(0.88);
            if (original) {
                APIHeaderSection_MakeHeader header(writer.Model(false));
                header.SetName(new TCollection_HAsciiString(std::filesystem::u8path(path).filename().u8string().c_str()));
                header.SetAuthorValue(1, new TCollection_HAsciiString("VINCENT"));
                header.SetOrganizationValue(1, new TCollection_HAsciiString("VINCENT"));
                header.SetOriginatingSystem(new TCollection_HAsciiString("STL2STP"));
                header.SetDescriptionValue(1, new TCollection_HAsciiString("STL TO STEP FILE"));
                header.SetPreprocessorVersion(new TCollection_HAsciiString("STL2STP STEP processor"));
            }
            if (writer.Write(temporary.c_str()) != IFSelect_RetDone) {
                throw Error(STL2STP_ERROR_IO, "STEP write failed");
            }
        } else {
            IGESControl_Controller::Init();
            IGESControl_Writer writer(original ? Interface_Static::CVal("write.iges.unit") : "MM",
                                     Interface_Static::IVal("write.brep.mode"));
            if (original) {
                IGESData_GlobalSection section(writer.Model()->GlobalSection());
                section.SetAuthorName(new TCollection_HAsciiString("VINCENT"));
                section.SetCompanyName(new TCollection_HAsciiString("VINCENT"));
                section.SetFileName(new TCollection_HAsciiString(std::filesystem::u8path(path).filename().u8string().c_str()));
                section.SetInterfaceVersion(new TCollection_HAsciiString("STL2STP IGES processor"));
                section.SetSendName(new TCollection_HAsciiString("STL2STP IGES processor"));
                section.SetSystemId(new TCollection_HAsciiString("STL2STP IGES processor"));
                writer.Model()->SetGlobalSection(section);
            }
            const bool added = writer.AddShape(shape, transfer_progress->Start());
            transfer_progress->checkpoint();
            if (!added) { throw Error(STL2STP_ERROR_OCCT, "IGES transfer failed"); }
            progress.report(0.88);
            writer.ComputeModel();
            progress.check();
            if (!writer.Write(temporary.c_str(), false)) {
                throw Error(STL2STP_ERROR_IO, "IGES write failed");
            }
        }
        progress.report(1);
        output.commit();
        if (topology) { *topology = counts; }
        return STL2STP_OK;
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
