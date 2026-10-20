#include "internal.hpp"

#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepLib.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Writer.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <Message_ProgressIndicator.hxx>
#include <Message_ProgressScope.hxx>
#include <NCollection_IndexedMap.hxx>
#include <STEPControl_Controller.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>
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
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>

#include <chrono>
#include <exception>
#include <fstream>
#include <mutex>

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

TopoDS_Shape make_shape(const Mesh& mesh, const stl2stp_export_options& options, Progress& progress) {
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
    if (options.merge_coplanar) {
        ShapeUpgrade_UnifySameDomain unify(sewn, true, true, false);
        unify.SetLinearTolerance(options.sewing_tolerance);
        unify.Build();
        progress.check();
        sewn = unify.Shape();
        if (sewn.IsNull()) {
            throw Error(STL2STP_ERROR_OCCT, "Coplanar merge produced no shape");
        }
    }
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
        Progress progress(callbacks);
        progress.report(0);
        auto lock = exchange_lock(progress);
        const auto shape = make_shape(handle->mesh, options, progress);
        const auto counts = count_topology(shape);
        OutputFile output(path);
        const std::string temporary = output.temporary().u8string();
        Handle(Indicator) transfer_progress = new Indicator(progress, 0.7, 0.18);
        if (step) {
            STEPControl_Controller::Init();
            StaticSetting schema("write.step.schema", "AP214IS");
            StaticSetting unit("write.step.unit", "MM");
            StaticSetting product("write.step.product.name", "STL2STP");
            STEPControl_Writer writer;
            const auto transferred = writer.Transfer(shape, STEPControl_AsIs, true, transfer_progress->Start());
            transfer_progress->checkpoint();
            if (transferred != IFSelect_RetDone) {
                throw Error(STL2STP_ERROR_OCCT, "STEP transfer failed");
            }
            progress.report(0.88);
            if (writer.Write(temporary.c_str()) != IFSelect_RetDone) {
                throw Error(STL2STP_ERROR_IO, "STEP write failed");
            }
        } else {
            IGESControl_Controller::Init();
            IGESControl_Writer writer("MM", Interface_Static::IVal("write.brep.mode"));
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
