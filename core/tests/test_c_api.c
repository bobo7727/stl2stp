#include <stl2stp/stl2stp.h>

int main(void) {
    stl2stp_handle* handle = stl2stp_create();
    stl2stp_export_options options = stl2stp_default_export_options();
    stl2stp_mesh_stats stats = {0};
    int valid = handle != 0 && options.merge_coplanar == 0 && options.sewing_tolerance > 0
        && options.mode == STL2STP_MODE_ORIGINAL && STL2STP_MODE_SEWING == 1
        && !options.author && !options.organization && !options.product && !options.description
        && sizeof(stl2stp_mode) == sizeof(int32_t);
    if (handle) {
        valid = valid && stl2stp_get_mesh_stats(handle, &stats) == STL2STP_ERROR_EMPTY;
        valid = valid && stl2stp_last_error(handle)[0] != '\0';
    }
    stl2stp_destroy(handle);
    stl2stp_destroy(0);
    return valid ? 0 : 1;
}
