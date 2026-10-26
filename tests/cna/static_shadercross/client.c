/* A C client of the libFNA3D that tests/cna/static_shadercross.cmake builds with
 * MOJOSHADER_STATIC_SPIRVCROSS against the stand-in SDL_shadercross: MojoShader's shader formats
 * must include the stand-in's DXBC and METALLIB, which only its statically linked archive, called
 * from inside libFNA3D, reports. Linked by the C driver, so the C++ runtime that archive needs
 * must come with libFNA3D itself. No device.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#include <stdio.h>

/* mojoshader.h's declaration; libFNA3D links MojoShader and exports it. */
extern unsigned int MOJOSHADER_sdlGetShaderFormats(void);

int main(void)
{
	const unsigned int standin = 0x4u | 0x20u; /* DXBC | METALLIB */
	const unsigned int formats = MOJOSHADER_sdlGetShaderFormats();
	printf("MOJOSHADER_sdlGetShaderFormats() = 0x%02X\n", formats);
	return ((formats & standin) == standin) ? 0 : 1;
}
