/* libcna fork regression test: a Depth vertex element feeds the shader's DEPTH input.
 *
 * VertexAttribUsage() had no case for VertexElementUsage.Depth: it returned 0, which is
 * MOJOSHADER_USAGE_POSITION, so the drivers looked up the Position input for a Depth stream --
 * binding it over the position, or leaving the DEPTH input unbound (2026-10-26 dependency audit
 * A3a2 A-04).
 *
 * A vertex shader that declares dcl_depth v1 passes v1 to the colour output; a whole-viewport
 * triangle whose Depth stream carries one colour must paint the backbuffer that colour, with the
 * Depth element after the Position element and before it. The same draw through dcl_color v1
 * and a Color element is the control.
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

#define SIZE 32

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

static int Near(uint8_t actual, uint8_t expected)
{
	const int difference = (int) actual - (int) expected;
	return difference >= -1 && difference <= 1;
}

/* Draws the triangle through an effect whose v1 is declared with shaderUsage, the stream
 * described with elementUsage, Position first or second, and checks every pixel.
 */
static void DrawAndCheck(
	FNA3D_Device *device,
	MOJOSHADER_usage shaderUsage,
	FNA3D_VertexElementUsage elementUsage,
	int positionFirst,
	const char *what
) {
	static const float color[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
	static const uint8_t expected[4] = { 64, 128, 191, 255 };
	FNA3D_Vec4 clearColor = { 1.0f, 0.0f, 1.0f, 1.0f };
	FNA3D_Effect *effect = NULL;
	MOJOSHADER_effect *effectData = NULL;
	MOJOSHADER_effectStateChanges stateChanges;
	FNA3D_Buffer *vertexBuffer;
	FNA3D_VertexElement elements[2];
	FNA3D_VertexElement *position = &elements[positionFirst ? 0 : 1];
	FNA3D_VertexElement *stream = &elements[positionFirst ? 1 : 0];
	FNA3D_VertexBufferBinding binding;
	TestEffect code;
	TestEffect_Vertex vertices[3];
	uint8_t pixels[SIZE * SIZE * 4];
	int32_t i, wrong = 0, first = -1;

	TestEffect_Build(&code, shaderUsage, 1);
	FNA3D_CreateEffect(device, (uint8_t*) code.words, code.length, &effect, &effectData);
	if (effect == NULL || effectData == NULL || effectData->error_count > 0)
	{
		CHECK(0, "%s: the test effect did not compile: %s", what,
			(effectData != NULL && effectData->error_count > 0) ?
				effectData->errors[0].error :
				"no effect");
		return;
	}

	TestEffect_FullscreenTriangle(vertices, color);
	vertexBuffer = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_WRITEONLY,
					     (int32_t) sizeof(vertices));
	FNA3D_SetVertexBufferData(device, vertexBuffer, 0, vertices, 3,
				  (int32_t) sizeof(TestEffect_Vertex),
				  (int32_t) sizeof(TestEffect_Vertex),
				  FNA3D_SETDATAOPTIONS_NONE);

	position->offset = 0;
	position->vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR4;
	position->vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_POSITION;
	position->usageIndex = 0;
	stream->offset = 16;
	stream->vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR4;
	stream->vertexElementUsage = elementUsage;
	stream->usageIndex = 0;
	SDL_memset(&binding, '\0', sizeof(binding));
	binding.vertexBuffer = vertexBuffer;
	binding.vertexDeclaration.vertexStride = (int32_t) sizeof(TestEffect_Vertex);
	binding.vertexDeclaration.elementCount = 2;
	binding.vertexDeclaration.elements = elements;

	TestEffect_ApplyDrawStates(device, SIZE, SIZE, 0);
	FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &clearColor, 1.0f, 0);
	SDL_memset(&stateChanges, '\0', sizeof(stateChanges));
	FNA3D_ApplyEffect(device, effect, 0, &stateChanges);
	FNA3D_ApplyVertexBufferBindings(device, &binding, 1, 1, 0);
	FNA3D_DrawPrimitives(device, FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 1);

	SDL_memset(pixels, '\0', sizeof(pixels));
	FNA3D_ReadBackbuffer(device, 0, 0, SIZE, SIZE, pixels, (int32_t) sizeof(pixels));
	for (i = 0; i < SIZE * SIZE; i += 1)
	{
		const uint8_t *got = pixels + i * 4;
		if (	!Near(got[0], expected[0]) ||
			!Near(got[1], expected[1]) ||
			!Near(got[2], expected[2]) ||
			!Near(got[3], expected[3])	)
		{
			if (first < 0)
			{
				first = i;
			}
			wrong += 1;
		}
	}
	CHECK(wrong == 0,
		"%s: %d of %d pixels are not the stream's colour (64,128,191,255); pixel %d is "
		"(%d,%d,%d,%d)",
		what, wrong, SIZE * SIZE, first,
		(first < 0) ? 0 : pixels[first * 4 + 0],
		(first < 0) ? 0 : pixels[first * 4 + 1],
		(first < 0) ? 0 : pixels[first * 4 + 2],
		(first < 0) ? 0 : pixels[first * 4 + 3]);

	/* An effect is no longer current once the next one is applied; dispose of both now. */
	FNA3D_AddDisposeVertexBuffer(device, vertexBuffer);
	FNA3D_AddDisposeEffect(device, effect);
}

int main(int argc, char **argv)
{
	FNA3D_PresentationParameters parameters;
	FNA3D_Device *device;
	SDL_Window *window;

	(void) argc;
	(void) argv;
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	window = SDL_CreateWindow("fna3d_cna_vertex_usage_tests", SIZE, SIZE,
				  FNA3D_PrepareWindowAttributes() | SDL_WINDOW_HIDDEN);
	if (window == NULL)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}

	SDL_memset(&parameters, '\0', sizeof(parameters));
	parameters.backBufferWidth = SIZE;
	parameters.backBufferHeight = SIZE;
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

	DrawAndCheck(device, MOJOSHADER_USAGE_COLOR, FNA3D_VERTEXELEMENTUSAGE_COLOR, 1,
		     "Color stream, after Position (control)");
	DrawAndCheck(device, MOJOSHADER_USAGE_DEPTH, FNA3D_VERTEXELEMENTUSAGE_DEPTH, 1,
		     "Depth stream, after Position");
	DrawAndCheck(device, MOJOSHADER_USAGE_DEPTH, FNA3D_VERTEXELEMENTUSAGE_DEPTH, 0,
		     "Depth stream, before Position");

	FNA3D_DestroyDevice(device);
	SDL_DestroyWindow(window);
	SDL_Quit();

	printf("%s: %d check(s), %d failed\n", label, checks, failures);
	return (failures == 0) ? 0 : 1;
}
