/* The stand-in SDL_shadercross's C half, for tests/cna/static_shadercross.cmake: the functions
 * MojoShader's statically linked path calls, with SDL_shadercross's signatures. It translates
 * nothing. The formats it reports come from its C++ half (standin_spirv_cross.cpp), as the real
 * library's translation comes from SPIRV-Cross, a C++ archive linked only through it: a library
 * that links this archive must link the C++ runtime as well.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#include <SDL3_shadercross/SDL_shadercross.h>

extern unsigned int FNA3D_CNA_StandinSpirvFormats(void);

bool SDLCALL SDL_ShaderCross_Init(void)
{
	return true;
}

void SDLCALL SDL_ShaderCross_Quit(void)
{
}

SDL_GPUShaderFormat SDLCALL SDL_ShaderCross_GetSPIRVShaderFormats(void)
{
	return (SDL_GPUShaderFormat) FNA3D_CNA_StandinSpirvFormats();
}

SDL_ShaderCross_GraphicsShaderMetadata * SDLCALL SDL_ShaderCross_ReflectGraphicsSPIRV(
	const Uint8 *bytecode, size_t bytecode_size, SDL_PropertiesID props
) {
	(void) bytecode;
	(void) bytecode_size;
	(void) props;
	SDL_SetError("FNA3D's stand-in SDL_shadercross reflects nothing");
	return NULL;
}

SDL_GPUShader * SDLCALL SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
	SDL_GPUDevice *device, const SDL_ShaderCross_SPIRV_Info *info,
	const SDL_ShaderCross_GraphicsShaderResourceInfo *resource_info, SDL_PropertiesID props
) {
	(void) device;
	(void) info;
	(void) resource_info;
	(void) props;
	SDL_SetError("FNA3D's stand-in SDL_shadercross compiles nothing");
	return NULL;
}
