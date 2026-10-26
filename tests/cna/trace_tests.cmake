# libcna fork regression test: a recorded trace replays and dumps as it was captured
# (TRACING_SUPPORT only). Run by ctest as `cmake -P` once per driver; see tests/cna/CMakeLists.txt
# for the variables. Every step runs, and the test fails at the end if any did.
#
#   1. fna3d_cna_trace_record records FNA3D_Trace.bin and finds the CNA extension calls in it,
#      each right after its string marker, with its arguments (B2 B-09: they were not traced).
#   2. fna3d_replay replays it with fna3d_cna_trace_interposer preloaded, which logs what the
#      replay hands FNA3D_SetXnaPixelCenterEXT and FNA3D_SetLegacyBumpMapEnvEXT; the log must be
#      exactly the captured calls (B2 B-09: replay did not know the marks). Where no interposer
#      can see those calls (INTERPOSER empty: a static FNA3D, or no LD_PRELOAD; INTERPOSER_SKIPPED
#      says which), the replay runs without it -- a mark it did not know still fails it -- and the
#      log comparison is skipped. Under AddressSanitizer
#      the replay runs with detect_stack_use_after_return=1, which catches the second frame's
#      pass change writing through the first frame's state-change record (B1 B-01: replay kept it
#      in a frame-local variable); without AddressSanitizer that write lands, harmlessly, in the
#      next frame's local, so it is caught only in a sanitizer build.
#   3. fna3d_dumpspirv dumps it, and fna3d_cna_spirv_check finds the Depth element linked as
#      DEPTH (A3a2 A-04: dumpspirv's own usage table had no Depth).
#
# SDL_ASSERT=abort makes an assertion in either tool -- "Unrecognized mark!", "Unrecognized
# VertexElementUsage!" -- end it with a failure instead of prompting.
#
# This file is part of the libcna fork of FNA3D; see tests/cna/README.md.

foreach(variable BUILD_DIR WORK_DIR RECORD REPLAY)
	if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
		message(FATAL_ERROR "trace_tests.cmake: ${variable} is not set")
	endif()
endforeach()

# WORK_DIR is removed and rewritten on every run, so it must be one of this test's own
# directories in the build tree and nothing else.
get_filename_component(WORK_NAME "${WORK_DIR}" NAME)
string(FIND "${WORK_DIR}" "${BUILD_DIR}/" WORK_IN_BUILD)
if(NOT WORK_NAME MATCHES "^trace-[a-z0-9_]+$" OR NOT WORK_IN_BUILD EQUAL 0)
	message(FATAL_ERROR "trace_tests.cmake: refusing to use ${WORK_DIR} as the work directory")
endif()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(TRACE "${WORK_DIR}/FNA3D_Trace.bin")

set(ENV{SDL_ASSERT} "abort")
set(FAILED_STEPS "")
function(run what)
	execute_process(COMMAND ${ARGN}
		WORKING_DIRECTORY "${WORK_DIR}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
	)
	if(NOT result EQUAL 0)
		message(STATUS "${what}: FAILED (${result}):\n${output}")
		set(FAILED_STEPS "${FAILED_STEPS};${what}" PARENT_SCOPE)
		set(STEP_OK FALSE PARENT_SCOPE)
		return()
	endif()
	message(STATUS "${what}: ok\n${output}")
	set(STEP_OK TRUE PARENT_SCOPE)
endfunction()

# 1. record
run("record" "${RECORD}")
if(NOT EXISTS "${TRACE}")
	message(FATAL_ERROR "no trace was written; nothing to replay")
endif()

# 2. replay
set(ORIGINAL_ASAN_OPTIONS "$ENV{ASAN_OPTIONS}")
set(REPLAY_ASAN_OPTIONS "detect_stack_use_after_return=1:verify_asan_link_order=0")
if(NOT ORIGINAL_ASAN_OPTIONS STREQUAL "")
	set(REPLAY_ASAN_OPTIONS "${ORIGINAL_ASAN_OPTIONS}:${REPLAY_ASAN_OPTIONS}")
endif()
if(NOT ASAN)
	message(STATUS "replay: not an AddressSanitizer build, so a write through a dead frame's "
		"state-change record (B1 B-01) cannot show")
endif()
set(ENV{ASAN_OPTIONS} "${REPLAY_ASAN_OPTIONS}")
if(DEFINED INTERPOSER AND NOT INTERPOSER STREQUAL "")
	set(ENV{LD_PRELOAD} "${INTERPOSER}")
	set(ENV{FNA3D_CNA_TRACE_LOG} "${WORK_DIR}/replay-calls.log")
endif()
run("replay" "${REPLAY}" "${TRACE}")
unset(ENV{LD_PRELOAD})
unset(ENV{FNA3D_CNA_TRACE_LOG})
set(ENV{ASAN_OPTIONS} "${ORIGINAL_ASAN_OPTIONS}")

set(EXPECTED_CALLS
"SetXnaPixelCenterEXT 1
SetLegacyBumpMapEnvEXT 3 0.5 -0.25 0.125 2 1.5 -0.75
SetXnaPixelCenterEXT 0
")
set(REPLAYED_CALLS "")
if(EXISTS "${WORK_DIR}/replay-calls.log")
	file(READ "${WORK_DIR}/replay-calls.log" REPLAYED_CALLS)
endif()
if(NOT DEFINED INTERPOSER OR INTERPOSER STREQUAL "")
	message(STATUS "replayed extension calls: SKIPPED, no interposer can observe them -- "
		"${INTERPOSER_SKIPPED}")
elseif(REPLAYED_CALLS STREQUAL EXPECTED_CALLS)
	message(STATUS "replayed extension calls: ok\n${REPLAYED_CALLS}")
else()
	message(STATUS "replayed extension calls: FAILED, expected\n${EXPECTED_CALLS}got\n${REPLAYED_CALLS}")
	set(FAILED_STEPS "${FAILED_STEPS};replayed extension calls")
endif()

# 3. dumpspirv
if(DEFINED DUMPSPIRV AND NOT DUMPSPIRV STREQUAL "")
	run("dumpspirv" "${DUMPSPIRV}" "${TRACE}")
	if(STEP_OK)
		run("dumpspirv's Depth element" "${SPIRV_CHECK}" "${TRACE}.spirv")
	endif()
endif()

if(NOT FAILED_STEPS STREQUAL "")
	string(REGEX REPLACE "^;" "" FAILED_STEPS "${FAILED_STEPS}")
	string(REPLACE ";" "; " FAILED_STEPS "${FAILED_STEPS}")
	message(FATAL_ERROR "trace tests, failed: ${FAILED_STEPS}")
endif()
