/* libcna fork regression test: what the replay tool hands the CNA extension entry points.
 *
 * Preloaded into fna3d_replay (LD_PRELOAD) by trace_tests.cmake, this records every
 * FNA3D_SetXnaPixelCenterEXT and FNA3D_SetLegacyBumpMapEnvEXT call, with its arguments, to the
 * file FNA3D_CNA_TRACE_LOG names, and then makes the call. The test compares the log with what
 * trace_record.c captured: a replay that skipped or misread the marks would not match.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct FNA3D_Device FNA3D_Device;

static void Log(const char *format, ...) __attribute__((format(printf, 1, 2)));

static void Log(const char *format, ...)
{
	const char *path = getenv("FNA3D_CNA_TRACE_LOG");
	FILE *file;
	va_list args;
	if (path == NULL)
	{
		return;
	}
	file = fopen(path, "a");
	if (file == NULL)
	{
		return;
	}
	va_start(args, format);
	vfprintf(file, format, args);
	va_end(args);
	fclose(file);
}

void FNA3D_SetXnaPixelCenterEXT(FNA3D_Device *device, uint8_t enabled)
{
	void (*real)(FNA3D_Device*, uint8_t);
	*(void**) &real = dlsym(RTLD_NEXT, "FNA3D_SetXnaPixelCenterEXT");
	Log("SetXnaPixelCenterEXT %u\n", (unsigned int) enabled);
	if (real != NULL)
	{
		real(device, enabled);
	}
}

void FNA3D_SetLegacyBumpMapEnvEXT(
	FNA3D_Device *device,
	int32_t sampler,
	float mat00,
	float mat01,
	float mat10,
	float mat11,
	float luminanceScale,
	float luminanceOffset
) {
	void (*real)(FNA3D_Device*, int32_t, float, float, float, float, float, float);
	*(void**) &real = dlsym(RTLD_NEXT, "FNA3D_SetLegacyBumpMapEnvEXT");
	Log("SetLegacyBumpMapEnvEXT %d %g %g %g %g %g %g\n", (int) sampler,
	    mat00, mat01, mat10, mat11, luminanceScale, luminanceOffset);
	if (real != NULL)
	{
		real(device, sampler, mat00, mat01, mat10, mat11, luminanceScale, luminanceOffset);
	}
}
