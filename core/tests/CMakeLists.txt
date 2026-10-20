add_executable(stl2stp-fixture-generator fixtures.cpp)
target_compile_features(stl2stp-fixture-generator PRIVATE cxx_std_17)
set_target_properties(stl2stp-fixture-generator PROPERTIES CXX_EXTENSIONS OFF)

add_executable(stl2stp-tests test_core.cpp)
target_link_libraries(stl2stp-tests PRIVATE stl2stp-core)
target_compile_definitions(stl2stp-tests PRIVATE STL2STP_WITH_OCCT=$<BOOL:${STL2STP_WITH_OCCT}>)

add_executable(stl2stp-c-api-test test_c_api.c)
target_link_libraries(stl2stp-c-api-test PRIVATE stl2stp-core)
set_target_properties(stl2stp-c-api-test PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
add_test(NAME stl2stp.c_header COMMAND stl2stp-c-api-test)

set(_data "${CMAKE_CURRENT_BINARY_DIR}/data")
configure_file(data/small_zero-area-duplicate.stl "${_data}/zero-area-duplicate.stl" COPYONLY)
add_test(NAME stl2stp.fixtures COMMAND stl2stp-fixture-generator "${_data}")
set_tests_properties(stl2stp.fixtures PROPERTIES FIXTURES_SETUP stl2stp_data)

set(_mesh_tests load_binary load_ascii solid_header truncated stats errors simplify save unicode cancel_mesh)
if(NOT STL2STP_WITH_OCCT)
    list(APPEND _mesh_tests disabled_exchange)
endif()
set(_exchange_tests step step_merge readback two_shells iges exchange_errors cancel_exchange)
foreach(_mode original sewing)
    foreach(_fixture cube two_shells degenerate_duplicate duplicate_open collinear collinear_only truncated iges)
        list(APPEND _exchange_tests "${_mode}_${_fixture}")
    endforeach()
endforeach()
foreach(_test IN LISTS _mesh_tests)
    add_test(NAME stl2stp.${_test} COMMAND stl2stp-tests "${_test}" "${_data}" "${_data}/output-${_test}")
    set_tests_properties(stl2stp.${_test} PROPERTIES FIXTURES_REQUIRED stl2stp_data LABELS mesh TIMEOUT 120)
endforeach()
if(STL2STP_WITH_OCCT)
    find_package(Threads REQUIRED)
    add_executable(stl2stp-metadata-tests test_metadata.cpp)
    target_include_directories(stl2stp-metadata-tests SYSTEM PRIVATE "${OCCT_INCLUDE_DIR}")
    target_link_libraries(stl2stp-metadata-tests PRIVATE stl2stp-core Threads::Threads)
    foreach(_mode original sewing)
        foreach(_format step iges)
            foreach(_scenario default custom isolation copy cancel timestamp encoding)
                set(_test "metadata_${_scenario}_${_mode}_${_format}")
                add_test(NAME stl2stp.${_test} COMMAND stl2stp-metadata-tests ${_scenario} ${_mode} ${_format}
                    "${_data}/cube-binary.stl" "${_data}/${_test}")
                set_tests_properties(stl2stp.${_test} PROPERTIES FIXTURES_REQUIRED stl2stp_data LABELS "occt;metadata" TIMEOUT 180)
            endforeach()
        endforeach()
    endforeach()
    foreach(_format step iges)
        add_test(NAME stl2stp.metadata_parallel_${_format} COMMAND stl2stp-metadata-tests parallel original ${_format}
            "${_data}/cube-binary.stl" "${_data}/metadata_parallel_${_format}")
        set_tests_properties(stl2stp.metadata_parallel_${_format} PROPERTIES FIXTURES_REQUIRED stl2stp_data LABELS "occt;metadata" TIMEOUT 180)
    endforeach()
    add_test(NAME stl2stp.metadata_iges_limits COMMAND stl2stp-metadata-tests limits original iges
        "${_data}/cube-binary.stl" "${_data}/metadata_iges_limits")
    set_tests_properties(stl2stp.metadata_iges_limits PROPERTIES FIXTURES_REQUIRED stl2stp_data LABELS "occt;metadata" TIMEOUT 180)
    foreach(_test IN LISTS _exchange_tests)
        add_test(NAME stl2stp.${_test} COMMAND stl2stp-tests "${_test}" "${_data}" "${_data}/output-${_test}")
        set_tests_properties(stl2stp.${_test} PROPERTIES FIXTURES_REQUIRED stl2stp_data LABELS occt TIMEOUT 180)
    endforeach()
endif()

if(STL2STP_BUILD_CLI)
add_test(NAME stl2stp.cli_info COMMAND stl2stp-cli info "${_data}/cube-binary.stl")
add_test(NAME stl2stp.cli_simplify COMMAND stl2stp-cli simplify "${_data}/sphere.stl" "${_data}/cli-simplified.stl" --faces 2000)
add_test(NAME stl2stp.cli_invalid COMMAND stl2stp-cli simplify "${_data}/cube-binary.stl" "${_data}/invalid-output.stl" --faces -1)
set_tests_properties(stl2stp.cli_invalid PROPERTIES WILL_FAIL TRUE)
set_tests_properties(stl2stp.cli_info PROPERTIES PASS_REGULAR_EXPRESSION "vertices=8 edges=18 faces=12")
set_tests_properties(stl2stp.cli_simplify PROPERTIES PASS_REGULAR_EXPRESSION "output_bytes=" TIMEOUT 120)
set_tests_properties(stl2stp.cli_info stl2stp.cli_simplify stl2stp.cli_invalid PROPERTIES FIXTURES_REQUIRED stl2stp_data)

if(STL2STP_WITH_OCCT)
    foreach(_mode original sewing)
        foreach(_format step iges)
            set(_test "cli_metadata_${_mode}_${_format}")
            add_test(NAME stl2stp.${_test} COMMAND stl2stp-cli convert "${_data}/cube-binary.stl"
                "${_data}/${_test}.${_format}" --mode ${_mode} --author "致知屋" --org "测试公司"
                --product "测试产品" --description "测试描述")
            set_tests_properties(stl2stp.${_test} PROPERTIES FIXTURES_REQUIRED stl2stp_data FIXTURES_SETUP ${_test}
                LABELS metadata TIMEOUT 180)
            add_test(NAME stl2stp.${_test}_read COMMAND stl2stp-metadata-tests cli ${_mode} ${_format}
                "${_data}/cube-binary.stl" "${_data}/${_test}.${_format}")
            set_tests_properties(stl2stp.${_test}_read PROPERTIES FIXTURES_REQUIRED ${_test} LABELS metadata TIMEOUT 180)
        endforeach()
    endforeach()
    foreach(_mode original sewing)
        foreach(_format step iges)
            set(_test "cli_timestamp_${_mode}_${_format}")
            add_test(NAME stl2stp.${_test} COMMAND stl2stp-cli convert "${_data}/cube-binary.stl"
                "${_data}/${_test}.${_format}" --mode ${_mode} --author "致知屋" --org "测试公司"
                --product "测试产品" --description "测试描述" --timestamp "2024-02-29T18:52:00.123+08:00")
            set_tests_properties(stl2stp.${_test} PROPERTIES FIXTURES_REQUIRED stl2stp_data FIXTURES_SETUP ${_test}
                LABELS metadata TIMEOUT 180)
            add_test(NAME stl2stp.${_test}_read COMMAND stl2stp-metadata-tests cli_timestamp ${_mode} ${_format}
                "${_data}/cube-binary.stl" "${_data}/${_test}.${_format}")
            set_tests_properties(stl2stp.${_test}_read PROPERTIES FIXTURES_REQUIRED ${_test} LABELS metadata TIMEOUT 180)
        endforeach()
    endforeach()
    add_test(NAME stl2stp.cli_timestamp_invalid COMMAND stl2stp-cli convert "${_data}/cube-binary.stl"
        "${_data}/invalid-timestamp.step" --timestamp "2026-02-29T00:00:00Z")
    set_tests_properties(stl2stp.cli_timestamp_invalid PROPERTIES FIXTURES_REQUIRED stl2stp_data WILL_FAIL TRUE LABELS metadata)
    foreach(_flag author org product description timestamp)
        foreach(_error missing duplicate)
            set(_args --${_flag})
            if(_error STREQUAL duplicate)
                set(_args --${_flag} "" --${_flag} second)
            endif()
            add_test(NAME stl2stp.cli_metadata_${_flag}_${_error} COMMAND stl2stp-cli convert "${_data}/cube-binary.stl"
                "${_data}/invalid-metadata.step" ${_args})
            set_tests_properties(stl2stp.cli_metadata_${_flag}_${_error} PROPERTIES FIXTURES_REQUIRED stl2stp_data
                WILL_FAIL TRUE LABELS metadata)
        endforeach()
    endforeach()
    add_test(NAME stl2stp.cli_convert COMMAND stl2stp-cli convert "${_data}/cube-binary.stl" "${_data}/cli-cube.step" --merge-coplanar)
    set_tests_properties(stl2stp.cli_convert PROPERTIES FIXTURES_REQUIRED stl2stp_data FIXTURES_SETUP cli_step
        PASS_REGULAR_EXPRESSION "mode=original" TIMEOUT 180)
    add_test(NAME stl2stp.cli_readback COMMAND stl2stp-cli readback "${_data}/cli-cube.step")
    set_tests_properties(stl2stp.cli_readback PROPERTIES FIXTURES_REQUIRED cli_step PASS_REGULAR_EXPRESSION "faces=6")
    add_test(NAME stl2stp.cli_iges COMMAND stl2stp-cli convert "${_data}/cube-binary.stl" "${_data}/cli-cube.igs" --simplify 8)
    set_tests_properties(stl2stp.cli_iges PROPERTIES FIXTURES_REQUIRED stl2stp_data PASS_REGULAR_EXPRESSION "output_bytes=" TIMEOUT 180)
    foreach(_mode original sewing)
        add_test(NAME stl2stp.cli_mode_${_mode} COMMAND stl2stp-cli convert "${_data}/two-shells.stl"
            "${_data}/cli-${_mode}.step" --mode ${_mode})
        set_tests_properties(stl2stp.cli_mode_${_mode} PROPERTIES FIXTURES_REQUIRED stl2stp_data
            PASS_REGULAR_EXPRESSION "mode=${_mode}" TIMEOUT 180)
    endforeach()
    add_test(NAME stl2stp.cli_mode_invalid COMMAND stl2stp-cli convert "${_data}/cube-binary.stl"
        "${_data}/invalid-mode.step" --mode invalid)
    add_test(NAME stl2stp.cli_mode_missing COMMAND stl2stp-cli convert "${_data}/cube-binary.stl"
        "${_data}/missing-mode.step" --mode)
    add_test(NAME stl2stp.cli_mode_duplicate COMMAND stl2stp-cli convert "${_data}/cube-binary.stl"
        "${_data}/duplicate-mode.step" --mode original --mode sewing)
    set_tests_properties(stl2stp.cli_mode_invalid stl2stp.cli_mode_missing stl2stp.cli_mode_duplicate PROPERTIES
        FIXTURES_REQUIRED stl2stp_data WILL_FAIL TRUE)
endif()
endif()

if(STL2STP_WITH_OCCT)
    # Regression tests for the code-review findings (see docs/core-validation.md, "Review findings").
    add_executable(stl2stp-review-tests test_review.cpp)
    target_include_directories(stl2stp-review-tests SYSTEM PRIVATE "${OCCT_INCLUDE_DIR}")
    target_link_libraries(stl2stp-review-tests PRIVATE stl2stp-core)
    foreach(_case original_closed_cube original_open_box original_flipped_triangle original_inverted
            original_non_manifold_edge original_collinear_sliver original_disconnected
            qem_open_boundary qem_link_condition qem_scale_invariance merge_rotated malformed_fuzz)
        add_test(NAME stl2stp.review_${_case} COMMAND stl2stp-review-tests ${_case} "${_data}/review-${_case}")
        set_tests_properties(stl2stp.review_${_case} PROPERTIES LABELS "review" TIMEOUT 300)
    endforeach()
endif()
