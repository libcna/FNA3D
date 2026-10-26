# libcna fork regression test: what FNA3D's MOJOSHADER_STATIC_SPIRVCROSS does with a static
# SDL3_shadercross package (2026-10-26 dependency audit A3a2 A-03, review R4-04 and R7-08).
# Run by ctest as `cmake -P`; see tests/cna/CMakeLists.txt for the variables. No display.
#
# The option once defined SDL_GPU_SHADERCROSS_STATIC, which nothing reads: MojoShader went on
# loading the shared SDL_shadercross at run time and nothing was linked. It must define
# CNA_MOJOSHADER_SDL_GPU_SHADERCROSS_STATIC for MojoShader, link the package's
# SDL3_shadercross::SDL3_shadercross-static into libFNA3D, and link the C++ runtime that archive's
# SPIRV-Cross needs. This test
#
#   1. builds and installs static_shadercross/, a stand-in package of that name: a C archive with
#      the SDL_ShaderCross_* functions, linking a C++ archive in place of SPIRV-Cross;
#   2. configures FNA3D's own source with MOJOSHADER_STATIC_SPIRVCROSS=ON against it and builds
#      and installs the FNA3D library;
#   3. finds CNA_MOJOSHADER_SDL_GPU_SHADERCROSS_STATIC on mojoshader_sdlgpu.c's compile command;
#   4. builds static_shadercross/client.c with the C compiler against the installed libFNA3D and
#      runs it: MOJOSHADER_sdlGetShaderFormats() must include the stand-in's DXBC and METALLIB,
#      which only its archive, statically linked and called from inside libFNA3D, reports -- the
#      dynamic path would look for libSDL3_shadercross.so.0, which is not there -- and the C
#      client links and loads only if libFNA3D brings the C++ runtime itself.
#
# This proves FNA3D's side: the selector, the archive's linkage and the C++ runtime. It does not
# prove the real SDL_shadercross package works: that one's config also asks for SPIRV-Cross's
# packages and for DirectXShaderCompiler even when built without DXC, and translates shaders,
# which the stand-in does not.
#
# Its two nested trees stay in WORK_DIR between runs and build incrementally; the installations
# and the client are made afresh every time.
#
# This file is part of the libcna fork of FNA3D; see tests/cna/README.md.

foreach(variable FNA3D_SOURCE_DIR FNA3D_BUILD_DIR WORK_DIR FIXTURE_DIR C_COMPILER CXX_COMPILER
		 GENERATOR)
	if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
		message(FATAL_ERROR "static_shadercross.cmake: ${variable} is not set")
	endif()
endforeach()

get_filename_component(WORK_NAME "${WORK_DIR}" NAME)
string(FIND "${WORK_DIR}" "${FNA3D_BUILD_DIR}/" WORK_IN_BUILD)
if(NOT WORK_NAME STREQUAL "static-shadercross" OR NOT WORK_IN_BUILD EQUAL 0)
	message(FATAL_ERROR "static_shadercross.cmake: refusing to use ${WORK_DIR} as the work directory")
endif()
foreach(fresh standin-install fna3d-install client)
	file(REMOVE_RECURSE "${WORK_DIR}/${fresh}")
endforeach()
file(MAKE_DIRECTORY "${WORK_DIR}/client")

function(run what)
	execute_process(COMMAND ${ARGN}
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
	)
	set(LAST_OUTPUT "${output}" PARENT_SCOPE)
	if(NOT result EQUAL 0)
		message(STATUS "${what}: FAILED (${result}):\n${output}")
		message(FATAL_ERROR "static SDL_shadercross, failed: ${what}")
	endif()
	message(STATUS "${what}: ok")
endfunction()

set(COMMON_ARGS
	-G "${GENERATOR}"
	"-DCMAKE_C_COMPILER=${C_COMPILER}"
	"-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
	-DCMAKE_BUILD_TYPE=Debug
)
foreach(launcher C_COMPILER_LAUNCHER CXX_COMPILER_LAUNCHER)
	if(DEFINED ${launcher} AND NOT "${${launcher}}" STREQUAL "")
		list(APPEND COMMON_ARGS "-DCMAKE_${launcher}=${${launcher}}")
	endif()
endforeach()
if(DEFINED SDL3_DIR AND NOT SDL3_DIR STREQUAL "")
	list(APPEND COMMON_ARGS "-DSDL3_DIR=${SDL3_DIR}")
endif()
set(BUILD_ARGS --config Debug --parallel 4)

# 1. the stand-in package
run("stand-in SDL3_shadercross: configure" "${CMAKE_COMMAND}"
	-S "${FIXTURE_DIR}" -B "${WORK_DIR}/standin-build" ${COMMON_ARGS})
run("stand-in SDL3_shadercross: build" "${CMAKE_COMMAND}" --build "${WORK_DIR}/standin-build"
	${BUILD_ARGS})
run("stand-in SDL3_shadercross: install" "${CMAKE_COMMAND}" --install "${WORK_DIR}/standin-build"
	--config Debug --prefix "${WORK_DIR}/standin-install")

# 2. FNA3D with MOJOSHADER_STATIC_SPIRVCROSS
run("FNA3D with MOJOSHADER_STATIC_SPIRVCROSS: configure" "${CMAKE_COMMAND}"
	-S "${FNA3D_SOURCE_DIR}" -B "${WORK_DIR}/fna3d-build" ${COMMON_ARGS}
	-DBUILD_SHARED_LIBS=ON
	-DMOJOSHADER_STATIC_SPIRVCROSS=ON
	"-DSDL3_shadercross_DIR=${WORK_DIR}/standin-install/lib/cmake/SDL3_shadercross"
	-DCMAKE_INSTALL_LIBDIR=lib
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
run("FNA3D with MOJOSHADER_STATIC_SPIRVCROSS: build" "${CMAKE_COMMAND}"
	--build "${WORK_DIR}/fna3d-build" --target FNA3D ${BUILD_ARGS})
run("FNA3D with MOJOSHADER_STATIC_SPIRVCROSS: install" "${CMAKE_COMMAND}"
	--install "${WORK_DIR}/fna3d-build" --config Debug --prefix "${WORK_DIR}/fna3d-install")

# 3. the selector on MojoShader's SDL_GPU glue
set(COMMANDS_FILE "${WORK_DIR}/fna3d-build/compile_commands.json")
if(NOT EXISTS "${COMMANDS_FILE}")
	message(FATAL_ERROR "static SDL_shadercross, failed: no ${COMMANDS_FILE}")
endif()
file(READ "${COMMANDS_FILE}" COMMANDS)
string(JSON COMMAND_COUNT LENGTH "${COMMANDS}")
set(SDLGPU_COMMAND "")
math(EXPR LAST "${COMMAND_COUNT} - 1")
foreach(i RANGE ${LAST})
	string(JSON FILE GET "${COMMANDS}" ${i} file)
	if(FILE MATCHES "/MojoShader/mojoshader_sdlgpu\\.c$")
		string(JSON SDLGPU_COMMAND GET "${COMMANDS}" ${i} command)
	endif()
endforeach()
if(SDLGPU_COMMAND STREQUAL "")
	message(FATAL_ERROR "static SDL_shadercross, failed: mojoshader_sdlgpu.c is not compiled")
endif()
if(NOT SDLGPU_COMMAND MATCHES "-DCNA_MOJOSHADER_SDL_GPU_SHADERCROSS_STATIC( |$)")
	message(FATAL_ERROR "static SDL_shadercross, failed: mojoshader_sdlgpu.c is compiled without "
		"CNA_MOJOSHADER_SDL_GPU_SHADERCROSS_STATIC:\n${SDLGPU_COMMAND}")
endif()
message(STATUS "mojoshader_sdlgpu.c compiled with CNA_MOJOSHADER_SDL_GPU_SHADERCROSS_STATIC: ok")

# 4. a C client of the installed library
set(RPATH "${WORK_DIR}/fna3d-install/lib")
if(DEFINED SDL3_DIR AND EXISTS "${SDL3_DIR}/../..")
	get_filename_component(SDL3_LIB_DIR "${SDL3_DIR}/../.." ABSOLUTE)
	string(APPEND RPATH ":${SDL3_LIB_DIR}")
endif()
run("C client: build" "${C_COMPILER}" "${FIXTURE_DIR}/client.c" -o "${WORK_DIR}/client/client"
	"-L${WORK_DIR}/fna3d-install/lib" -lFNA3D "-Wl,-rpath,${RPATH}")
run("C client: run" "${WORK_DIR}/client/client")
message(STATUS "${LAST_OUTPUT}")
