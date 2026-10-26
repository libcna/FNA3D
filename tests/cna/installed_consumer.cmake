# libcna fork regression test: an installed FNA3D, moved away from its install prefix, serves an
# Effect client through find_package(FNA3D) and through pkg-config (2026-10-26 dependency audit
# A3a2 A-02). Run by ctest as `cmake -P`; see tests/cna/CMakeLists.txt for the variables.
#
# The installation used to carry no mojoshader.h (FNA3D.h's Effect API is declared in its types),
# its FNA3D.pc published neither that header's directory nor the definitions FNA3D compiled it
# with, and FNA3DConfig.cmake included target files nothing writes and looked for SDL2 whatever
# BUILD_SDL3 said. Each half below builds and runs installed_consumer/effect_client.c, and checks
# that no path into FNA3D's source or build tree reached the compiler.
#
# This file is part of the libcna fork of FNA3D; see tests/cna/README.md.

foreach(variable FNA3D_SOURCE_DIR FNA3D_BUILD_DIR FNA3D_INSTALL_LIBDIR WORK_DIR CONSUMER_DIR
		 C_COMPILER GENERATOR)
	if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
		message(FATAL_ERROR "installed_consumer.cmake: ${variable} is not set")
	endif()
endforeach()

# WORK_DIR is removed and rewritten on every run, so it must be this test's own directory in the
# build tree and nothing else.
get_filename_component(WORK_NAME "${WORK_DIR}" NAME)
string(FIND "${WORK_DIR}" "${FNA3D_BUILD_DIR}/" WORK_IN_BUILD)
if(NOT WORK_NAME STREQUAL "installed-consumer" OR NOT WORK_IN_BUILD EQUAL 0)
	message(FATAL_ERROR "installed_consumer.cmake: refusing to use ${WORK_DIR} as the work directory")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
# The client's own sources are copied out of the source tree, so that a path into it in the
# consumer's build can only have come from the installation.
file(COPY "${CONSUMER_DIR}/" DESTINATION "${WORK_DIR}/consumer-src")
set(CONSUMER_SRC "${WORK_DIR}/consumer-src")

# Runs one step; a failed step is reported and ends its half (STEP_OK false), the other half still
# runs, and the test fails at the end.
set(FAILED_STEPS "")
function(run what)
	execute_process(COMMAND ${ARGN}
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
	)
	set(LAST_OUTPUT "${output}" PARENT_SCOPE)
	if(NOT result EQUAL 0)
		message(STATUS "${what}: FAILED (${result}):\n${output}")
		set(FAILED_STEPS "${FAILED_STEPS};${what}" PARENT_SCOPE)
		set(STEP_OK FALSE PARENT_SCOPE)
		return()
	endif()
	message(STATUS "${what}: ok")
	set(STEP_OK TRUE PARENT_SCOPE)
endfunction()

# Nothing that reached the consumer's compiler or linker may name FNA3D's source or build tree
# (the work directory itself lies in the build tree, so it is taken out first).
function(check_no_tree_paths what text)
	string(REPLACE "${WORK_DIR}" "<work>" text "${text}")
	foreach(tree "${FNA3D_SOURCE_DIR}" "${FNA3D_BUILD_DIR}")
		string(FIND "${text}" "${tree}" found)
		if(NOT found EQUAL -1)
			message(STATUS "${what}: FAILED, it names ${tree}:\n${text}")
			set(FAILED_STEPS "${FAILED_STEPS};${what} names a tree path" PARENT_SCOPE)
			set(STEP_OK FALSE PARENT_SCOPE)
			return()
		endif()
	endforeach()
endfunction()

# Install, then move the installation: nothing may depend on the prefix it was installed to.
run("install" "${CMAKE_COMMAND}" --install "${FNA3D_BUILD_DIR}" --prefix "${WORK_DIR}/staged")
if(NOT STEP_OK)
	message(FATAL_ERROR "cmake --install failed")
endif()
file(RENAME "${WORK_DIR}/staged" "${WORK_DIR}/moved")
set(PREFIX "${WORK_DIR}/moved")
if(NOT EXISTS "${PREFIX}/include/FNA3D/mojoshader.h")
	message(STATUS "the installation has no include/FNA3D/mojoshader.h")
	set(FAILED_STEPS "${FAILED_STEPS};mojoshader.h installed")
endif()

# find_package(FNA3D)
set(CONFIGURE_ARGS
	-S "${CONSUMER_SRC}"
	-B "${WORK_DIR}/cmake-consumer"
	-G "${GENERATOR}"
	"-DCMAKE_C_COMPILER=${C_COMPILER}"
	"-DCMAKE_C_FLAGS=${C_FLAGS}"
	"-DCMAKE_EXE_LINKER_FLAGS=${EXE_LINKER_FLAGS}"
	"-DCMAKE_PREFIX_PATH=${PREFIX}"
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)
if(DEFINED SDL3_DIR AND NOT SDL3_DIR STREQUAL "")
	list(APPEND CONFIGURE_ARGS "-DSDL3_DIR=${SDL3_DIR}")
endif()
run("find_package(FNA3D) consumer: configure" "${CMAKE_COMMAND}" ${CONFIGURE_ARGS})
if(STEP_OK)
	run("find_package(FNA3D) consumer: build"
		"${CMAKE_COMMAND}" --build "${WORK_DIR}/cmake-consumer" --verbose)
endif()
if(STEP_OK)
	check_no_tree_paths("the find_package(FNA3D) consumer's build" "${LAST_OUTPUT}")
endif()
if(STEP_OK)
	run("find_package(FNA3D) consumer: run" "${WORK_DIR}/cmake-consumer/effect_client")
	message(STATUS "${LAST_OUTPUT}")
endif()

# pkg-config, with the prefix taken from where FNA3D.pc now lies
find_program(PKG_CONFIG NAMES pkg-config pkgconf)
if(NOT PKG_CONFIG)
	message(STATUS "pkg-config consumer: skipped, no pkg-config")
else()
	set(PC_PATH "${PREFIX}/${FNA3D_INSTALL_LIBDIR}/pkgconfig")
	set(RUN_LIBRARY_PATH "${PREFIX}/${FNA3D_INSTALL_LIBDIR}")
	if(DEFINED SDL3_DIR AND EXISTS "${SDL3_DIR}/../../pkgconfig/sdl3.pc")
		get_filename_component(SDL3_PC_DIR "${SDL3_DIR}/../../pkgconfig" ABSOLUTE)
		get_filename_component(SDL3_LIB_DIR "${SDL3_DIR}/../.." ABSOLUTE)
		string(APPEND PC_PATH ":${SDL3_PC_DIR}")
		string(APPEND RUN_LIBRARY_PATH ":${SDL3_LIB_DIR}")
	endif()
	set(ENV{PKG_CONFIG_PATH} "${PC_PATH}")
	run("pkg-config FNA3D" "${PKG_CONFIG}" --define-prefix --cflags --libs FNA3D)
	string(STRIP "${LAST_OUTPUT}" PC_FLAGS)
	if(STEP_OK)
		check_no_tree_paths("pkg-config's flags" "${PC_FLAGS}")
	endif()
	if(STEP_OK)
		separate_arguments(PC_FLAGS UNIX_COMMAND "${PC_FLAGS}")
		separate_arguments(EXTRA_C_FLAGS UNIX_COMMAND "${C_FLAGS}")
		separate_arguments(EXTRA_LINKER_FLAGS UNIX_COMMAND "${EXE_LINKER_FLAGS}")
		run("pkg-config consumer: build" "${C_COMPILER}" ${EXTRA_C_FLAGS}
			"${CONSUMER_SRC}/effect_client.c" -o "${WORK_DIR}/pkg-config-client"
			${PC_FLAGS} ${EXTRA_LINKER_FLAGS})
	endif()
	if(STEP_OK)
		set(ENV{LD_LIBRARY_PATH} "${RUN_LIBRARY_PATH}")
		run("pkg-config consumer: run" "${WORK_DIR}/pkg-config-client")
		message(STATUS "${LAST_OUTPUT}")
	endif()
endif()

if(NOT FAILED_STEPS STREQUAL "")
	string(REGEX REPLACE "^;" "" FAILED_STEPS "${FAILED_STEPS}")
	string(REPLACE ";" "; " FAILED_STEPS "${FAILED_STEPS}")
	message(FATAL_ERROR "installed consumer, failed: ${FAILED_STEPS}")
endif()
