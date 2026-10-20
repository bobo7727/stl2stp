#ifndef STL2STP_H
#define STL2STP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stl2stp_handle stl2stp_handle;

typedef enum stl2stp_status {
    STL2STP_OK = 0,
    STL2STP_WARNING_PARTIAL = 1,
    STL2STP_WARNING_TARGET_NOT_REACHED = 2,
    /* Appended in core/fixes (values are stable; never renumber). */
    STL2STP_WARNING_NOT_SOLID = 3,          /* open/non-closable input: written as shell(s)/surfaces, no solid */
    STL2STP_WARNING_REPAIRED = 4,           /* output is valid but input was repaired (see stl2stp_last_error) */
    STL2STP_WARNING_METADATA_CHANGED = 5,   /* IGES metadata was not ASCII and was rewritten to ASCII */
    STL2STP_ERROR_ARGUMENT = -1,
    STL2STP_ERROR_IO = -2,
    STL2STP_ERROR_FORMAT = -3,
    STL2STP_ERROR_EMPTY = -4,
    STL2STP_ERROR_CANCELLED = -5,
    STL2STP_ERROR_OCCT = -6,
    STL2STP_ERROR_MEMORY = -7,
    STL2STP_ERROR_INTERNAL = -8
} stl2stp_status;

typedef void (*stl2stp_progress_callback)(double fraction, void* user_data);
typedef int (*stl2stp_cancel_callback)(void* user_data);

typedef struct stl2stp_callbacks {
    stl2stp_progress_callback progress;
    stl2stp_cancel_callback cancelled;
    void* user_data;
} stl2stp_callbacks;

typedef struct stl2stp_mesh_stats {
    uint64_t vertices;
    uint64_t edges;
    uint64_t faces;
    double minimum[3];
    double maximum[3];
} stl2stp_mesh_stats;

typedef struct stl2stp_topology {
    uint64_t solids;
    uint64_t shells;
    uint64_t faces;
    uint64_t edges;
    uint64_t vertices;
} stl2stp_topology;

typedef enum stl2stp_mode
#ifdef __cplusplus
    : int32_t
#endif
{
    STL2STP_MODE_ORIGINAL = 0,
    STL2STP_MODE_SEWING = 1
} stl2stp_mode;

/* Always start from stl2stp_default_export_options() so fields appended in later versions get defaults. */
typedef struct stl2stp_export_options {
    double sewing_tolerance;
    int merge_coplanar;
    stl2stp_mode mode;
    const char* author;
    const char* organization;
    const char* product;
    const char* description;
    const char* timestamp;
    /* Appended in core/fixes. */
    double merge_angular_tolerance; /* radians, coplanar merge; default 1e-4, valid range (0, 0.1] */
    int strict_original;            /* 1 = legacy ORIGINAL output without validation/repair (may be invalid); default 0 */
} stl2stp_export_options;

stl2stp_handle* stl2stp_create(void);
void stl2stp_destroy(stl2stp_handle* handle);
const char* stl2stp_last_error(const stl2stp_handle* handle);
stl2stp_export_options stl2stp_default_export_options(void);
stl2stp_status stl2stp_load_stl(stl2stp_handle* handle, const char* path,
                               const stl2stp_callbacks* callbacks);
stl2stp_status stl2stp_get_mesh_stats(stl2stp_handle* handle, stl2stp_mesh_stats* stats);
stl2stp_status stl2stp_simplify(stl2stp_handle* handle, uint64_t target_faces,
                               const stl2stp_callbacks* callbacks);
stl2stp_status stl2stp_save_stl(stl2stp_handle* handle, const char* path,
                               const stl2stp_callbacks* callbacks);
stl2stp_status stl2stp_export_step(stl2stp_handle* handle, const char* path,
                                 const stl2stp_export_options* options,
                                 const stl2stp_callbacks* callbacks,
                                 stl2stp_topology* topology);
stl2stp_status stl2stp_export_iges(stl2stp_handle* handle, const char* path,
                                 const stl2stp_export_options* options,
                                 const stl2stp_callbacks* callbacks,
                                 stl2stp_topology* topology);
stl2stp_status stl2stp_read_step(stl2stp_handle* handle, const char* path,
                               const stl2stp_callbacks* callbacks,
                               stl2stp_topology* topology);

#ifdef __cplusplus
}
#endif

#endif
