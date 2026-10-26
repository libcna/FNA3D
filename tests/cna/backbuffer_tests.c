/* libcna fork regression test: ReadBackbuffer returns the rectangle asked for.
 *
 * GetBackBufferData's rectangle is in the backbuffer's top-left origin. The OpenGL driver handed
 * its y to glReadPixels unchanged, whose origin is the bottom-left corner, so any read but a full
 * one came from the mirrored rows (2026-10-26 dependency audit B2-10); and it resolved a
 * multisampled backbuffer with a blit that an enabled scissor test clipped, so the rest of the
 * read was stale (B2-06).
 *
 * Every pixel of the backbuffer is painted its own colour -- red and green from its row, blue
 * from its half -- by drawing one row half at a time through a scissor rectangle, which FNA3D
 * places in the same top-left origin. The scissor test stays enabled, with the last row's
 * rectangle, as a draw leaves it. Full and partial reads must then hold exactly those colours,
 * in order, and write nothing past the w * h * 4 bytes they need: on a backbuffer the size of
 * the window, on a smaller (faux) one, and on multisampled ones.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FNA3D.h"
#include "mojoshader.h"
#include "test_effect.h"

#define GUARD 0xA5
#define GUARD_BYTES 64
#define WINDOW_SIZE 64

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

typedef struct Config
{
	int32_t width;
	int32_t height;
	int32_t multiSampleCount;
	const char *name;
} Config;

static const Config configs[] =
{
	{ WINDOW_SIZE, WINDOW_SIZE, 0, "window-sized" },
	{ 48, 40, 0, "48x40 faux" },
	{ WINDOW_SIZE, WINDOW_SIZE, 4, "window-sized 4x MSAA" },
	{ 48, 40, 4, "48x40 4x MSAA" },
};

/* The colour every pixel is painted, in the backbuffer's top-left origin. */
static void Expected(const Config *config, int32_t x, int32_t y, uint8_t rgba[4])
{
	rgba[0] = (uint8_t) (y * 4);
	rgba[1] = (uint8_t) (255 - y * 4);
	rgba[2] = (x < config->width / 2) ? 64 : 192;
	rgba[3] = 255;
}

static uint8_t *GuardedBuffer(size_t bytes)
{
	uint8_t *buffer = (uint8_t*) malloc(bytes + GUARD_BYTES);
	memset(buffer, GUARD, bytes + GUARD_BYTES);
	return buffer;
}

static int GuardIntact(const uint8_t *buffer, size_t bytes)
{
	size_t i;
	for (i = bytes; i < bytes + GUARD_BYTES; i += 1)
	{
		if (buffer[i] != GUARD)
		{
			return 0;
		}
	}
	return 1;
}

static int Near(uint8_t actual, uint8_t expected)
{
	const int difference = (int) actual - (int) expected;
	return difference >= -1 && difference <= 1;
}

/* Reads (x, y, w, h) and compares every pixel with the painting; uniform is the colour to expect
 * everywhere instead, when not NULL.
 */
static void CheckRead(
	FNA3D_Device *device,
	const Config *config,
	int32_t x,
	int32_t y,
	int32_t w,
	int32_t h,
	const uint8_t *uniform,
	const char *what
) {
	const size_t bytes = (size_t) w * (size_t) h * 4;
	uint8_t *buffer = GuardedBuffer(bytes);
	int32_t row, column, wrong = 0;
	int32_t firstX = 0, firstY = 0;
	uint8_t firstGot[4] = { 0, 0, 0, 0 }, firstWanted[4] = { 0, 0, 0, 0 };

	FNA3D_ReadBackbuffer(device, x, y, w, h, buffer, (int32_t) bytes);
	CHECK(GuardIntact(buffer, bytes),
		"%s, %s: read (%d,%d %dx%d) wrote past its %zu bytes",
		config->name, what, x, y, w, h, bytes);
	for (row = 0; row < h; row += 1)
	{
		for (column = 0; column < w; column += 1)
		{
			const uint8_t *got = buffer + ((size_t) row * w + column) * 4;
			uint8_t wanted[4];
			if (uniform != NULL)
			{
				memcpy(wanted, uniform, 4);
			}
			else
			{
				Expected(config, x + column, y + row, wanted);
			}
			if (	!Near(got[0], wanted[0]) ||
				!Near(got[1], wanted[1]) ||
				!Near(got[2], wanted[2]) ||
				!Near(got[3], wanted[3])	)
			{
				if (wrong == 0)
				{
					firstX = x + column;
					firstY = y + row;
					memcpy(firstGot, got, 4);
					memcpy(firstWanted, wanted, 4);
				}
				wrong += 1;
			}
		}
	}
	CHECK(wrong == 0,
		"%s, %s: read (%d,%d %dx%d): %d of %d pixels wrong, first (%d,%d) is "
		"(%d,%d,%d,%d), expected (%d,%d,%d,%d)",
		config->name, what, x, y, w, h, wrong, w * h, firstX, firstY,
		firstGot[0], firstGot[1], firstGot[2], firstGot[3],
		firstWanted[0], firstWanted[1], firstWanted[2], firstWanted[3]);
	free(buffer);
}

static int RunConfig(SDL_Window *window, const Config *config)
{
	const int32_t width = config->width, height = config->height;
	const int32_t halves = height * 2;
	FNA3D_PresentationParameters parameters;
	FNA3D_Device *device;
	FNA3D_Effect *effect = NULL;
	MOJOSHADER_effect *effectData = NULL;
	MOJOSHADER_effectStateChanges stateChanges;
	FNA3D_Buffer *vertexBuffer;
	FNA3D_VertexElement elements[2];
	FNA3D_VertexBufferBinding binding;
	TestEffect code;
	TestEffect_Vertex *vertices;
	FNA3D_Vec4 clearColor = { 0.5f, 0.5f, 0.5f, 1.0f };
	const uint8_t cleared[4] = { 128, 128, 128, 255 };
	int32_t row, half, i;
	int32_t bbw = 0, bbh = 0;

	SDL_memset(&parameters, '\0', sizeof(parameters));
	parameters.backBufferWidth = width;
	parameters.backBufferHeight = height;
	parameters.backBufferFormat = FNA3D_SURFACEFORMAT_COLOR;
	parameters.multiSampleCount = config->multiSampleCount;
	parameters.deviceWindowHandle = window;
	parameters.depthStencilFormat = FNA3D_DEPTHFORMAT_NONE;
	parameters.presentationInterval = FNA3D_PRESENTINTERVAL_IMMEDIATE;
	device = FNA3D_CreateDevice(&parameters, 0);
	if (device == NULL)
	{
		fprintf(stderr, "%s: FNA3D_CreateDevice failed\n", config->name);
		return 0;
	}
	if (SDL_GL_GetCurrentContext() != NULL)
	{
		label = SDL_GetHintBoolean("FNA3D_OPENGL_FORCE_ES3", false) ? "OpenGL ES 3" : "OpenGL";
	}
	FNA3D_GetBackbufferSize(device, &bbw, &bbh);
	CHECK(bbw == width && bbh == height, "%s: backbuffer is %dx%d", config->name, bbw, bbh);
	if (config->multiSampleCount > 0)
	{
		printf("%s: %d sample(s)\n", config->name, FNA3D_GetBackbufferMultiSampleCount(device));
	}

	TestEffect_Build(&code, MOJOSHADER_USAGE_COLOR, 1);
	FNA3D_CreateEffect(device, (uint8_t*) code.words, code.length, &effect, &effectData);
	if (effect == NULL || effectData == NULL || effectData->error_count > 0)
	{
		fprintf(stderr, "%s: the test effect did not compile: %s\n", config->name,
			(effectData != NULL && effectData->error_count > 0) ?
				effectData->errors[0].error :
				"no effect");
		failures += 1;
		FNA3D_DestroyDevice(device);
		return 1;
	}

	/* One triangle per row half, in the colour that row half is painted. */
	vertices = (TestEffect_Vertex*) malloc(sizeof(TestEffect_Vertex) * 3 * (size_t) halves);
	for (row = 0; row < height; row += 1)
	{
		for (half = 0; half < 2; half += 1)
		{
			uint8_t rgba[4];
			float color[4];
			Expected(config, half * (width / 2), row, rgba);
			for (i = 0; i < 4; i += 1)
			{
				color[i] = rgba[i] / 255.0f;
			}
			TestEffect_FullscreenTriangle(&vertices[(row * 2 + half) * 3], color);
		}
	}
	vertexBuffer = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_WRITEONLY,
					     (int32_t) (sizeof(TestEffect_Vertex) * 3 * halves));
	FNA3D_SetVertexBufferData(device, vertexBuffer, 0, vertices, 3 * halves,
				  (int32_t) sizeof(TestEffect_Vertex),
				  (int32_t) sizeof(TestEffect_Vertex),
				  FNA3D_SETDATAOPTIONS_NONE);
	free(vertices);

	elements[0].offset = 0;
	elements[0].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR4;
	elements[0].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_POSITION;
	elements[0].usageIndex = 0;
	elements[1].offset = 16;
	elements[1].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR4;
	elements[1].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_COLOR;
	elements[1].usageIndex = 0;
	SDL_memset(&binding, '\0', sizeof(binding));
	binding.vertexBuffer = vertexBuffer;
	binding.vertexDeclaration.vertexStride = (int32_t) sizeof(TestEffect_Vertex);
	binding.vertexDeclaration.elementCount = 2;
	binding.vertexDeclaration.elements = elements;

	/* A whole read of the cleared backbuffer, the scissor test off: a multisampled backbuffer's
	 * resolve then holds the clear colour everywhere, so a later resolve that is clipped shows.
	 */
	TestEffect_ApplyDrawStates(device, width, height, 0);
	FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &clearColor, 1.0f, 0);
	CheckRead(device, config, 0, 0, width, height, cleared, "cleared");

	/* Paint, one row half at a time; the last rectangle stays the scissor. */
	TestEffect_ApplyDrawStates(device, width, height, 1);
	SDL_memset(&stateChanges, '\0', sizeof(stateChanges));
	FNA3D_ApplyEffect(device, effect, 0, &stateChanges);
	FNA3D_ApplyVertexBufferBindings(device, &binding, 1, 1, 0);
	for (row = 0; row < height; row += 1)
	{
		for (half = 0; half < 2; half += 1)
		{
			FNA3D_Rect scissor;
			scissor.x = half * (width / 2);
			scissor.y = row;
			scissor.w = width / 2;
			scissor.h = 1;
			FNA3D_SetScissorRect(device, &scissor);
			FNA3D_DrawPrimitives(device, FNA3D_PRIMITIVETYPE_TRIANGLELIST,
					     (row * 2 + half) * 3, 1);
		}
	}

	/* The whole backbuffer -- with the scissor still on (B2-06) -- then rectangles that are not
	 * symmetric about the middle row (B2-10): a band near the top, the top and bottom rows, an
	 * odd-sized block across both halves, the bottom-right corner, a single pixel.
	 */
	CheckRead(device, config, 0, 0, width, height, NULL, "painted, scissor on");
	CheckRead(device, config, 3, 8, width - 6, 4, NULL, "painted, scissor on");
	CheckRead(device, config, 0, 0, width, 1, NULL, "painted, scissor on");
	CheckRead(device, config, 0, height - 1, width, 1, NULL, "painted, scissor on");
	CheckRead(device, config, width / 2 - 3, 5, 7, height / 2, NULL, "painted, scissor on");
	CheckRead(device, config, width - 4, height - 14, 4, 14, NULL, "painted, scissor on");
	CheckRead(device, config, 1, 2, 1, 1, NULL, "painted, scissor on");

	/* And once more with the scissor test off, which the old code read correctly when full. */
	TestEffect_ApplyDrawStates(device, width, height, 0);
	CheckRead(device, config, 0, 0, width, height, NULL, "painted, scissor off");
	CheckRead(device, config, 2, height - 9, 11, 6, NULL, "painted, scissor off");

	FNA3D_AddDisposeVertexBuffer(device, vertexBuffer);
	FNA3D_AddDisposeEffect(device, effect);
	FNA3D_DestroyDevice(device);
	return 1;
}

int main(int argc, char **argv)
{
	SDL_Window *window;
	size_t i;

	(void) argc;
	(void) argv;
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	window = SDL_CreateWindow("fna3d_cna_backbuffer_tests", WINDOW_SIZE, WINDOW_SIZE,
				  FNA3D_PrepareWindowAttributes() | SDL_WINDOW_HIDDEN);
	if (window == NULL)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}

	for (i = 0; i < SDL_arraysize(configs); i += 1)
	{
		if (!RunConfig(window, &configs[i]))
		{
			return 1;
		}
	}

	SDL_DestroyWindow(window);
	SDL_Quit();

	printf("%s: %d check(s), %d failed\n", label, checks, failures);
	return (failures == 0) ? 0 : 1;
}
