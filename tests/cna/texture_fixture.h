/* SPDX-License-Identifier: Zlib */
/* Native FNA3D sampling fixture. Literal Shader Model 2 tokens pass constant
 * coordinates to texld; there is no stock Effect or CNA state adapter involved.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include "FNA3D.h"
#include "mojoshader.h"
#include "test_effect.h"
#define TW 64
#define TH 32
static int failures;
static FNA3D_Device *device;
static SDL_Window *window;
static FNA3D_Effect *effect;
static FNA3D_Buffer *vb;
static void Check(int yes, const char *what)
{
	if (!yes) { fprintf(stderr, "FAILED: %s\n", what); failures += 1; }
}
static int Start(void)
{
	FNA3D_PresentationParameters p;
	if (!SDL_Init(SDL_INIT_VIDEO)) return 0;
	window = SDL_CreateWindow("fork texture contract", TW, TH,
		FNA3D_PrepareWindowAttributes() | SDL_WINDOW_HIDDEN);
	if (window == NULL) return 0;
	SDL_zero(p);
	p.backBufferWidth = TW; p.backBufferHeight = TH;
	p.backBufferFormat = FNA3D_SURFACEFORMAT_COLOR;
	p.depthStencilFormat = FNA3D_DEPTHFORMAT_NONE;
	p.deviceWindowHandle = window;
	p.presentationInterval = FNA3D_PRESENTINTERVAL_IMMEDIATE;
	device = FNA3D_CreateDevice(&p, 1);
	return device != NULL;
}
static int Program(int dimension, const float coordinates[4])
{
	uint32_t vs[16], ps[] = {
		0xFFFF0200, 0x0200001F, 0x80000000, 0xB00F0000,
		0x0200001F, 0, 0xA00F0800,
		0x03000042, 0x800F0000, 0xB0E40000, 0xA0E40800,
		0x02000001, 0x800F0800, 0x80E40000, 0x0000FFFF
	};
	TestEffect code;
	TestEffect_Vertex vertices[3];
	MOJOSHADER_effect *data = NULL;
	size_t vsCount = TestEffect_VertexShader(vs, MOJOSHADER_USAGE_COLOR);
	/* The second mov now writes oT0 rather than oD0. */
	vs[11] = 0xE00F0000;
	ps[5] = 0x80000000u | ((uint32_t) dimension << 27);
	TestEffect_BuildPrograms(&code, vs, vsCount, ps, SDL_arraysize(ps), 1);
	FNA3D_CreateEffect(device, (uint8_t*) code.words, code.length, &effect, &data);
	Check(effect != NULL && data != NULL && data->error_count == 0, "sampling effect compiles");
	if (effect == NULL || data == NULL || data->error_count != 0)
	{
		if (data != NULL && data->error_count > 0) fprintf(stderr, "%s\n", data->errors[0].error);
		if (effect != NULL) FNA3D_AddDisposeEffect(device, effect);
		return 0;
	}
	TestEffect_FullscreenTriangle(vertices, coordinates);
	vb = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_WRITEONLY, sizeof(vertices));
	Check(vb != NULL, "sampling vertex buffer exists");
	if (vb == NULL) { FNA3D_AddDisposeEffect(device, effect); return 0; }
	FNA3D_SetVertexBufferData(device, vb, 0, vertices, 3, sizeof(vertices[0]),
		sizeof(vertices[0]), FNA3D_SETDATAOPTIONS_NONE);
	return 1;
}
static void Draw(FNA3D_Texture *texture, int mip, int x, int width)
{
	FNA3D_VertexElement elements[2] = {
		{0, FNA3D_VERTEXELEMENTFORMAT_VECTOR4, FNA3D_VERTEXELEMENTUSAGE_POSITION, 0},
		{16, FNA3D_VERTEXELEMENTFORMAT_VECTOR4, FNA3D_VERTEXELEMENTUSAGE_COLOR, 0}
	};
	FNA3D_VertexBufferBinding binding;
	MOJOSHADER_effectStateChanges changes;
	FNA3D_SamplerState sampler;
	FNA3D_Rect scissor = { x, 0, width, TH };
	TestEffect_ApplyDrawStates(device, TW, TH, 1);
	SDL_zero(binding); SDL_zero(changes); SDL_zero(sampler);
	binding.vertexBuffer = vb;
	binding.vertexDeclaration.vertexStride = sizeof(TestEffect_Vertex);
	binding.vertexDeclaration.elementCount = 2;
	binding.vertexDeclaration.elements = elements;
	sampler.filter = FNA3D_TEXTUREFILTER_POINT;
	sampler.addressU = sampler.addressV = sampler.addressW = FNA3D_TEXTUREADDRESSMODE_CLAMP;
	sampler.maxAnisotropy = 1; sampler.maxMipLevel = mip;
	FNA3D_SetScissorRect(device, &scissor);
	FNA3D_ApplyEffect(device, effect, 0, &changes);
	FNA3D_VerifySampler(device, 0, texture, &sampler);
	FNA3D_ApplyVertexBufferBindings(device, &binding, 1, 1, 0);
	FNA3D_DrawPrimitives(device, FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 1);
}
static void ExpectPixel(const uint8_t *pixels, int x, const uint8_t rgba[4], const char *what)
{
	Check(SDL_memcmp(pixels + (TH / 2 * TW + x) * 4, rgba, 4) == 0, what);
}
static void EndProgram(void)
{
	FNA3D_AddDisposeVertexBuffer(device, vb);
	FNA3D_AddDisposeEffect(device, effect);
}
static int Finish(void)
{
	FNA3D_DestroyDevice(device); SDL_DestroyWindow(window); SDL_Quit();
	printf("texture contract: %d failure(s)\n", failures);
	return failures ? 1 : 0;
}
