cmake_minimum_required(VERSION 3.0)

project(openRBRVR)

set(CMAKE_CXX_STANDARD 23)

set(SOURCES
    "API.cpp"
    "Menu.cpp"
    "openRBRVR.cpp"
    "VR.cpp"
    "Vertex.cpp"
    "openRBRVR.cpp"
    "OpenVR.cpp"
    "OpenXR.cpp"
)

set(HEADERS
    "Minhook/Include/MinHook.h"
    "glm/mat4x4.hpp"
    "Config.hpp"
    "D3D.hpp"
    "Hook.hpp"
    "IPlugin.h"
    "Licenses.hpp"
    "Menu.hpp"
    "Util.hpp"
    "VR.hpp"
    "Vertex.hpp"
    "openRBRVR.def"
    "openRBRVR.hpp"
    "OpenVR.hpp"
    "OpenXR.hpp"
    "Quaternion.hpp"
)

set(openRBRVR_Major 0)
set(openRBRVR_Minor 5)
set(openRBRVR_Patch 0)
set(openRBRVR_Tweak 9)
set(openRBRVR_TweakStr "-rc${openRBRVR_Tweak}")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Version.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/Version.hpp
  @ONLY
)

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

target_compile_options(${PROJECT_NAME} PRIVATE /arch:AVX)
target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32 _WINDOWS _USRDLL _MBCS)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/minhook/include"
    "${CMAKE_SOURCE_DIR}/dxvk/src/d3d9"
    "${CMAKE_SOURCE_DIR}/dxvk/include/vulkan/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_SOURCE_DIR}/lib
  ${CMAKE_SOURCE_DIR}/minhook/bin
)

# set(OPENXR_LOADER "$<$<CONFIG:Debug>:openxr_loaderd>$<$<NOT:$<CONFIG:Debug>>:openxr_loader>")
target_link_libraries(${PROJECT_NAME} PUBLIC
  d3d9
  d3dcompiler
  libminhook.x86
  openvr_api
  openxr_loader
  #${OPENXR_LOADER}
)

cmake_path(NATIVE_PATH CMAKE_INSTALL_PREFIX NATIVE_CMAKE_INSTALL_PREFIX)

set_target_properties(${PROJECT_NAME}
PROPERTIES
	VS_DEBUGGER_COMMAND "${NATIVE_CMAKE_INSTALL_PREFIX}\\RichardBurnsRally_SSE.exe"
	VS_DEBUGGER_COMMAND_ARGUMENTS "$<TARGET_FILE:${PROJECT_NAME}>"
)

# Set the output directory for Windows
set_target_properties(${PROJECT_NAME} PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
	LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
	PDB_OUTPUT_DIRECTORY_DEBUG     "${CMAKE_BINARY_DIR}/Release"
	ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
	LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
	PDB_OUTPUT_DIRECTORY_RELEASE     "${CMAKE_BINARY_DIR}/Release"
)

add_custom_target(fmt
    COMMAND clang-format -i ${SOURCES} ${HEADERS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(build_and_copy
    COMMAND copy ${CMAKE_BUILD_TYPE}\\openRBRVR.dll ${NATIVE_CMAKE_INSTALL_PREFIX}\\Plugins
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_dependencies(build_and_copy ${PROJECT_NAME})
