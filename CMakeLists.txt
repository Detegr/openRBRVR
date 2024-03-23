cmake_minimum_required(VERSION 3.0)

project(openRBRVR)

set(CMAKE_CXX_STANDARD 23)

set(SOURCES
    "src/API.cpp"
    "src/Dx.cpp"
    "src/Globals.cpp"
    "src/Menu.cpp"
    "src/OpenVR.cpp"
    "src/OpenXR.cpp"
    "src/RBR.cpp"
    "src/RenderTarget.cpp"
    "src/VR.cpp"
    "src/Vertex.cpp"
    "src/openRBRVR.cpp"
    "src/openRBRVR.cpp"
)

set(HEADERS
    "src/Config.hpp"
    "src/D3D.hpp"
    "src/Dx.hpp"
    "src/Globals.hpp"
    "src/Hook.hpp"
    "src/IPlugin.h"
    "src/Licenses.hpp"
    "src/Menu.hpp"
    "src/OpenVR.hpp"
    "src/OpenXR.hpp"
    "src/Quaternion.hpp"
    "src/RBR.hpp"
    "src/RenderTarget.hpp"
    "src/Util.hpp"
    "src/VR.hpp"
    "src/Vertex.hpp"
    "src/openRBRVR.def"
    "src/openRBRVR.hpp"
)

set(openRBRVR_Major 0)
set(openRBRVR_Minor 8)
set(openRBRVR_Patch 0)
set(openRBRVR_Tweak 0)
# set(openRBRVR_TweakStr "-rc${openRBRVR_Tweak}")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/version.rc.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.rc
  @ONLY)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Version.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/Version.hpp
  @ONLY
)

add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

target_compile_options(${PROJECT_NAME} PRIVATE /arch:AVX)
target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32 _WINDOWS _USRDLL _MBCS)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/thirdparty"
    "${CMAKE_SOURCE_DIR}/thirdparty/dxvk/include/vulkan/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/dxvk/src/d3d9"
    "${CMAKE_SOURCE_DIR}/thirdparty/glm"
    "${CMAKE_SOURCE_DIR}/thirdparty/minhook/include"
    "${CMAKE_SOURCE_DIR}/thirdparty/openvr"
    "${CMAKE_SOURCE_DIR}/thirdparty/openxr"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_SOURCE_DIR}/thirdparty/lib
  ${CMAKE_SOURCE_DIR}/thirdparty/minhook/bin
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
