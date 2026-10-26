# Regression tests for the libcna fork (see README.md in this directory).
#
# They create a real FNA3D device in an SDL window, so they need a display; in CNA's checkout run
# them through tools/platform/run_gpu_tests_private.sh. Each test runs on the OpenGL driver, once
# on a desktop context and once on an OpenGL ES 3 one (FNA3D_OPENGL_FORCE_ES3), and on the SDL_GPU
# driver; on Windows FNA3D_FORCE_DRIVER=D3D11 runs the same executables on Direct3D 11.

add_executable(fna3d_cna_readback_tests readback_tests.c)
target_link_libraries(fna3d_cna_readback_tests PRIVATE FNA3D)
target_include_directories(fna3d_cna_readback_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
if(TARGET SDL3::SDL3)
	target_link_libraries(fna3d_cna_readback_tests PRIVATE SDL3::SDL3)
endif()

add_test(NAME fna3d_cna_readback_opengl COMMAND fna3d_cna_readback_tests)
set_tests_properties(fna3d_cna_readback_opengl PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL")
add_test(NAME fna3d_cna_readback_opengl_es3 COMMAND fna3d_cna_readback_tests)
set_tests_properties(fna3d_cna_readback_opengl_es3 PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL;FNA3D_OPENGL_FORCE_ES3=1")
add_test(NAME fna3d_cna_readback_sdlgpu COMMAND fna3d_cna_readback_tests)
set_tests_properties(fna3d_cna_readback_sdlgpu PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=SDLGPU")

add_executable(fna3d_cna_dispose_tests dispose_tests.c)
target_link_libraries(fna3d_cna_dispose_tests PRIVATE FNA3D)
target_include_directories(fna3d_cna_dispose_tests PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../../include"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../MojoShader"
)
if(TARGET SDL3::SDL3)
	target_link_libraries(fna3d_cna_dispose_tests PRIVATE SDL3::SDL3)
endif()

add_test(NAME fna3d_cna_dispose_opengl COMMAND fna3d_cna_dispose_tests)
set_tests_properties(fna3d_cna_dispose_opengl PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL")
add_test(NAME fna3d_cna_dispose_opengl_es3 COMMAND fna3d_cna_dispose_tests)
set_tests_properties(fna3d_cna_dispose_opengl_es3 PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL;FNA3D_OPENGL_FORCE_ES3=1")
add_test(NAME fna3d_cna_dispose_sdlgpu COMMAND fna3d_cna_dispose_tests)
set_tests_properties(fna3d_cna_dispose_sdlgpu PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=SDLGPU")

# The tests that draw build a minimal Effect in memory (test_effect.h) and hand FNA3D_ApplyEffect
# MojoShader's state-change record, so they see FNA3D's own mojoshader.h.
add_executable(fna3d_cna_backbuffer_tests backbuffer_tests.c)
target_link_libraries(fna3d_cna_backbuffer_tests PRIVATE FNA3D)
target_include_directories(fna3d_cna_backbuffer_tests PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../../include"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../MojoShader"
)
if(TARGET SDL3::SDL3)
	target_link_libraries(fna3d_cna_backbuffer_tests PRIVATE SDL3::SDL3)
endif()

add_test(NAME fna3d_cna_backbuffer_opengl COMMAND fna3d_cna_backbuffer_tests)
set_tests_properties(fna3d_cna_backbuffer_opengl PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL")
add_test(NAME fna3d_cna_backbuffer_opengl_es3 COMMAND fna3d_cna_backbuffer_tests)
set_tests_properties(fna3d_cna_backbuffer_opengl_es3 PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL;FNA3D_OPENGL_FORCE_ES3=1")
add_test(NAME fna3d_cna_backbuffer_sdlgpu COMMAND fna3d_cna_backbuffer_tests)
set_tests_properties(fna3d_cna_backbuffer_sdlgpu PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=SDLGPU")

add_executable(fna3d_cna_vertex_usage_tests vertex_usage_tests.c)
target_link_libraries(fna3d_cna_vertex_usage_tests PRIVATE FNA3D)
target_include_directories(fna3d_cna_vertex_usage_tests PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../../include"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../MojoShader"
)
if(TARGET SDL3::SDL3)
	target_link_libraries(fna3d_cna_vertex_usage_tests PRIVATE SDL3::SDL3)
endif()

add_test(NAME fna3d_cna_vertex_usage_opengl COMMAND fna3d_cna_vertex_usage_tests)
set_tests_properties(fna3d_cna_vertex_usage_opengl PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL")
add_test(NAME fna3d_cna_vertex_usage_opengl_es3 COMMAND fna3d_cna_vertex_usage_tests)
set_tests_properties(fna3d_cna_vertex_usage_opengl_es3 PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL;FNA3D_OPENGL_FORCE_ES3=1")
add_test(NAME fna3d_cna_vertex_usage_sdlgpu COMMAND fna3d_cna_vertex_usage_tests)
set_tests_properties(fna3d_cna_vertex_usage_sdlgpu PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=SDLGPU")

add_executable(fna3d_cna_vface_tests vface_tests.c)
target_link_libraries(fna3d_cna_vface_tests PRIVATE FNA3D)
target_include_directories(fna3d_cna_vface_tests PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../../include"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../MojoShader"
)
if(TARGET SDL3::SDL3)
	target_link_libraries(fna3d_cna_vface_tests PRIVATE SDL3::SDL3)
endif()

add_test(NAME fna3d_cna_vface_opengl COMMAND fna3d_cna_vface_tests)
set_tests_properties(fna3d_cna_vface_opengl PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL")
add_test(NAME fna3d_cna_vface_opengl_es3 COMMAND fna3d_cna_vface_tests)
set_tests_properties(fna3d_cna_vface_opengl_es3 PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=OpenGL;FNA3D_OPENGL_FORCE_ES3=1")
add_test(NAME fna3d_cna_vface_sdlgpu COMMAND fna3d_cna_vface_tests)
set_tests_properties(fna3d_cna_vface_sdlgpu PROPERTIES
	ENVIRONMENT "FNA3D_FORCE_DRIVER=SDLGPU")

# A blend state's pipeline-cache key tells states apart (pipeline_cache_tests.c). No display;
# FNA3D_PipelineCache.c is compiled into the test.
add_executable(fna3d_cna_pipeline_cache_tests
	pipeline_cache_tests.c
	"${PROJECT_SOURCE_DIR}/src/FNA3D_PipelineCache.c")
target_include_directories(fna3d_cna_pipeline_cache_tests PRIVATE
	"${PROJECT_SOURCE_DIR}/include"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/MojoShader")
if(TARGET SDL3::SDL3)
	target_link_libraries(fna3d_cna_pipeline_cache_tests PRIVATE SDL3::SDL3)
endif()
add_test(NAME fna3d_cna_pipeline_cache COMMAND fna3d_cna_pipeline_cache_tests)

# The SDL_GPU driver forwards every shader format MojoShader offers to SDL's device creation
# (sdlgpu_properties_tests.c). No display; the driver, FNA3D_PipelineCache.c and MojoShader are
# compiled into the test, so it needs no symbol libFNA3D does not export.
add_executable(fna3d_cna_sdlgpu_properties_tests
	sdlgpu_properties_tests.c
	"${PROJECT_SOURCE_DIR}/src/FNA3D_PipelineCache.c")
target_include_directories(fna3d_cna_sdlgpu_properties_tests PRIVATE
	"${PROJECT_SOURCE_DIR}/include"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/MojoShader")
target_link_libraries(fna3d_cna_sdlgpu_properties_tests PRIVATE mojoshader ${LOBJC})
if(TARGET SDL3::SDL3)
	target_link_libraries(fna3d_cna_sdlgpu_properties_tests PRIVATE SDL3::SDL3)
endif()
add_test(NAME fna3d_cna_sdlgpu_properties COMMAND fna3d_cna_sdlgpu_properties_tests)

# The installation, moved away from its prefix, serves an Effect client through find_package and
# pkg-config (installed_consumer.cmake). No display; FNA3D installs only as a shared library.
if(BUILD_SHARED_LIBS)
	add_test(NAME fna3d_cna_installed_consumer COMMAND "${CMAKE_COMMAND}"
		"-DFNA3D_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
		"-DFNA3D_BUILD_DIR=${PROJECT_BINARY_DIR}"
		"-DFNA3D_INSTALL_LIBDIR=${FNA3D_INSTALL_LIBDIR}"
		"-DWORK_DIR=${PROJECT_BINARY_DIR}/tests/cna/installed-consumer"
		"-DCONSUMER_DIR=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer"
		"-DC_COMPILER=${CMAKE_C_COMPILER}"
		"-DC_FLAGS=${CMAKE_C_FLAGS}"
		"-DEXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
		"-DGENERATOR=${CMAKE_GENERATOR}"
		"-DCONFIG=$<CONFIG>"
		"-DSDL3_DIR=${SDL3_DIR}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer.cmake"
	)
endif()

# MOJOSHADER_STATIC_SPIRVCROSS, configured against a stand-in static SDL3_shadercross package
# (static_shadercross.cmake, static_shadercross/), must select MojoShader's statically linked
# path, link the archive into libFNA3D and link the C++ runtime it needs. No display. It builds
# FNA3D once more inside its work directory (incrementally after the first run), so it needs a
# C++ compiler; registered for GCC- or Clang-style toolchains on Linux-like systems, as it links
# its C client with -L, -l and an rpath.
include(CheckLanguage)
check_language(CXX)
if(	UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND
	CMAKE_CXX_COMPILER AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"	)
	add_test(NAME fna3d_cna_static_shadercross COMMAND "${CMAKE_COMMAND}"
		"-DFNA3D_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
		"-DFNA3D_BUILD_DIR=${PROJECT_BINARY_DIR}"
		"-DWORK_DIR=${PROJECT_BINARY_DIR}/tests/cna/static-shadercross"
		"-DFIXTURE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/static_shadercross"
		"-DC_COMPILER=${CMAKE_C_COMPILER}"
		"-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
		"-DC_COMPILER_LAUNCHER=${CMAKE_C_COMPILER_LAUNCHER}"
		"-DCXX_COMPILER_LAUNCHER=${CMAKE_CXX_COMPILER_LAUNCHER}"
		"-DGENERATOR=${CMAKE_GENERATOR}"
		"-DSDL3_DIR=${SDL3_DIR}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/static_shadercross.cmake"
	)
else()
	message(STATUS "tests/cna: fna3d_cna_static_shadercross is not registered "
		"(it needs a C++ compiler and a GCC- or Clang-style toolchain on a Linux-like system)")
endif()

# replay.c must compile against SDL 2 as well (BUILD_SDL3=OFF, TRACING_SUPPORT=ON), with the
# definitions such a configuration gives it: this directory's, without the SDL 3 ones, plus
# FNA3D_TRACING. A syntax check at C99 and C17 (C23 would make `true` a keyword); registered only
# when SDL 2's development files are found and the compiler takes -fsyntax-only.
find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND AND DEFINED SDL2_INCLUDE_DIRS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	get_directory_property(FNA3D_CNA_SDL2_DEFINITIONS COMPILE_DEFINITIONS)
	list(REMOVE_ITEM FNA3D_CNA_SDL2_DEFINITIONS USE_SDL3 FNA3D_DRIVER_SDL)
	list(APPEND FNA3D_CNA_SDL2_DEFINITIONS FNA3D_TRACING)
	set(FNA3D_CNA_SDL2_FLAGS "")
	foreach(definition ${FNA3D_CNA_SDL2_DEFINITIONS})
		list(APPEND FNA3D_CNA_SDL2_FLAGS "-D${definition}")
	endforeach()
	foreach(directory ${SDL2_INCLUDE_DIRS})
		if(NOT directory STREQUAL "")
			list(APPEND FNA3D_CNA_SDL2_FLAGS "-I${directory}")
		endif()
	endforeach()
	foreach(standard gnu99 gnu17)
		add_test(NAME fna3d_cna_replay_sdl2_${standard} COMMAND "${CMAKE_C_COMPILER}"
			-std=${standard} -fsyntax-only
			${FNA3D_CNA_SDL2_FLAGS}
			"-I${PROJECT_SOURCE_DIR}/include"
			"-I${PROJECT_SOURCE_DIR}/MojoShader"
			"${PROJECT_SOURCE_DIR}/replay/replay.c"
		)
	endforeach()
else()
	message(STATUS "tests/cna: no SDL 2 development files (or no -fsyntax-only); "
		"fna3d_cna_replay_sdl2_* are not registered")
endif()

# One trace session spans overlapping devices, and a device that is not made ends the session it
# was counted in (trace_overlap_tests.c), on each driver; it writes FNA3D_Trace.bin into a
# directory of its own.
if(TRACING_SUPPORT)
	add_executable(fna3d_cna_trace_overlap_tests trace_overlap_tests.c)
	target_link_libraries(fna3d_cna_trace_overlap_tests PRIVATE FNA3D)
	target_include_directories(fna3d_cna_trace_overlap_tests PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/../../include")
	if(TARGET SDL3::SDL3)
		target_link_libraries(fna3d_cna_trace_overlap_tests PRIVATE SDL3::SDL3)
	endif()
	foreach(variant opengl opengl_es3 sdlgpu)
		if(variant STREQUAL "opengl")
			set(environment "FNA3D_FORCE_DRIVER=OpenGL")
		elseif(variant STREQUAL "opengl_es3")
			set(environment "FNA3D_FORCE_DRIVER=OpenGL;FNA3D_OPENGL_FORCE_ES3=1")
		else()
			set(environment "FNA3D_FORCE_DRIVER=SDLGPU")
		endif()
		set(directory "${CMAKE_CURRENT_BINARY_DIR}/trace-overlap-${variant}")
		file(MAKE_DIRECTORY "${directory}")
		add_test(NAME fna3d_cna_trace_overlap_${variant} COMMAND fna3d_cna_trace_overlap_tests
			WORKING_DIRECTORY "${directory}")
		set_tests_properties(fna3d_cna_trace_overlap_${variant} PROPERTIES
			ENVIRONMENT "${environment}")
	endforeach()
endif()

# A recorded trace replays and dumps as it was captured (trace_tests.cmake), on each driver;
# only a TRACING_SUPPORT build has the trace writer, fna3d_replay and fna3d_dumpspirv.
if(TRACING_SUPPORT AND TARGET fna3d_replay)
	add_executable(fna3d_cna_trace_record trace_record.c)
	target_link_libraries(fna3d_cna_trace_record PRIVATE FNA3D)
	target_include_directories(fna3d_cna_trace_record PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/../../include"
		"${CMAKE_CURRENT_SOURCE_DIR}/../../MojoShader"
	)
	# The replay's extension calls are observed by an LD_PRELOAD interposer (dlfcn's RTLD_NEXT),
	# which sees them only where fna3d_replay calls a shared libFNA3D through the dynamic linker
	# and the loader honours LD_PRELOAD. Elsewhere the replay still runs, and the call-log
	# comparison is skipped, saying why.
	include(CheckIncludeFile)
	check_include_file(dlfcn.h FNA3D_CNA_HAVE_DLFCN_H)
	set(FNA3D_CNA_INTERPOSER "")
	set(FNA3D_CNA_INTERPOSER_SKIPPED "")
	if(NOT BUILD_SHARED_LIBS)
		set(FNA3D_CNA_INTERPOSER_SKIPPED "FNA3D is a static library, so fna3d_replay calls it directly")
	elseif(NOT UNIX OR APPLE OR EMSCRIPTEN)
		set(FNA3D_CNA_INTERPOSER_SKIPPED "this platform's loader has no LD_PRELOAD")
	elseif(NOT FNA3D_CNA_HAVE_DLFCN_H)
		set(FNA3D_CNA_INTERPOSER_SKIPPED "there is no dlfcn.h")
	else()
		add_library(fna3d_cna_trace_interposer MODULE trace_interposer.c)
		target_link_libraries(fna3d_cna_trace_interposer PRIVATE ${CMAKE_DL_LIBS})
		set(FNA3D_CNA_INTERPOSER "$<TARGET_FILE:fna3d_cna_trace_interposer>")
	endif()
	if(NOT FNA3D_CNA_INTERPOSER_SKIPPED STREQUAL "")
		message(STATUS "tests/cna: fna3d_cna_trace_* replay without the call-log interposer: "
			"${FNA3D_CNA_INTERPOSER_SKIPPED}")
	endif()
	if(TARGET SDL3::SDL3)
		target_link_libraries(fna3d_cna_trace_record PRIVATE SDL3::SDL3)
	endif()
	set(FNA3D_CNA_DUMPSPIRV "")
	set(FNA3D_CNA_SPIRV_CHECK "")
	if(TARGET fna3d_dumpspirv)
		# Links through the MojoShader libFNA3D exports, the one fna3d_dumpspirv uses.
		add_executable(fna3d_cna_spirv_check spirv_check.c)
		target_link_libraries(fna3d_cna_spirv_check PRIVATE FNA3D)
		target_include_directories(fna3d_cna_spirv_check PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}/../../include"
			"${CMAKE_CURRENT_SOURCE_DIR}/../../MojoShader"
		)
		if(TARGET SDL3::SDL3)
			target_link_libraries(fna3d_cna_spirv_check PRIVATE SDL3::SDL3)
		endif()
		set(FNA3D_CNA_DUMPSPIRV "$<TARGET_FILE:fna3d_dumpspirv>")
		set(FNA3D_CNA_SPIRV_CHECK "$<TARGET_FILE:fna3d_cna_spirv_check>")
	endif()
	if(CMAKE_C_FLAGS MATCHES "-fsanitize=[^ ]*address")
		set(FNA3D_CNA_ASAN ON)
	else()
		set(FNA3D_CNA_ASAN OFF)
	endif()
	foreach(variant opengl opengl_es3 sdlgpu)
		if(variant STREQUAL "opengl")
			set(environment "FNA3D_FORCE_DRIVER=OpenGL")
		elseif(variant STREQUAL "opengl_es3")
			set(environment "FNA3D_FORCE_DRIVER=OpenGL;FNA3D_OPENGL_FORCE_ES3=1")
		else()
			set(environment "FNA3D_FORCE_DRIVER=SDLGPU")
		endif()
		add_test(NAME fna3d_cna_trace_${variant} COMMAND "${CMAKE_COMMAND}"
			"-DBUILD_DIR=${PROJECT_BINARY_DIR}"
			"-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/trace-${variant}"
			"-DRECORD=$<TARGET_FILE:fna3d_cna_trace_record>"
			"-DREPLAY=$<TARGET_FILE:fna3d_replay>"
			"-DINTERPOSER=${FNA3D_CNA_INTERPOSER}"
			"-DINTERPOSER_SKIPPED=${FNA3D_CNA_INTERPOSER_SKIPPED}"
			"-DDUMPSPIRV=${FNA3D_CNA_DUMPSPIRV}"
			"-DSPIRV_CHECK=${FNA3D_CNA_SPIRV_CHECK}"
			"-DASAN=${FNA3D_CNA_ASAN}"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/trace_tests.cmake"
		)
		set_tests_properties(fna3d_cna_trace_${variant} PROPERTIES ENVIRONMENT "${environment}")
	endforeach()
endif()
