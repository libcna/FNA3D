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
