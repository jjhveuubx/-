cmake_minimum_required(VERSION 3.21)

project(xai_type_aq LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(XAI_TYPE_AQ_BUILD_SHARED "Build xai_type_aq as a shared library for plugin loading" ON)

set(VENDOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/vendor")
set(VENDOR_AQ_SDK_DIR "${VENDOR_ROOT}/aq_sdk")
set(VENDOR_VISIONFLOW_DIR "${VENDOR_ROOT}/visionflow")
set(VENDOR_OPENCV_DIR "${VENDOR_ROOT}/opencv")

if(MSVC)
    add_compile_options(/utf-8 /EHsc /W3)
endif()

set(AQ_SDK_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../给阿丘/给阿丘"
    CACHE PATH "Path to the Aq SDK root")
set(VISIONFLOW_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../VisionFlow-0.9.1-release.daa9b734.0-msvcrt-x64_windows-opt"
    CACHE PATH "Path to the VisionFlow SDK root")
set(AQ_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../new" CACHE PATH "Path to the Aq SDK root" FORCE)
set(OPENCV_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../OpenCV-4.7.0"
    CACHE PATH "Path to the OpenCV build root")
set(OPENCV_INCLUDE_FALLBACK
    "C:/tools/opencv/build/include"
    CACHE PATH "Fallback OpenCV include directory when vendor package has no headers")
set(MSVC_REDIST_DIR
    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Redist/MSVC/14.29.30133/x64/Microsoft.VC142.CRT"
    CACHE PATH "Path to the MSVC CRT redistributable directory")
set(MSVC_OPENMP_REDIST_DIR
    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Redist/MSVC/14.29.30133/x64/Microsoft.VC142.OpenMP"
    CACHE PATH "Path to the MSVC OpenMP redistributable directory")

set(AQ_SDK_LIB "${AQ_SDK_ROOT}/xai_api.lib")
set(AQ_SDK_DEBUG_LIB "${AQ_SDK_ROOT}/xai_apid.lib")
set(VISIONFLOW_INCLUDE_DIR "${VISIONFLOW_ROOT}/include")
set(VISIONFLOW_LIB "${VISIONFLOW_ROOT}/lib/visionflow.if.lib")
set(VISIONFLOW_BIN_DIR "${VISIONFLOW_ROOT}/bin")
if(EXISTS "${VENDOR_AQ_SDK_DIR}/xai_api.lib")
    set(AQ_SDK_LIB "${VENDOR_AQ_SDK_DIR}/xai_api.lib")
endif()
if(EXISTS "${VENDOR_AQ_SDK_DIR}/xai_apid.lib")
    set(AQ_SDK_DEBUG_LIB "${VENDOR_AQ_SDK_DIR}/xai_apid.lib")
endif()
if(EXISTS "${VENDOR_VISIONFLOW_DIR}/visionflow.if.lib")
    set(VISIONFLOW_LIB "${VENDOR_VISIONFLOW_DIR}/visionflow.if.lib")
endif()
if(EXISTS "${OPENCV_ROOT}/include")
    set(OPENCV_INCLUDE_DIR "${OPENCV_ROOT}/include")
elseif(EXISTS "${OPENCV_ROOT}/build/include")
    set(OPENCV_INCLUDE_DIR "${OPENCV_ROOT}/build/include")
else()
    set(OPENCV_INCLUDE_DIR "${OPENCV_INCLUDE_FALLBACK}")
endif()

if(EXISTS "${OPENCV_ROOT}/lib")
    set(OPENCV_LIB_DIR "${OPENCV_ROOT}/lib")
elseif(EXISTS "${OPENCV_ROOT}/x64/vc16/lib")
    set(OPENCV_LIB_DIR "${OPENCV_ROOT}/x64/vc16/lib")
elseif(EXISTS "${OPENCV_ROOT}/build/x64/vc16/lib")
    set(OPENCV_LIB_DIR "${OPENCV_ROOT}/build/x64/vc16/lib")
else()
    message(FATAL_ERROR "Cannot locate OpenCV lib directory under ${OPENCV_ROOT}")
endif()

if(EXISTS "${OPENCV_ROOT}/bin")
    set(OPENCV_BIN_DIR "${OPENCV_ROOT}/bin")
elseif(EXISTS "${OPENCV_ROOT}/x64/vc16/bin")
    set(OPENCV_BIN_DIR "${OPENCV_ROOT}/x64/vc16/bin")
elseif(EXISTS "${OPENCV_ROOT}/build/x64/vc16/bin")
    set(OPENCV_BIN_DIR "${OPENCV_ROOT}/build/x64/vc16/bin")
else()
    message(FATAL_ERROR "Cannot locate OpenCV bin directory under ${OPENCV_ROOT}")
endif()

file(GLOB OPENCV_WORLD_LIB_CANDIDATES "${OPENCV_LIB_DIR}/opencv_world*.lib")
list(FILTER OPENCV_WORLD_LIB_CANDIDATES EXCLUDE REGEX "d\\.lib$")
list(LENGTH OPENCV_WORLD_LIB_CANDIDATES OPENCV_WORLD_LIB_COUNT)
if(OPENCV_WORLD_LIB_COUNT EQUAL 0)
    message(FATAL_ERROR "Cannot find OpenCV release import lib under ${OPENCV_LIB_DIR}")
endif()
list(GET OPENCV_WORLD_LIB_CANDIDATES 0 OPENCV_WORLD_LIB)
if(EXISTS "${VENDOR_OPENCV_DIR}/opencv_world470.lib")
    set(OPENCV_WORLD_LIB "${VENDOR_OPENCV_DIR}/opencv_world470.lib")
endif()

file(GLOB OPENCV_WORLD_DLL_CANDIDATES "${OPENCV_BIN_DIR}/opencv_world*.dll")
list(FILTER OPENCV_WORLD_DLL_CANDIDATES EXCLUDE REGEX "d\\.dll$")
list(LENGTH OPENCV_WORLD_DLL_CANDIDATES OPENCV_WORLD_DLL_COUNT)
if(OPENCV_WORLD_DLL_COUNT EQUAL 0)
    message(FATAL_ERROR "Cannot find OpenCV release DLL under ${OPENCV_BIN_DIR}")
endif()
list(GET OPENCV_WORLD_DLL_CANDIDATES 0 OPENCV_WORLD_DLL)

get_filename_component(OPENCV_WORLD_LIB_NAME "${OPENCV_WORLD_LIB}" NAME_WE)
get_filename_component(OPENCV_WORLD_DLL_NAME "${OPENCV_WORLD_DLL}" NAME)
set(XAI_OPENCV_RUNTIME_LEGACY OFF)
if(OPENCV_WORLD_LIB_NAME MATCHES "opencv_world470")
    set(XAI_OPENCV_RUNTIME_LEGACY ON)
endif()

if(XAI_TYPE_AQ_BUILD_SHARED)
    set(XAI_TYPE_AQ_LIBRARY_TYPE SHARED)
else()
    set(XAI_TYPE_AQ_LIBRARY_TYPE STATIC)
endif()

add_library(xai_type_aq ${XAI_TYPE_AQ_LIBRARY_TYPE}
    mmessage.cpp
    opencv_compat.cpp
    xai_type_aq.cpp
)

target_include_directories(xai_type_aq
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}"
        "${OPENCV_INCLUDE_DIR}"
    PRIVATE
        "${VISIONFLOW_INCLUDE_DIR}"
)

target_link_libraries(xai_type_aq
    PUBLIC
        "${AQ_SDK_LIB}"
        "${VISIONFLOW_LIB}"
        "${OPENCV_WORLD_LIB}"
)

if(MSVC AND XAI_TYPE_AQ_BUILD_SHARED)
    target_link_libraries(xai_type_aq PRIVATE delayimp.lib)
    target_link_options(xai_type_aq PRIVATE "/DELAYLOAD:visionflow.dll")
endif()

target_compile_definitions(xai_type_aq
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        $<$<BOOL:${XAI_TYPE_AQ_BUILD_SHARED}>:XAI_TYPE_AQ_BUILD_SHARED>
        $<$<BOOL:${XAI_OPENCV_RUNTIME_LEGACY}>:XAI_OPENCV_RUNTIME_LEGACY>
        XAI_OPENCV_WORLD_DLL_NAME="${OPENCV_WORLD_DLL_NAME}"
)

set_target_properties(xai_type_aq PROPERTIES
    OUTPUT_NAME "xai_type_aq"
)

install(TARGETS xai_type_aq
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION bin
    RUNTIME DESTINATION bin
)

if(NOT XAI_TYPE_AQ_BUILD_SHARED)
    add_executable(xai_type_aq_verify
        verify_static.cpp
    )

    target_include_directories(xai_type_aq_verify
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}"
            "${VISIONFLOW_INCLUDE_DIR}"
            "${OPENCV_INCLUDE_DIR}"
    )

    target_link_libraries(xai_type_aq_verify
        PRIVATE
            xai_type_aq
            "${AQ_SDK_LIB}"
            "${VISIONFLOW_LIB}"
            "${OPENCV_WORLD_LIB}"
    )

    target_compile_definitions(xai_type_aq_verify
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
    )

    install(TARGETS xai_type_aq_verify
        RUNTIME DESTINATION tools
    )
endif()

if(XAI_TYPE_AQ_BUILD_SHARED)
    add_executable(xai_type_aq_verify_plugin
        verify_plugin.cpp
    )

    target_include_directories(xai_type_aq_verify_plugin
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}"
            "${VISIONFLOW_INCLUDE_DIR}"
            "${OPENCV_INCLUDE_DIR}"
    )

    target_link_libraries(xai_type_aq_verify_plugin
        PRIVATE
            "${OPENCV_WORLD_LIB}"
    )

    target_compile_definitions(xai_type_aq_verify_plugin
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
    )

    install(TARGETS xai_type_aq_verify_plugin
        RUNTIME DESTINATION tools
    )
endif()

install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/README.md"
    DESTINATION .
)

install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/mmessage.h"
    "${CMAKE_CURRENT_LIST_DIR}/xai_api.h"
    "${CMAKE_CURRENT_LIST_DIR}/xai_def.h"
    "${CMAKE_CURRENT_LIST_DIR}/xai_err.h"
    "${CMAKE_CURRENT_LIST_DIR}/xai_inst_base.h"
    "${CMAKE_CURRENT_LIST_DIR}/xai_meth.h"
    "${CMAKE_CURRENT_LIST_DIR}/xai_type_api.h"
    "${CMAKE_CURRENT_LIST_DIR}/xai_type_base.h"
    "${CMAKE_CURRENT_LIST_DIR}/xai_type_aq.h"
    DESTINATION include
)

install(FILES
    "${AQ_SDK_LIB}"
    "${AQ_SDK_DEBUG_LIB}"
    "${VISIONFLOW_LIB}"
    "${OPENCV_WORLD_LIB}"
    DESTINATION lib
)

install(FILES "${OPENCV_WORLD_DLL}" DESTINATION bin)

install(DIRECTORY
    "${VISIONFLOW_BIN_DIR}/"
    DESTINATION bin
)

set(MSVC_RUNTIME_DLLS
    concrt140.dll
    msvcp140.dll
    msvcp140_1.dll
    msvcp140_2.dll
    msvcp140_atomic_wait.dll
    msvcp140_codecvt_ids.dll
)

foreach(runtime_file IN LISTS MSVC_RUNTIME_DLLS)
    if(EXISTS "${MSVC_REDIST_DIR}/${runtime_file}")
        install(FILES "${MSVC_REDIST_DIR}/${runtime_file}" DESTINATION bin)
    endif()
endforeach()

if(EXISTS "${MSVC_OPENMP_REDIST_DIR}/vcomp140.dll")
    install(FILES "${MSVC_OPENMP_REDIST_DIR}/vcomp140.dll" DESTINATION bin)
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../try/try.vfmodel")
    install(FILES
        "${CMAKE_CURRENT_LIST_DIR}/../try/try.vfmodel"
        DESTINATION model
    )
endif()
