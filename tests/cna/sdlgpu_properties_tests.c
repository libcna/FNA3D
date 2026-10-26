/* libcna fork regression test: the SDL_GPU driver asks SDL for a device that takes every shader
 * format MojoShader can produce.
 *
 * SDLGPU_INTERNAL_FillProperties turns MOJOSHADER_sdlGetShaderFormats() into the
 * SDL_PROP_GPU_DEVICE_CREATE_SHADERS_*_BOOLEAN properties SDL_CreateGPUDeviceWithProperties
 * chooses a backend by. It set PRIVATE, SPIRV, DXIL, MSL and METALLIB but never DXBC, which
 * SDL_shadercross offers once its Init has loaded d3dcompiler (FXC), DXC or not -- and SDL's
 * D3D12 backend refuses a device that says neither DXIL nor DXBC (2026-10-26 remediation review,
 * round 2, R7-03).
 *
 * No device and no display: the driver is compiled into the test, with MojoShader's format query
 * replaced by one that returns what the test asks for, FNA3D_PipelineCache.c and MojoShader
 * alongside, and FNA3D's two log functions defined here.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#include <stdio.h>
#include <SDL3/SDL.h>

static unsigned int testFormats = 0;

#define MOJOSHADER_sdlGetShaderFormats CNATest_GetShaderFormats
#define SDLGPUDriver CNATest_SDLGPUDriver
#include "FNA3D_Driver_SDL.c"
#undef SDLGPUDriver
#undef MOJOSHADER_sdlGetShaderFormats

unsigned int CNATest_GetShaderFormats(void)
{
	return testFormats;
}

void FNA3D_LogWarn(const char *fmt, ...)
{
	(void) fmt;
}

void FNA3D_LogError(const char *fmt, ...)
{
	(void) fmt;
}

typedef struct FormatProperty
{
	SDL_GPUShaderFormat format;
	const char *property;
	const char *name;
} FormatProperty;

static const FormatProperty formatProperties[] =
{
	{ SDL_GPU_SHADERFORMAT_PRIVATE, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_PRIVATE_BOOLEAN, "PRIVATE" },
	{ SDL_GPU_SHADERFORMAT_SPIRV, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, "SPIRV" },
	{ SDL_GPU_SHADERFORMAT_DXBC, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOLEAN, "DXBC" },
	{ SDL_GPU_SHADERFORMAT_DXIL, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, "DXIL" },
	{ SDL_GPU_SHADERFORMAT_MSL, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, "MSL" },
	{ SDL_GPU_SHADERFORMAT_METALLIB, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_METALLIB_BOOLEAN, "METALLIB" },
};

static int checks = 0;
static int failures = 0;

/* Every format property must say whether MojoShader offers that format, read as SDL reads it:
 * a property not set is false.
 */
static void Check(unsigned int formats)
{
	SDL_PropertiesID props;
	size_t i;

	testFormats = formats;
	props = SDLGPU_INTERNAL_FillProperties(false);
	for (i = 0; i < SDL_arraysize(formatProperties); i += 1)
	{
		const bool offered = (formats & formatProperties[i].format) != 0;
		const bool asked = SDL_GetBooleanProperty(props, formatProperties[i].property, false);
		checks += 1;
		if (asked != offered)
		{
			failures += 1;
			fprintf(stderr, "FAILED: formats 0x%02X: %s is %s, MojoShader %s it\n",
				formats, formatProperties[i].name,
				SDL_HasProperty(props, formatProperties[i].property) ?
					(asked ? "true" : "false") : "not set",
				offered ? "offers" : "does not offer");
		}
	}
	SDL_DestroyProperties(props);
}

int main(int argc, char **argv)
{
	size_t i;

	(void) argc;
	(void) argv;
	Check(0);
	for (i = 0; i < SDL_arraysize(formatProperties); i += 1)
	{
		Check(formatProperties[i].format);
	}
	/* SDL_shadercross without DXC, initialised with FXC: SPIR-V passed through, DXBC and MSL. */
	Check(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_MSL);
	Check(0x3F);

	printf("SDL_GPU device properties: %d check(s), %d failed\n", checks, failures);
	return (failures == 0) ? 0 : 1;
}
