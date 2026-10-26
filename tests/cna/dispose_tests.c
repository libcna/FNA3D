/* FNA3D - 3D Graphics Library for FNA
 *
 * Copyright (c) 2020-2024 Ethan Lee
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

/* Resources disposed off the device thread are queued for the next SwapBuffers. Destroying the
 * device before that presentation must still free them, and the device its own caches: every SDL
 * allocation the device made is gone again once FNA3D_DestroyDevice returns.
 */

#include <stdio.h>
#include <SDL3/SDL.h>

#include "FNA3D.h"
#include "mojoshader.h"
#include "test_effect.h"

/* Live SDL allocations, counted through SDL_SetMemoryFunctions (SDL_GetNumAllocations is -1
 * unless SDL was built to track them).
 */
static SDL_malloc_func realMalloc;
static SDL_calloc_func realCalloc;
static SDL_realloc_func realRealloc;
static SDL_free_func realFree;
static SDL_AtomicInt liveAllocations;

static void *CountingMalloc(size_t size)
{
	void *result = realMalloc(size);
	if (result != NULL)
	{
		SDL_AddAtomicInt(&liveAllocations, 1);
	}
	return result;
}

static void *CountingCalloc(size_t count, size_t size)
{
	void *result = realCalloc(count, size);
	if (result != NULL)
	{
		SDL_AddAtomicInt(&liveAllocations, 1);
	}
	return result;
}

static void *CountingRealloc(void *memory, size_t size)
{
	void *result = realRealloc(memory, size);
	if ((memory == NULL) && (result != NULL))
	{
		SDL_AddAtomicInt(&liveAllocations, 1);
	}
	return result;
}

static void CountingFree(void *memory)
{
	if (memory != NULL)
	{
		SDL_AddAtomicInt(&liveAllocations, -1);
	}
	realFree(memory);
}

typedef struct DisposeJob
{
	FNA3D_Device *device;
	FNA3D_Texture *texture;
	FNA3D_Buffer *vertexBuffer;
	FNA3D_Buffer *indexBuffer;
	FNA3D_Renderbuffer *renderbuffer;
	FNA3D_Query *query;
} DisposeJob;

static int DisposeOffThread(void *data)
{
	DisposeJob *job = (DisposeJob*) data;
	FNA3D_AddDisposeTexture(job->device, job->texture);
	FNA3D_AddDisposeVertexBuffer(job->device, job->vertexBuffer);
	FNA3D_AddDisposeIndexBuffer(job->device, job->indexBuffer);
	FNA3D_AddDisposeRenderbuffer(job->device, job->renderbuffer);
	if (job->query != NULL)
	{
		FNA3D_AddDisposeQuery(job->device, job->query);
	}
	return 0;
}

static int Cycle(SDL_Window *window, int offThread)
{
	FNA3D_PresentationParameters parameters;
	FNA3D_Device *device;
	DisposeJob job;
	FNA3D_SamplerState samplerState;
	SDL_Thread *thread;
	TestEffect code;
	FNA3D_Effect *effect = NULL;
	MOJOSHADER_effect *effectData = NULL;
	MOJOSHADER_effectStateChanges stateChanges;
	FNA3D_VertexElement elements[2];
	FNA3D_VertexBufferBinding binding;

	SDL_memset(&parameters, '\0', sizeof(parameters));
	parameters.backBufferWidth = 64;
	parameters.backBufferHeight = 64;
	parameters.backBufferFormat = FNA3D_SURFACEFORMAT_COLOR;
	parameters.deviceWindowHandle = window;
	parameters.depthStencilFormat = FNA3D_DEPTHFORMAT_NONE;
	parameters.presentationInterval = FNA3D_PRESENTINTERVAL_IMMEDIATE;
	device = FNA3D_CreateDevice(&parameters, 0);
	if (device == NULL)
	{
		fprintf(stderr, "FNA3D_CreateDevice failed\n");
		return 0;
	}

	job.device = device;
	job.texture = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, 4, 4, 1, 0);
	job.vertexBuffer = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_NONE, 64);
	job.indexBuffer = FNA3D_GenIndexBuffer(device, 0, FNA3D_BUFFERUSAGE_NONE, 64);
	job.renderbuffer = FNA3D_GenDepthStencilRenderbuffer(device, 4, 4,
							     FNA3D_DEPTHFORMAT_D24S8, 0);
	/* Desktop GL always has occlusion queries; OpenGL ES 3 is not asked for them here. */
	job.query = SDL_GetHintBoolean("FNA3D_OPENGL_FORCE_ES3", false) ?
		NULL :
		FNA3D_CreateQuery(device);
	/* A sampler in the driver's sampler cache, which the SDL_GPU driver keeps in an array. */
	SDL_memset(&samplerState, '\0', sizeof(samplerState));
	samplerState.filter = FNA3D_TEXTUREFILTER_LINEAR;
	samplerState.addressU = FNA3D_TEXTUREADDRESSMODE_CLAMP;
	samplerState.addressV = FNA3D_TEXTUREADDRESSMODE_CLAMP;
	samplerState.addressW = FNA3D_TEXTUREADDRESSMODE_CLAMP;
	samplerState.maxAnisotropy = 4;
	FNA3D_VerifySampler(device, 0, job.texture, &samplerState);
	/* Vertex buffer bindings applied through an effect, which the SDL_GPU driver remembers in
	 * its vertex-buffer-bindings cache. The effect itself goes on the device thread.
	 */
	TestEffect_Build(&code, MOJOSHADER_USAGE_COLOR, 1);
	FNA3D_CreateEffect(device, (uint8_t*) code.words, code.length, &effect, &effectData);
	if (effect == NULL || effectData == NULL || effectData->error_count > 0)
	{
		fprintf(stderr, "the test effect did not compile\n");
		return 0;
	}
	elements[0].offset = 0;
	elements[0].vertexElementFormat = FNA3D_VERTEXELEMENTFORMAT_VECTOR4;
	elements[0].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_POSITION;
	elements[0].usageIndex = 0;
	elements[1] = elements[0];
	elements[1].offset = 16;
	elements[1].vertexElementUsage = FNA3D_VERTEXELEMENTUSAGE_COLOR;
	SDL_memset(&binding, '\0', sizeof(binding));
	binding.vertexBuffer = job.vertexBuffer;
	binding.vertexDeclaration.vertexStride = 32;
	binding.vertexDeclaration.elementCount = 2;
	binding.vertexDeclaration.elements = elements;
	SDL_memset(&stateChanges, '\0', sizeof(stateChanges));
	FNA3D_ApplyEffect(device, effect, 0, &stateChanges);
	FNA3D_ApplyVertexBufferBindings(device, &binding, 1, 1, 0);
	FNA3D_AddDisposeEffect(device, effect);
	if (offThread)
	{
		thread = SDL_CreateThread(DisposeOffThread, "dispose", &job);
		SDL_WaitThread(thread, NULL);
	}
	else
	{
		DisposeOffThread(&job);
	}
	FNA3D_DestroyDevice(device);
	return 1;
}

int main(int argc, char **argv)
{
	SDL_Window *window;
	int before, after, failed;

	(void) argc;
	(void) argv;
	SDL_GetOriginalMemoryFunctions(&realMalloc, &realCalloc, &realRealloc, &realFree);
	SDL_SetMemoryFunctions(CountingMalloc, CountingCalloc, CountingRealloc, CountingFree);
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	window = SDL_CreateWindow("fna3d_cna_dispose_tests", 64, 64,
				  FNA3D_PrepareWindowAttributes() | SDL_WINDOW_HIDDEN);
	if (window == NULL)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}

	/* A first device lets SDL make whatever one-time allocations a GL context costs. */
	if (!Cycle(window, 0))
	{
		return 1;
	}
	before = SDL_GetAtomicInt(&liveAllocations);
	if (!Cycle(window, 0))
	{
		return 1;
	}
	after = SDL_GetAtomicInt(&liveAllocations);
	failed = (after != before);
	printf("on the device thread: %d SDL allocation(s) outstanding before, %d after%s\n",
	       before, after, failed ? " -- FAILED" : "");

	before = SDL_GetAtomicInt(&liveAllocations);
	if (!Cycle(window, 1))
	{
		return 1;
	}
	after = SDL_GetAtomicInt(&liveAllocations);
	printf("off the device thread: %d SDL allocation(s) outstanding before, %d after%s\n",
	       before, after, (after != before) ? " -- FAILED" : "");
	failed |= (after != before);

	SDL_DestroyWindow(window);
	SDL_Quit();
	return failed ? 1 : 0;
}
