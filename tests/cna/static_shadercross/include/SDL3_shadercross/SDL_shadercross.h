/* A stand-in for <SDL3_shadercross/SDL_shadercross.h>, for tests/cna/static_shadercross.cmake
 * only: the part of SDL_shadercross's API that MojoShader's SDL_GPU glue calls when FNA3D's
 * MOJOSHADER_STATIC_SPIRVCROSS selects the statically linked SDL_shadercross, with
 * SDL_shadercross's own types and signatures. standin_shadercross.c defines the functions.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#ifndef FNA3D_CNA_SDL_SHADERCROSS_STANDIN_H
#define FNA3D_CNA_SDL_SHADERCROSS_STANDIN_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SDL_ShaderCross_ShaderStage
{
	SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
	SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
	SDL_SHADERCROSS_SHADERSTAGE_COMPUTE
} SDL_ShaderCross_ShaderStage;

typedef struct SDL_ShaderCross_GraphicsShaderResourceInfo
{
	Uint32 num_samplers;
	Uint32 num_storage_textures;
	Uint32 num_storage_buffers;
	Uint32 num_uniform_buffers;
} SDL_ShaderCross_GraphicsShaderResourceInfo;

typedef struct SDL_ShaderCross_GraphicsShaderMetadata
{
	SDL_ShaderCross_GraphicsShaderResourceInfo resource_info;
	Uint32 num_inputs;
	void *inputs;
	Uint32 num_outputs;
	void *outputs;
} SDL_ShaderCross_GraphicsShaderMetadata;

typedef struct SDL_ShaderCross_SPIRV_Info
{
	const Uint8 *bytecode;
	size_t bytecode_size;
	const char *entrypoint;
	SDL_ShaderCross_ShaderStage shader_stage;
	SDL_PropertiesID props;
} SDL_ShaderCross_SPIRV_Info;

extern bool SDLCALL SDL_ShaderCross_Init(void);
extern void SDLCALL SDL_ShaderCross_Quit(void);
extern SDL_GPUShaderFormat SDLCALL SDL_ShaderCross_GetSPIRVShaderFormats(void);
extern SDL_ShaderCross_GraphicsShaderMetadata * SDLCALL SDL_ShaderCross_ReflectGraphicsSPIRV(
	const Uint8 *bytecode, size_t bytecode_size, SDL_PropertiesID props);
extern SDL_GPUShader * SDLCALL SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
	SDL_GPUDevice *device, const SDL_ShaderCross_SPIRV_Info *info,
	const SDL_ShaderCross_GraphicsShaderResourceInfo *resource_info, SDL_PropertiesID props);

#ifdef __cplusplus
}
#endif

#endif /* FNA3D_CNA_SDL_SHADERCROSS_STANDIN_H */
