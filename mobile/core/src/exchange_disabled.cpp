#include "internal.hpp"

namespace {

stl2stp_status unavailable(stl2stp_handle* handle) {
    return stl2stp::protect(handle, []() -> stl2stp_status {
        throw stl2stp::Error(STL2STP_ERROR_OCCT, "This mesh-only build has no OCCT exchange support");
    });
}

}

extern "C" {

stl2stp_status stl2stp_export_step(stl2stp_handle* handle, const char*,
                                 const stl2stp_export_options*, const stl2stp_callbacks*, stl2stp_topology*) {
    return unavailable(handle);
}

stl2stp_status stl2stp_export_iges(stl2stp_handle* handle, const char*,
                                 const stl2stp_export_options*, const stl2stp_callbacks*, stl2stp_topology*) {
    return unavailable(handle);
}

stl2stp_status stl2stp_read_step(stl2stp_handle* handle, const char*, const stl2stp_callbacks*, stl2stp_topology*) {
    return unavailable(handle);
}

}
