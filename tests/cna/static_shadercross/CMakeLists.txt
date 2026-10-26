# A stand-in SDL3_shadercross package, for tests/cna/static_shadercross.cmake only: an installed
# CMake package exporting SDL3_shadercross::SDL3_shadercross-static, a C archive with the
# SDL_ShaderCross_* functions MojoShader's statically linked path calls, which links -- PRIVATE,
# so only through the export's $<LINK_ONLY:> -- a C++ archive standing in for SPIRV-Cross, as the
# real package's static target links spirv-cross-c. It translates no shader; it exists to show
# what FNA3D's MOJOSHADER_STATIC_SPIRVCROSS does with such a package.
#
# This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
cmake_minimum_required(VERSION 3.10)
project(fna3d_cna_shadercross_standin C CXX)

find_package(SDL3 CONFIG REQUIRED)

add_library(spirv-cross-standin STATIC standin_spirv_cross.cpp)
set_property(TARGET spirv-cross-standin PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(SDL3_shadercross-static STATIC standin_shadercross.c)
set_property(TARGET SDL3_shadercross-static PROPERTY OUTPUT_NAME SDL3_shadercross)
set_property(TARGET SDL3_shadercross-static PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(SDL3_shadercross-static PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
	"$<INSTALL_INTERFACE:include>")
# SDL's headers, as the real target links SDL3::Headers -- here for the build only, so that the
# package needs no find_dependency of its own.
target_link_libraries(SDL3_shadercross-static PRIVATE
	"$<BUILD_INTERFACE:SDL3::Headers>"
	spirv-cross-standin)

install(TARGETS SDL3_shadercross-static spirv-cross-standin
	EXPORT SDL3_shadercross-static-export
	ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT SDL3_shadercross-static-export
	FILE SDL3_shadercross-static-targets.cmake
	NAMESPACE SDL3_shadercross::
	DESTINATION lib/cmake/SDL3_shadercross)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/SDL3_shadercrossConfig.cmake"
	"include(\"\${CMAKE_CURRENT_LIST_DIR}/SDL3_shadercross-static-targets.cmake\")\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/SDL3_shadercrossConfig.cmake"
	DESTINATION lib/cmake/SDL3_shadercross)
