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
target_include_directories(fna3d_cna_dispose_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
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
		"-DSDL3_DIR=${SDL3_DIR}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer.cmake"
	)
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
