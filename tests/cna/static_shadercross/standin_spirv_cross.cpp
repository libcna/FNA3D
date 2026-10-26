/* The stand-in SDL_shadercross's C++ half, for tests/cna/static_shadercross.cmake, in place of
 * SPIRV-Cross: it needs the C++ runtime (a std::string, constructed once behind a guard), so a
 * library linking it without that runtime fails to link or to load. The formats are DXBC and
 * METALLIB, which MojoShader on Linux never reports of its own.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#include <string>

static const std::string &Name()
{
	static const std::string name("FNA3D's stand-in for SPIRV-Cross");
	return name;
}

extern "C" unsigned int FNA3D_CNA_StandinSpirvFormats(void)
{
	/* SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_METALLIB */
	return Name().empty() ? 0u : (0x4u | 0x20u);
}
