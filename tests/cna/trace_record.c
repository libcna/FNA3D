/* libcna fork regression test: record a trace for replay and dumpspirv (TRACING_SUPPORT only).
 *
 * Run in an empty directory, it records FNA3D_Trace.bin there -- three frames that the other trace
 * tests consume (trace_tests.cmake):
 *
 *   1. XNA pixel centres on and a bump environment for stage 3 (the CNA extension entry points,
 *      marks 200 and 201), then a draw through pass 0 of a two-pass effect;
 *   2. pass 1 of the same effect -- a later frame changing the pass of an applied effect, which
 *      MojoShader answers by writing through the state-change record the effect was applied
 *      with (replay kept that in a frame-local variable) -- and pixel centres off;
 *   3. a draw whose vertex declaration has a Depth element in Byte4 format, which dumpspirv must
 *      link to the shader's DEPTH input.
 *
 * Each extension call follows a string marker; the record right after each marker in the trace
 * must be that call's, with its arguments (2026-10-26 dependency audit B2 B-09).
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

#define SIZE 64

#define MARK_SETXNAPIXELCENTEREXT	200
#define MARK_SETLEGACYBUMPMAPENVEXT	201

static const char markerOn[] = "cna-r4-04: pixel centres on";
static const char markerBump[] = "cna-r4-04: bump environment of stage 3";
static const char markerOff[] = "cna-r4-04: pixel centres off";
static const float bumpEnv[6] = { 0.5f, -0.25f, 0.125f, 2.0f, 1.5f, -0.75f };

static int failures = 0;

/* MojoShader keeps the address an effect is applied with, so it lives as long as the device. */
static MOJOSHADER_effectStateChanges stateChanges;

static FNA3D_Effect *CreateEffect(FNA3D_Device *device, MOJOSHADER_usage usage, uint32_t passes)
{
	TestEffect code;
	FNA3D_Effect *effect = NULL;
	MOJOSHADER_effect *effectData = NULL;
	TestEffect_Build(&code, usage, passes);
	FNA3D_CreateEffect(device, (uint8_t*) code.words, code.length, &effect, &effectData);
	if (effect == NULL || effectData == NULL || effectData->error_count > 0)
	{
		fprintf(stderr, "the test effect did not compile: %s\n",
			(effectData != NULL && effectData->error_count > 0) ?
				effectData->errors[0].error :
				"no effect");
		return NULL;
	}
	return effect;
}

/* Finds a string marker's record (its text and terminator) and returns the offset just past it. */
static long AfterMarker(const uint8_t *trace, long length, const char *marker)
{
	const long markerLength = (long) strlen(marker) + 1;
	long i;
	for (i = 0; i + markerLength <= length; i += 1)
	{
		if (memcmp(trace + i, marker, (size_t) markerLength) == 0)
		{
			return i + markerLength;
		}
	}
	return -1;
}

static void CheckRecord(const uint8_t *trace, long length, const char *marker,
			const uint8_t *expected, size_t expectedLength, const char *what)
{
	const long at = AfterMarker(trace, length, marker);
	if (at < 0)
	{
		fprintf(stderr, "FAILED: the trace has no marker \"%s\"\n", marker);
		failures += 1;
	}
	else if (	at + (long) expectedLength > length ||
			memcmp(trace + at, expected, expectedLength) != 0	)
	{
		fprintf(stderr, "FAILED: the record after \"%s\" is not %s (mark %d found)\n",
			marker, what, (at < length) ? trace[at] : -1);
		failures += 1;
	}
	else
	{
		printf("trace: %s follows \"%s\"\n", what, marker);
	}
}

static void CheckTrace(void)
{
	uint8_t expected[1 + 4 + 6 * 4];
	const int32_t sampler = 3;
	size_t length = 0;
	uint8_t *trace = (uint8_t*) SDL_LoadFile("FNA3D_Trace.bin", &length);
	if (trace == NULL)
	{
		fprintf(stderr, "FAILED: no FNA3D_Trace.bin was written: %s\n", SDL_GetError());
		failures += 1;
		return;
	}

	expected[0] = MARK_SETXNAPIXELCENTEREXT;
	expected[1] = 1;
	CheckRecord(trace, (long) length, markerOn, expected, 2, "SetXnaPixelCenterEXT(1)");
	expected[1] = 0;
	CheckRecord(trace, (long) length, markerOff, expected, 2, "SetXnaPixelCenterEXT(0)");
	expected[0] = MARK_SETLEGACYBUMPMAPENVEXT;
	memcpy(expected + 1, &sampler, 4);
	memcpy(expected + 5, bumpEnv, sizeof(bumpEnv));
	CheckRecord(trace, (long) length, markerBump, expected, sizeof(expected),
		    "SetLegacyBumpMapEnvEXT(3, 0.5, -0.25, 0.125, 2, 1.5, -0.75)");
	SDL_free(trace);
}

int main(int argc, char **argv)
{
	static const float color[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
	FNA3D_Vec4 clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	FNA3D_PresentationParameters parameters;
	FNA3D_Device *device;
	FNA3D_Effect *colorEffect, *depthEffect;
	FNA3D_Buffer *vertexBuffer;
	FNA3D_VertexElement colorElements[2], depthElements[2];
	FNA3D_VertexBufferBinding colorBinding, depthBinding;
	TestEffect_Vertex vertices[3];
	SDL_Window *window;

	(void) argc;
	(void) argv;
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	window = SDL_CreateWindow("fna3d_cna_trace_record", SIZE, SIZE,
				  FNA3D_PrepareWindowAttributes());
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
	colorEffect = CreateEffect(device, MOJOSHADER_USAGE_COLOR, 2);
	depthEffect = CreateEffect(device, MOJOSHADER_USAGE_DEPTH, 1);
	if (colorEffect == NULL || depthEffect == NULL)
	{
		return 1;
	}

	TestEffect_FullscreenTriangle(vertices, color);
	vertexBuffer = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_WRITEONLY,
					     (int32_t) sizeof(vertices));
	FNA3D_SetVertexBufferData(device, vertexBuffer, 0, vertices, 3,
				  (int32_t) sizeof(TestEffect_Vertex),
				  (int32_t) sizeof(TestEffect_Vertex),
				  FNA3D_SETDATAOPTIONS_NONE);

	colorElements[0].offset = 0;
	colorElements[0].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR4;
	colorElements[0].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_POSITION;
	colorElements[0].usageIndex = 0;
	colorElements[1] = colorElements[0];
	colorElements[1].offset = 16;
	colorElements[1].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_COLOR;
	depthElements[0] = colorElements[0];
	depthElements[1] = colorElements[1];
	depthElements[1].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_BYTE4;
	depthElements[1].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_DEPTH;
	SDL_memset(&colorBinding, '\0', sizeof(colorBinding));
	colorBinding.vertexBuffer = vertexBuffer;
	colorBinding.vertexDeclaration.vertexStride = (int32_t) sizeof(TestEffect_Vertex);
	colorBinding.vertexDeclaration.elementCount = 2;
	colorBinding.vertexDeclaration.elements = colorElements;
	depthBinding = colorBinding;
	depthBinding.vertexDeclaration.elements = depthElements;

	/* Frame 1 */
	TestEffect_ApplyDrawStates(device, SIZE, SIZE, 0);
	FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &clearColor, 1.0f, 0);
	FNA3D_SetStringMarker(device, markerOn);
	FNA3D_SetXnaPixelCenterEXT(device, 1);
	FNA3D_SetStringMarker(device, markerBump);
	FNA3D_SetLegacyBumpMapEnvEXT(device, 3, bumpEnv[0], bumpEnv[1], bumpEnv[2], bumpEnv[3],
				     bumpEnv[4], bumpEnv[5]);
	FNA3D_ApplyEffect(device, colorEffect, 0, &stateChanges);
	FNA3D_ApplyVertexBufferBindings(device, &colorBinding, 1, 1, 0);
	FNA3D_DrawPrimitives(device, FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 1);
	FNA3D_SwapBuffers(device, NULL, NULL, window);

	/* Frame 2: the same effect, another pass */
	FNA3D_ApplyEffect(device, colorEffect, 1, &stateChanges);
	FNA3D_ApplyVertexBufferBindings(device, &colorBinding, 1, 1, 0);
	FNA3D_DrawPrimitives(device, FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 1);
	FNA3D_SetStringMarker(device, markerOff);
	FNA3D_SetXnaPixelCenterEXT(device, 0);
	FNA3D_SwapBuffers(device, NULL, NULL, window);

	/* Frame 3: a Depth element */
	FNA3D_ApplyEffect(device, depthEffect, 0, &stateChanges);
	FNA3D_ApplyVertexBufferBindings(device, &depthBinding, 1, 1, 0);
	FNA3D_DrawPrimitives(device, FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 1);
	FNA3D_SwapBuffers(device, NULL, NULL, window);

	FNA3D_AddDisposeVertexBuffer(device, vertexBuffer);
	FNA3D_AddDisposeEffect(device, colorEffect);
	FNA3D_AddDisposeEffect(device, depthEffect);
	FNA3D_DestroyDevice(device);
	SDL_DestroyWindow(window);

	CheckTrace();
	SDL_Quit();
	printf("trace record: %d failure(s)\n", failures);
	return (failures == 0) ? 0 : 1;
}
