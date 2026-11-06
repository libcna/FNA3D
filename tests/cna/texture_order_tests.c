/* SPDX-License-Identifier: Zlib */
/* Oracle: ordered SetData/draw API calls. Initial red must survive in the left
 * draw; the update is green only in the right draw. Readback happens last.
 */
#include "texture_fixture.h"
static const uint8_t red[4] = {255, 0, 0, 255}, green[4] = {0, 255, 0, 255};
static void Upload(FNA3D_Texture *texture, int kind, int level, const uint8_t rgba[4])
{
	uint8_t bytes[4 * 4 * 4 * 4];
	int size = 4 >> level, i, count = size * size * (kind == 4 ? size : 1);
	for (i = 0; i < count; i += 1) SDL_memcpy(bytes + i * 4, rgba, 4);
	if (kind == 3)
		FNA3D_SetTextureDataCube(device, texture, 0, 0, size, size,
			FNA3D_CUBEMAPFACE_POSITIVEX, level, bytes, count * 4);
	else if (kind == 4)
		FNA3D_SetTextureData3D(device, texture, 0, 0, 0, size, size, size, level, bytes, count * 4);
	else FNA3D_SetTextureData2D(device, texture, 0, 0, size, size, level, bytes, count * 4);
}
static void Run(int kind, int levels, int selected)
{
	const float uv[4] = {kind == 3 ? 1.0f : 0.5f, kind == 3 ? 0.0f : 0.5f,
		kind == 3 ? 0.0f : 0.5f, 1.0f};
	FNA3D_Texture *texture;
	uint8_t pixels[TW * TH * 4];
	FNA3D_Vec4 black = {0, 0, 0, 1};
	int level;
	printf("ordering kind=%d levels=%d sampled=%d\n", kind, levels, selected);
	if (!Program(kind, uv)) return;
	texture = kind == 3 ? FNA3D_CreateTextureCube(device, FNA3D_SURFACEFORMAT_COLOR, 4, levels, 0) :
		kind == 4 ? FNA3D_CreateTexture3D(device, FNA3D_SURFACEFORMAT_COLOR, 4, 4, 4, levels) :
		FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, 4, 4, levels, 0);
	for (level = 0; level < levels; level += 1) Upload(texture, kind, level, red);
	FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &black, 1, 0);
	Draw(texture, selected, 0, TW / 2);
	/* No readback, swap or CNA workaround between these calls. */
	Upload(texture, kind, selected, green);
	Draw(texture, selected, TW / 2, TW / 2);
	FNA3D_ReadBackbuffer(device, 0, 0, TW, TH, pixels, sizeof(pixels));
	ExpectPixel(pixels, TW / 4, red, "draw before update retains red");
	ExpectPixel(pixels, 3 * TW / 4, green, "draw after update sees green");
	FNA3D_AddDisposeTexture(device, texture); EndProgram();
}
int main(void)
{
	if (!Start()) { fprintf(stderr, "device unavailable: %s\n", SDL_GetError()); return 1; }
	Run(2, 1, 0); /* existing whole-texture cycling control */
	Run(2, 3, 0); Run(2, 3, 1); /* mipmapped, base and authored level */
	Run(3, 1, 0); Run(3, 3, 1); /* cube */
	Run(4, 1, 0); Run(4, 3, 1); /* volume */
	return Finish();
}
