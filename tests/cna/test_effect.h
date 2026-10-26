/* libcna fork regression tests: a minimal Direct3D 9 Effects Framework binary, built in memory.
 *
 * Drawing through FNA3D takes an Effect, and no Effect compiler runs on Linux, so the tests build
 * the smallest one MOJOSHADER_compileEffect accepts straight from the format: one technique of
 * `passCount` passes, each binding the same vs_2_0 and ps_2_0 program, no parameters. The vertex
 * shader passes v0 (POSITION0) to oPos and v1 -- declared with the usage the caller names,
 * MOJOSHADER_USAGE_COLOR or _DEPTH, index 0 (MojoShader's usages are Direct3D 9's) -- to oD0;
 * the pixel shader writes its colour input v0 to oC0. So a draw paints exactly the colour its v1
 * stream carries.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#ifndef FNA3D_CNA_TEST_EFFECT_H
#define FNA3D_CNA_TEST_EFFECT_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "FNA3D.h"
#include "mojoshader.h"

#define TEST_EFFECT_MAX_PASSES 4
#define TEST_EFFECT_MAX_WORDS 128

typedef struct TestEffect
{
	uint32_t words[TEST_EFFECT_MAX_WORDS];
	uint32_t length; /* in bytes */
} TestEffect;

/* vs_2_0: dcl_position v0; dcl_<colorUsage> v1; mov oPos, v0; mov oD0, v1 */
static size_t TestEffect_VertexShader(uint32_t *tokens, MOJOSHADER_usage colorUsage)
{
	size_t n = 0;
	tokens[n++] = 0xFFFE0200;			/* vs_2_0 */
	tokens[n++] = 0x0200001F;			/* dcl */
	tokens[n++] = 0x80000000;			/*   POSITION0 */
	tokens[n++] = 0x900F0000;			/*   v0.xyzw */
	tokens[n++] = 0x0200001F;			/* dcl */
	tokens[n++] = 0x80000000 | (uint32_t) colorUsage;	/*   COLOR0 or DEPTH0 */
	tokens[n++] = 0x900F0001;			/*   v1.xyzw */
	tokens[n++] = 0x02000001;			/* mov */
	tokens[n++] = 0xC00F0000;			/*   oPos.xyzw */
	tokens[n++] = 0x90E40000;			/*   v0 */
	tokens[n++] = 0x02000001;			/* mov */
	tokens[n++] = 0xD00F0000;			/*   oD0.xyzw */
	tokens[n++] = 0x90E40001;			/*   v1 */
	tokens[n++] = 0x0000FFFF;			/* end */
	return n;
}

/* ps_2_0: dcl v0; mov oC0, v0 */
static size_t TestEffect_PixelShader(uint32_t *tokens)
{
	size_t n = 0;
	tokens[n++] = 0xFFFF0200;			/* ps_2_0 */
	tokens[n++] = 0x0200001F;			/* dcl */
	tokens[n++] = 0x80000000;
	tokens[n++] = 0x900F0000;			/*   v0.xyzw */
	tokens[n++] = 0x02000001;			/* mov */
	tokens[n++] = 0x800F0800;			/*   oC0.xyzw */
	tokens[n++] = 0x90E40000;			/*   v0 */
	tokens[n++] = 0x0000FFFF;			/* end */
	return n;
}

/* The binary: magic, the offset of the structure counts from the base (the byte after it), then
 * at the base an empty string (offset 0, every name), the two shader state values -- a type record
 * (type, class, name, semantic, elements) and the object index it holds -- the counts, the
 * technique and its passes, and the two shader objects.
 */
static void TestEffect_Build(TestEffect *effect, MOJOSHADER_usage colorUsage, uint32_t passCount)
{
	uint32_t vs[16], ps[16];
	const size_t vsCount = TestEffect_VertexShader(vs, colorUsage);
	const size_t psCount = TestEffect_PixelShader(ps);
	uint32_t *w = effect->words;
	size_t n = 0;
	uint32_t pass;

	if (passCount < 1) passCount = 1;
	if (passCount > TEST_EFFECT_MAX_PASSES) passCount = TEST_EFFECT_MAX_PASSES;

	w[n++] = 0xFEFF0901;
	w[n++] = 52;				/* counts at base + 52 */
	/* base + 0 */
	w[n++] = 0;				/* empty string */
	/* base + 4: vertex shader type, base + 24: its object */
	w[n++] = MOJOSHADER_SYMTYPE_VERTEXSHADER;
	w[n++] = MOJOSHADER_SYMCLASS_OBJECT;
	w[n++] = 0;
	w[n++] = 0;
	w[n++] = 0;
	w[n++] = 1;
	/* base + 28: pixel shader type, base + 48: its object */
	w[n++] = MOJOSHADER_SYMTYPE_PIXELSHADER;
	w[n++] = MOJOSHADER_SYMCLASS_OBJECT;
	w[n++] = 0;
	w[n++] = 0;
	w[n++] = 0;
	w[n++] = 2;
	/* base + 52: parameters, techniques, (unused), objects */
	w[n++] = 0;
	w[n++] = 1;
	w[n++] = 0;
	w[n++] = 3;
	/* technique: name, annotations, passes */
	w[n++] = 0;
	w[n++] = 0;
	w[n++] = passCount;
	for (pass = 0; pass < passCount; pass += 1)
	{
		/* pass: name, annotations, states; state: type, index, type offset, value offset */
		w[n++] = 0;
		w[n++] = 0;
		w[n++] = 2;
		w[n++] = MOJOSHADER_RS_VERTEXSHADER;
		w[n++] = 0;
		w[n++] = 4;
		w[n++] = 24;
		w[n++] = MOJOSHADER_RS_PIXELSHADER;
		w[n++] = 0;
		w[n++] = 28;
		w[n++] = 48;
	}
	/* small objects, large objects; each small object: index, byte length, bytes */
	w[n++] = 2;
	w[n++] = 0;
	w[n++] = 1;
	w[n++] = (uint32_t) (vsCount * 4);
	memcpy(&w[n], vs, vsCount * 4);
	n += vsCount;
	w[n++] = 2;
	w[n++] = (uint32_t) (psCount * 4);
	memcpy(&w[n], ps, psCount * 4);
	n += psCount;

	effect->length = (uint32_t) (n * 4);
}

/* Opaque, unculled, depth-free drawing into a width x height viewport, as XNA's
 * BlendState.Opaque, DepthStencilState.None and RasterizerState.CullNone (plus the scissor test
 * when asked for).
 */
static void TestEffect_ApplyDrawStates(FNA3D_Device *device, int32_t width, int32_t height,
				       uint8_t scissorTestEnable)
{
	FNA3D_Viewport viewport;
	FNA3D_BlendState blend;
	FNA3D_DepthStencilState depthStencil;
	FNA3D_RasterizerState rasterizer;

	viewport.x = 0;
	viewport.y = 0;
	viewport.w = width;
	viewport.h = height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	FNA3D_SetViewport(device, &viewport);

	memset(&blend, '\0', sizeof(blend));
	blend.colorSourceBlend = FNA3D_BLEND_ONE;
	blend.colorDestinationBlend = FNA3D_BLEND_ZERO;
	blend.colorBlendFunction = FNA3D_BLENDFUNCTION_ADD;
	blend.alphaSourceBlend = FNA3D_BLEND_ONE;
	blend.alphaDestinationBlend = FNA3D_BLEND_ZERO;
	blend.alphaBlendFunction = FNA3D_BLENDFUNCTION_ADD;
	blend.colorWriteEnable = FNA3D_COLORWRITECHANNELS_ALL;
	blend.colorWriteEnable1 = FNA3D_COLORWRITECHANNELS_ALL;
	blend.colorWriteEnable2 = FNA3D_COLORWRITECHANNELS_ALL;
	blend.colorWriteEnable3 = FNA3D_COLORWRITECHANNELS_ALL;
	blend.blendFactor.r = 255;
	blend.blendFactor.g = 255;
	blend.blendFactor.b = 255;
	blend.blendFactor.a = 255;
	blend.multiSampleMask = -1;
	FNA3D_SetBlendState(device, &blend);

	memset(&depthStencil, '\0', sizeof(depthStencil));
	depthStencil.depthBufferFunction = FNA3D_COMPAREFUNCTION_LESSEQUAL;
	depthStencil.stencilMask = -1;
	depthStencil.stencilWriteMask = -1;
	depthStencil.stencilFunction = FNA3D_COMPAREFUNCTION_ALWAYS;
	depthStencil.ccwStencilFunction = FNA3D_COMPAREFUNCTION_ALWAYS;
	FNA3D_SetDepthStencilState(device, &depthStencil);

	memset(&rasterizer, '\0', sizeof(rasterizer));
	rasterizer.fillMode = FNA3D_FILLMODE_SOLID;
	rasterizer.cullMode = FNA3D_CULLMODE_NONE;
	rasterizer.scissorTestEnable = scissorTestEnable;
	rasterizer.multiSampleAntiAlias = 1;
	FNA3D_ApplyRasterizerState(device, &rasterizer);
}

/* One vertex: a clip-space position and the colour the v1 input carries. */
typedef struct TestEffect_Vertex
{
	float position[4];
	float color[4];
} TestEffect_Vertex;

/* A triangle that covers the whole viewport, in one colour. */
static void TestEffect_FullscreenTriangle(TestEffect_Vertex *vertices, const float color[4])
{
	static const float corners[3][2] = { { -1.0f, -1.0f }, { 3.0f, -1.0f }, { -1.0f, 3.0f } };
	int i;
	for (i = 0; i < 3; i += 1)
	{
		vertices[i].position[0] = corners[i][0];
		vertices[i].position[1] = corners[i][1];
		vertices[i].position[2] = 0.5f;
		vertices[i].position[3] = 1.0f;
		memcpy(vertices[i].color, color, sizeof(vertices[i].color));
	}
}

#endif /* FNA3D_CNA_TEST_EFFECT_H */
