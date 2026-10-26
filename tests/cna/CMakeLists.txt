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
