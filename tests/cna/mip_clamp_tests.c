/* SPDX-License-Identifier: Zlib */
/* Oracle: XNA SamplerState.Apply IL_0342..0355 passes MaxMipLevel as uint32.
 * A constant coordinate has zero derivatives: the minimum mip clamp alone
 * selects red/green/blue. This bypasses SpriteBatch and verifies each upload.
 */
#include "texture_fixture.h"
int main(void)
{
	static const uint8_t colors[3][4] = {{255,0,0,255}, {0,255,0,255}, {0,0,255,255}};
	static const int levels[] = {0,1,2,-1,99,0};
	const float uv[4] = {0.5f,0.5f,0,1};
	FNA3D_Texture *texture;
	uint8_t bytes[64], read[64], pixels[TW * TH * 4];
	int level, i, size;
	if (!Start()) return 1;
	if (!Program(2, uv)) return Finish();
	texture = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, 4, 4, 3, 0);
	for (level = 0; level < 3; level += 1)
	{
		size = 4 >> level;
		for (i = 0; i < size * size; i += 1) SDL_memcpy(bytes + i * 4, colors[level], 4);
		FNA3D_SetTextureData2D(device, texture, 0, 0, size, size, level, bytes, size * size * 4);
		SDL_memset(read, 0xAB, sizeof(read));
		FNA3D_GetTextureData2D(device, texture, 0, 0, size, size, level, read, size * size * 4);
		Check(SDL_memcmp(read, bytes, size * size * 4) == 0, "authored mip upload/readback identity");
	}
	for (i = 0; i < (int) SDL_arraysize(levels); i += 1)
	{
		int expected = levels[i] == 0 ? 0 : levels[i] == 1 ? 1 : 2;
		printf("mip clamp request=%d expected stored=%d\n", levels[i], expected);
		Draw(texture, levels[i], 0, TW);
		FNA3D_ReadBackbuffer(device, 0, 0, TW, TH, pixels, sizeof(pixels));
		ExpectPixel(pixels, TW / 2, colors[expected], "unsigned most-detailed allowed stored mip");
	}
	FNA3D_AddDisposeTexture(device, texture); EndProgram();
	return Finish();
}
