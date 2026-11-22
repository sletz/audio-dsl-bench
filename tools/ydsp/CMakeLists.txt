# Static library wrapping the YUP YDSP JIT behind a plain C API (ydsp_engine.h), linked by the harness.
#   cmake -S tools/ydsp -B build/_ydsp -G Ninja -DCMAKE_BUILD_TYPE=Release -DYUP_ROOT=../yup
cmake_minimum_required (VERSION 3.31)
project (ydsp_engine VERSION 1.0)

if (NOT YUP_ROOT)
    get_filename_component (YUP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../yup" ABSOLUTE)
endif()
set (YUP_FETCH_UPSTREAM_MODULES ON CACHE BOOL "Fetch missing YUP module upstream sources" FORCE)
include ("${YUP_ROOT}/cmake/yup.cmake")

yup_add_module ("${YUP_ROOT}/modules/yup_core" "" "Modules")
yup_add_module ("${YUP_ROOT}/modules/yup_events" "" "Modules")
yup_add_module ("${YUP_ROOT}/modules/yup_dsp" "" "Modules")
yup_add_module ("${YUP_ROOT}/modules/yup_audio_basics" "" "Modules")
yup_add_module ("${YUP_ROOT}/modules/yup_dsp_jit" "" "Modules")
yup_add_module ("${YUP_ROOT}/modules/yup_simd" "" "Modules")
yup_add_module ("${YUP_ROOT}/thirdparty/asmjit_library" "" "Thirdparty")
yup_add_module ("${YUP_ROOT}/thirdparty/sleef_library" "" "Thirdparty")
yup_add_module ("${YUP_ROOT}/thirdparty/xsimd" "" "Thirdparty")
yup_add_module ("${YUP_ROOT}/thirdparty/zlib" "" "Thirdparty")

yup_standalone_app (
    TARGET_NAME ydsp_engine
    TARGET_VERSION 1.0
    TARGET_APP_ID "org.bench.ydsp_engine"
    TARGET_APP_NAMESPACE "org.bench"
    TARGET_CONSOLE ON
    TARGET_STATIC_LIBRARY ON
    MODULES
        yup_core
        yup_events
        yup_dsp
        yup_audio_basics
        yup_simd
        yup_dsp_jit
        asmjit_library
        xsimd)

target_sources (ydsp_engine PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ydsp_engine.cpp")
