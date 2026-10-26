/* libcna fork regression test: a pixel shader's VFACE has Direct3D 9's sign, and the cull modes
 * remove the faces XNA removes, on every driver, into the backbuffer and into a render target.
 *
 * Direct3D 9 (and so XNA) calls a clockwise triangle the front face: VFACE is positive on it, and
 * CullClockwiseFace removes it. The OpenGL driver used to hand GLSL gl_FrontFacing, whose sense
 * follows glFrontFace (2026-10-26 audit D-01); the Direct3D 11 driver called the counter-clockwise
 * triangle the front face (FrontCounterClockwise = TRUE), so SV_IsFrontFace -- VFACE -- was
 * positive on the counter-clockwise one (2026-10-26 remediation review, round 2, R8-01). The pixel
 * shader paints green where VFACE >= 0 and red where it is negative; a clockwise and a
 * counter-clockwise triangle are drawn side by side under each cull mode.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FNA3D.h"
#include "mojoshader.h"
#include "test_effect.h"

#define WIDTH 64
#define HEIGHT 32

static int failures = 0;
static int checks = 0;
static const char *label = "Non-GL driver";

#define CHECK(condition, ...) \
	do { \
		checks += 1; \
		if (!(condition)) \
		{ \
			failures += 1; \
			fprintf(stderr, "%s:%d: FAILED: ", __FILE__, __LINE__); \
			fprintf(stderr, __VA_ARGS__); \
			fprintf(stderr, "\n"); \
		} \
	} while (0)

#define DST(type, num, mask) (0x80000000u | (((type) & 7u) << 28) | (((type) & 0x18u) << 8) | \
			      ((uint32_t) (mask) << 16) | (uint32_t) (num))
#define SRC(type, num, swz) (DST(type, num, 0) | ((uint32_t) (swz) << 16))
#define OP(op, len) ((uint32_t) (op) | ((uint32_t) (len) << 24))
#define DCL(usage, index) (0x80000000u | ((uint32_t) (index) << 16) | (uint32_t) (usage))

/* vs_3_0: dcl_position v0; dcl_position o0; mov o0, v0 */
static const uint32_t vs3[] =
{
	0xFFFE0300u,
	OP(31, 2), DCL(0, 0), DST(1, 0, 15),
	OP(31, 2), DCL(0, 0), DST(6, 0, 15),
	OP(1, 2), DST(6, 0, 15), SRC(1, 0, 0xE4),
	0x0000FFFFu
};

/* ps_3_0: def c0 = green; def c1 = red; dcl vFace; cmp oC0, vFace.x, c0, c1 */
static const uint32_t psVface[] =
{
	0xFFFF0300u,
	OP(81, 5), DST(2, 0, 15), 0x00000000u, 0x3F800000u, 0x00000000u, 0x3F800000u,
	OP(81, 5), DST(2, 1, 15), 0x3F800000u, 0x00000000u, 0x00000000u, 0x3F800000u,
	OP(31, 2), 0x80000000u, DST(17, 1, 15),
	OP(88, 4), DST(8, 0, 15), SRC(17, 1, 0x00), SRC(2, 0, 0xE4), SRC(2, 1, 0xE4),
	0x0000FFFFu
};

/* Left: clockwise on screen (bottom-left, top, bottom-right). Right: counter-clockwise. */
static const float vertices[6][4] =
{
	{ -0.9f, -0.5f, 0.5f, 1.0f }, { -0.5f, 0.5f, 0.5f, 1.0f }, { -0.1f, -0.5f, 0.5f, 1.0f },
	{  0.1f, -0.5f, 0.5f, 1.0f }, {  0.9f, -0.5f, 0.5f, 1.0f }, {  0.5f, 0.5f, 0.5f, 1.0f },
};

enum { CLEARED, GREEN, RED };
static const uint8_t colours[3][4] = { { 0, 0, 255, 255 }, { 0, 255, 0, 255 }, { 255, 0, 0, 255 } };
static const char *names[3] = { "the clear colour (culled)", "green (VFACE positive)",
				"red (VFACE negative)" };

static void Draw(
	FNA3D_Device *device,
	FNA3D_Effect *effect,
	FNA3D_Buffer *vertexBuffer,
	FNA3D_CullMode cullMode
) {
	FNA3D_Vec4 clearColor = { 0.0f, 0.0f, 1.0f, 1.0f };
	FNA3D_RasterizerState rasterizer;
	MOJOSHADER_effectStateChanges stateChanges;
	FNA3D_VertexElement element;
	FNA3D_VertexBufferBinding binding;

	TestEffect_ApplyDrawStates(device, WIDTH, HEIGHT, 0);
	SDL_memset(&rasterizer, '\0', sizeof(rasterizer));
	rasterizer.fillMode = FNA3D_FILLMODE_SOLID;
	rasterizer.cullMode = cullMode;
	rasterizer.multiSampleAntiAlias = 1;
	FNA3D_ApplyRasterizerState(device, &rasterizer);
	FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &clearColor, 1.0f, 0);

	element.offset = 0;
	element.vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR4;
	element.vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_POSITION;
	element.usageIndex = 0;
	SDL_memset(&binding, '\0', sizeof(binding));
	binding.vertexBuffer = vertexBuffer;
	binding.vertexDeclaration.vertexStride = (int32_t) sizeof(vertices[0]);
	binding.vertexDeclaration.elementCount = 1;
	binding.vertexDeclaration.elements = &element;

	SDL_memset(&stateChanges, '\0', sizeof(stateChanges));
	FNA3D_ApplyEffect(device, effect, 0, &stateChanges);
	FNA3D_ApplyVertexBufferBindings(device, &binding, 1, 1, 0);
	FNA3D_DrawPrimitives(device, FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 2);
}

static int Is(const uint8_t *pixel, int colour)
{
	int c;
	for (c = 0; c < 4; c += 1)
	{
		const int difference = (int) pixel[c] - (int) colours[colour][c];
		if (difference < -2 || difference > 2)
		{
			return 0;
		}
	}
	return 1;
}

/* The centre of each triangle: (16, 18) and (48, 18) in top-left window pixels. */
static void Expect(const uint8_t *pixels, int left, int right, const char *what)
{
	const uint8_t *l = pixels + ((18 * WIDTH) + 16) * 4;
	const uint8_t *r = pixels + ((18 * WIDTH) + 48) * 4;
	CHECK(Is(l, left), "%s: the clockwise triangle should be %s, got (%d,%d,%d,%d)",
		what, names[left], l[0], l[1], l[2], l[3]);
	CHECK(Is(r, right), "%s: the counter-clockwise triangle should be %s, got (%d,%d,%d,%d)",
		what, names[right], r[0], r[1], r[2], r[3]);
}

static void Run(FNA3D_Device *device, FNA3D_Effect *effect, FNA3D_Buffer *vertexBuffer,
		FNA3D_Texture *target, const char *where)
{
	static const struct { FNA3D_CullMode mode; int left, right; const char *name; } cases[] =
	{
		{ FNA3D_CULLMODE_NONE,				GREEN,	 RED,	  "CullMode.None" },
		{ FNA3D_CULLMODE_CULLCLOCKWISEFACE,		CLEARED, RED,	  "CullClockwiseFace" },
		{ FNA3D_CULLMODE_CULLCOUNTERCLOCKWISEFACE,	GREEN,	 CLEARED, "CullCounterClockwiseFace" },
	};
	uint8_t pixels[WIDTH * HEIGHT * 4];
	char what[96];
	size_t i;

	for (i = 0; i < SDL_arraysize(cases); i += 1)
	{
		FNA3D_RenderTargetBinding binding;
		SDL_snprintf(what, sizeof(what), "%s, %s", where, cases[i].name);
		if (target != NULL)
		{
			SDL_memset(&binding, '\0', sizeof(binding));
			binding.type = FNA3D_RENDERTARGET_TYPE_2D;
			binding.twod.width = WIDTH;
			binding.twod.height = HEIGHT;
			binding.levelCount = 1;
			binding.texture = target;
			FNA3D_SetRenderTargets(device, &binding, 1, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
		}
		Draw(device, effect, vertexBuffer, cases[i].mode);
		SDL_memset(pixels, '\0', sizeof(pixels));
		if (target != NULL)
		{
			FNA3D_SetRenderTargets(device, NULL, 0, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
			FNA3D_GetTextureData2D(device, target, 0, 0, WIDTH, HEIGHT, 0, pixels,
					       (int32_t) sizeof(pixels));
		}
		else
		{
			FNA3D_ReadBackbuffer(device, 0, 0, WIDTH, HEIGHT, pixels, (int32_t) sizeof(pixels));
		}
		Expect(pixels, cases[i].left, cases[i].right, what);
	}
}

int main(int argc, char **argv)
{
	FNA3D_PresentationParameters parameters;
	FNA3D_Device *device;
	SDL_Window *window;
	FNA3D_Effect *effect = NULL;
	MOJOSHADER_effect *effectData = NULL;
	FNA3D_Buffer *vertexBuffer;
	FNA3D_Texture *target;
	TestEffect code;

	(void) argc;
	(void) argv;
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	window = SDL_CreateWindow("fna3d_cna_vface_tests", WIDTH, HEIGHT,
				  FNA3D_PrepareWindowAttributes() | SDL_WINDOW_HIDDEN);
	if (window == NULL)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_memset(&parameters, '\0', sizeof(parameters));
	parameters.backBufferWidth = WIDTH;
	parameters.backBufferHeight = HEIGHT;
	parameters.backBufferFormat = FNA3D_SURFACEFORMAT_COLOR;
	parameters.deviceWindowHandle = window;
	parameters.depthStencilFormat = FNA3D_DEPTHFORMAT_NONE;
	parameters.presentationInterval = FNA3D_PRESENTINTERVAL_IMMEDIATE;
	device = FNA3D_CreateDevice(&parameters, 0);
	if (device == NULL)
	{
		fprintf(stderr, "FNA3D_CreateDevice failed\n");
		return 1;
	}
	if (SDL_GL_GetCurrentContext() != NULL)
	{
		label = SDL_GetHintBoolean("FNA3D_OPENGL_FORCE_ES3", false) ? "OpenGL ES 3" : "OpenGL";
	}

	TestEffect_BuildPrograms(&code, vs3, SDL_arraysize(vs3), psVface, SDL_arraysize(psVface), 1);
	FNA3D_CreateEffect(device, (uint8_t*) code.words, code.length, &effect, &effectData);
	if (effect == NULL || effectData == NULL || effectData->error_count > 0)
	{
		fprintf(stderr, "the VFACE test effect did not compile: %s\n",
			(effectData != NULL && effectData->error_count > 0) ?
				effectData->errors[0].error : "no effect");
		return 1;
	}
	vertexBuffer = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_WRITEONLY,
					     (int32_t) sizeof(vertices));
	FNA3D_SetVertexBufferData(device, vertexBuffer, 0, vertices, 6,
				  (int32_t) sizeof(vertices[0]), (int32_t) sizeof(vertices[0]),
				  FNA3D_SETDATAOPTIONS_NONE);
	target = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, WIDTH, HEIGHT, 1, 1);

	Run(device, effect, vertexBuffer, NULL, "backbuffer");
	Run(device, effect, vertexBuffer, target, "render target");

	FNA3D_AddDisposeTexture(device, target);
	FNA3D_AddDisposeVertexBuffer(device, vertexBuffer);
	FNA3D_AddDisposeEffect(device, effect);
	FNA3D_DestroyDevice(device);
	SDL_DestroyWindow(window);
	SDL_Quit();

	printf("%s: %d check(s), %d failed\n", label, checks, failures);
	return (failures == 0) ? 0 : 1;
}
