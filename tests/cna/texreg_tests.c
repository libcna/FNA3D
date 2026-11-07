/* SPDX-License-Identifier: Zlib */
/* D3D9 TEXREG2AR/GB/RGB sample the destination stage using source.AR/GB/RGB.
 * The first texture is (32,96,160,224). Point sampling a 4x4 grid therefore selects
 * (3,0), (1,2), (0,1): floor(4 * selected byte / 255). RGB_bx2 has
 * negative x/y and clamps to (0,0). These are hand-derived texels.
 * Instruction contracts: learn.microsoft.com/en-us/windows/win32/direct3dhlsl/
 * texreg2ar---ps, texreg2gb---ps, texreg2rgb---ps. */
#include "texture_fixture.h"

static int TexRegProgram(int opcode, int source, int destination, int modifier)
{
	uint32_t vs[] = {0xFFFE0200, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A,
					 0x900F0001, 0x02000001, 0xC00F0000, 0x90E40000, 0x02000001, 0xE00F0000,
					 0x90E40001, 0x02000001, 0xE00F0001, 0x90E40001, 0x02000001, 0xE00F0002,
					 0x90E40001, 0x02000001, 0xE00F0003, 0x90E40001, 0x0000FFFF};
	uint32_t ps[] = {0xFFFF0102,
					 66,
					 0xB00F0000u | (uint32_t)source,
					 (uint32_t)opcode,
					 0xB00F0000u | (uint32_t)destination,
					 0xB0E40000u | (uint32_t)source | ((uint32_t)modifier << 24),
					 1,
					 0x800F0000,
					 0xB0E40000u | (uint32_t)destination,
					 0x0000FFFF};
	const float uv[4] = {.5f, .5f, 0, 1};
	TestEffect code;
	TestEffect_Vertex vertices[3];
	MOJOSHADER_effect *data = NULL;
	TestEffect_BuildPrograms(&code, vs, SDL_arraysize(vs), ps, SDL_arraysize(ps), 1);
	FNA3D_CreateEffect(device, (uint8_t *)code.words, code.length, &effect, &data);
	Check(effect != NULL && data != NULL && data->error_count == 0, "texreg effect compiles");
	if (effect == NULL || data == NULL || data->error_count != 0)
	{
		if (data != NULL && data->error_count > 0)
			fprintf(stderr, "%s\n", data->errors[0].error);
		if (effect != NULL)
			FNA3D_AddDisposeEffect(device, effect);
		return 0;
	}
	TestEffect_FullscreenTriangle(vertices, uv);
	vb = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_WRITEONLY, sizeof(vertices));
	Check(vb != NULL, "texreg vertex buffer exists");
	if (vb == NULL)
	{
		FNA3D_AddDisposeEffect(device, effect);
		return 0;
	}
	FNA3D_SetVertexBufferData(device, vb, 0, vertices, 3, sizeof(vertices[0]), sizeof(vertices[0]),
							  FNA3D_SETDATAOPTIONS_NONE);
	return 1;
}

int main(void)
{
	static uint8_t first[] = {32, 96, 160, 224};
	static const uint8_t expected[][4] = {
		{160, 40, 91, 255}, {80, 120, 49, 255}, {40, 80, 14, 255}, {40, 40, 7, 255}};
	static const int opcodes[] = {69, 70, 82, 82};
	uint8_t grid[4 * 4 * 4], pixels[TW * TH * 4];
	FNA3D_Texture *sourceTexture, *lookupTexture;
	FNA3D_SamplerState sampler;
	FNA3D_Vec4 clear = {0, 0, 0, 0};
	int x, y, variant, source, destination, stage, cases = 0;
	if (!Start())
		return 1;
	TestEffect_ApplyDrawStates(device, TW, TH, 1);
	for (y = 0; y < 4; y++)
		for (x = 0; x < 4; x++)
		{
			uint8_t *p = grid + (y * 4 + x) * 4;
			p[0] = (uint8_t)((x + 1) * 40);
			p[1] = (uint8_t)((y + 1) * 40);
			p[2] = (uint8_t)((x * 4 + y + 1) * 7);
			p[3] = 255;
		}
	sourceTexture = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, 1, 1, 1, 0);
	lookupTexture = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, 4, 4, 1, 0);
	Check(sourceTexture != NULL && lookupTexture != NULL, "both test textures exist");
	if (sourceTexture == NULL || lookupTexture == NULL)
	{
		if (sourceTexture != NULL)
			FNA3D_AddDisposeTexture(device, sourceTexture);
		if (lookupTexture != NULL)
			FNA3D_AddDisposeTexture(device, lookupTexture);
		return Finish();
	}
	FNA3D_SetTextureData2D(device, sourceTexture, 0, 0, 1, 1, 0, first, sizeof(first));
	FNA3D_SetTextureData2D(device, lookupTexture, 0, 0, 4, 4, 0, grid, sizeof(grid));
	SDL_zero(sampler);
	sampler.filter = FNA3D_TEXTUREFILTER_POINT;
	sampler.addressU = sampler.addressV = sampler.addressW = FNA3D_TEXTUREADDRESSMODE_CLAMP;
	sampler.maxAnisotropy = 1;
	for (variant = 0; variant < 4; variant++)
		for (destination = 1; destination <= 3; destination++)
			for (source = 0; source < destination; source++)
			{
				int same = 1, i;
				printf("texreg opcode=%d src=%d dst=%d signed=%d\n", opcodes[variant], source,
					   destination, variant == 3);
				if (!TexRegProgram(opcodes[variant], source, destination, variant == 3 ? 4 : 0))
					continue;
				FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &clear, 1, 0);
				for (stage = 0; stage < 4; stage++)
					FNA3D_VerifySampler(device, stage, sourceTexture, &sampler);
				FNA3D_VerifySampler(device, destination, lookupTexture, &sampler);
				Draw(sourceTexture, 0, 0, TW);
				FNA3D_ReadBackbuffer(device, 0, 0, TW, TH, pixels, sizeof(pixels));
				for (i = 0; i < TW * TH; i++)
					if (SDL_memcmp(pixels + i * 4, expected[variant], 4))
					{
						same = 0;
						break;
					}
				if (!same)
					fprintf(stderr, "pixel%d=(%u,%u,%u,%u), expected(%u,%u,%u,%u)\n", i,
							pixels[i * 4], pixels[i * 4 + 1], pixels[i * 4 + 2], pixels[i * 4 + 3],
							expected[variant][0], expected[variant][1], expected[variant][2],
							expected[variant][3]);
				Check(same, "every texreg pixel matches the independent selected texel");
				EndProgram();
				cases++;
			}
	Check(cases == 24, "all opcode/register/signed variants rendered");
	FNA3D_AddDisposeTexture(device, sourceTexture);
	FNA3D_AddDisposeTexture(device, lookupTexture);
	return Finish();
}
