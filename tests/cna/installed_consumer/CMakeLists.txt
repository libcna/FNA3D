# An Effect client of an installed FNA3D, configured by tests/cna/installed_consumer.cmake against
# an installation moved away from its install prefix. It sees nothing of FNA3D's source or build
# tree: only what find_package(FNA3D) brings.
cmake_minimum_required(VERSION 3.10)
project(fna3d_installed_consumer C)

find_package(FNA3D CONFIG REQUIRED)

add_executable(effect_client effect_client.c)
target_link_libraries(effect_client PRIVATE FNA3D::FNA3D)

# Where each configuration's executable lies (a multi-config generator puts it in a directory of
# the configuration's name), for installed_consumer.cmake to run it.
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/effect_client-$<CONFIG>.path"
	CONTENT "$<TARGET_FILE:effect_client>")
