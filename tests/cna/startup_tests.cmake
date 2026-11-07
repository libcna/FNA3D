# SPDX-License-Identifier: Zlib
# Vulkan VUID-VkViewport-width-01770 forbids a zero-width viewport. Native
# validation writes directly to stderr, bypassing SDL_SetLogOutputFunction.
execute_process(COMMAND "${STARTUP_EXE}" RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE errors)
message("${output}${errors}")
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Native startup exited with ${result}")
endif()
if("${output}${errors}" MATCHES "Validation Error:|VUID-")
    message(FATAL_ERROR "Native startup emitted a GPU validation error")
endif()
