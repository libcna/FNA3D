/* libcna regression: traced device overlap and foreign-window presents refuse
 * without changing the legacy one-device byte stream. SDL allocation ownership
 * still balances, including failed driver creation and refused second creation.
 * With tracing disabled, two real devices and foreign-window presents remain usable.
 * See tests/cna/README.md.
 */

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FNA3D.h"

#define TRACE_FILE "FNA3D_Trace.bin"

/* FNA3D_Tracing.c's marks */
#define MARK_CREATEDEVICE	0
#define MARK_DESTROYDEVICE	1
#define MARK_SETSTRINGMARKER	56

static const char markerFirst[] = "cna-r7-04: through the first device";
static const char markerAfter[] = "cna-l41: first device survives refused overlap";

static int checks = 0;
static int failures = 0;

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

/* FNA3D's log goes to stderr here: SDL's own logging allocates on first use. */
static void FNA3DCALL LogToStderr(const char *message)
{
	fprintf(stderr, "%s\n", message);
}

typedef struct Trace
{
	uint8_t bytes[1024];
	size_t length;
} Trace;

static void Append(Trace *trace, const void *data, size_t length)
{
	SDL_assert(trace->length + length <= sizeof(trace->bytes));
	memcpy(trace->bytes + trace->length, data, length);
	trace->length += length;
}

/* The records FNA3D_Tracing.c writes: each field as it lies in memory, in this order. */
static void AppendCreateDevice(Trace *trace, const FNA3D_PresentationParameters *parameters,
			       uint8_t debugMode)
{
	const uint8_t mark = MARK_CREATEDEVICE;
	Append(trace, &mark, sizeof(mark));
	Append(trace, &parameters->backBufferWidth, sizeof(parameters->backBufferWidth));
	Append(trace, &parameters->backBufferHeight, sizeof(parameters->backBufferHeight));
	Append(trace, &parameters->backBufferFormat, sizeof(parameters->backBufferFormat));
	Append(trace, &parameters->multiSampleCount, sizeof(parameters->multiSampleCount));
	Append(trace, &parameters->isFullScreen, sizeof(parameters->isFullScreen));
	Append(trace, &parameters->depthStencilFormat, sizeof(parameters->depthStencilFormat));
	Append(trace, &parameters->presentationInterval, sizeof(parameters->presentationInterval));
	Append(trace, &parameters->displayOrientation, sizeof(parameters->displayOrientation));
	Append(trace, &parameters->renderTargetUsage, sizeof(parameters->renderTargetUsage));
	Append(trace, &debugMode, sizeof(debugMode));
}

static void AppendMarker(Trace *trace, const char *text)
{
	const uint8_t mark = MARK_SETSTRINGMARKER;
	const int32_t length = (int32_t) strlen(text) + 1;
	Append(trace, &mark, sizeof(mark));
	Append(trace, &length, sizeof(length));
	Append(trace, text, (size_t) length);
}

static void Parameters(FNA3D_PresentationParameters *parameters, SDL_Window *window, int32_t size)
{
	SDL_memset(parameters, '\0', sizeof(*parameters));
	parameters->backBufferWidth = size;
	parameters->backBufferHeight = size;
	parameters->backBufferFormat = FNA3D_SURFACEFORMAT_COLOR;
	parameters->deviceWindowHandle = window;
	parameters->depthStencilFormat = FNA3D_DEPTHFORMAT_NONE;
	parameters->presentationInterval = FNA3D_PRESENTINTERVAL_IMMEDIATE;
}

/* Two overlapping devices; returns 0 if either could not be made. The trace this must write is
 * appended to expected.
 */
static int Overlap(SDL_Window *windowA, SDL_Window *windowB, Trace *expected)
{
	FNA3D_PresentationParameters parametersA, parametersB;
	FNA3D_Device *deviceA, *deviceB;

	Parameters(&parametersA, windowA, 64);
	Parameters(&parametersB, windowB, 32);
	AppendCreateDevice(expected, &parametersA, 0);
	deviceA = FNA3D_CreateDevice(&parametersA, 0);
	if (deviceA == NULL)
	{
		fprintf(stderr, "FNA3D_CreateDevice failed for the first device\n");
		return 0;
	}
	FNA3D_SetStringMarker(deviceA, markerFirst);
	AppendMarker(expected, markerFirst);

	deviceB = FNA3D_CreateDevice(&parametersB, 0);
	CHECK(deviceB == NULL, "an overlapping traced device is refused before creation");
	if (deviceB != NULL)
	{
		FNA3D_DestroyDevice(deviceB);
	}
	FNA3D_DestroyDevice(NULL);
	/* Neither a refused overlap nor a foreign-window present may end/corrupt A's trace. */
	FNA3D_SwapBuffers(deviceA, NULL, NULL, windowB);
	FNA3D_SetStringMarker(deviceA, markerAfter);
	AppendMarker(expected, markerAfter);
	FNA3D_DestroyDevice(deviceA);
	{
		const uint8_t mark = MARK_DESTROYDEVICE;
		Append(expected, &mark, sizeof(mark));
	}
	return 1;
}

/* Reads the trace file whole; NULL (length 0) if there is none. */
static uint8_t *ReadTrace(size_t *length)
{
	uint8_t *data;
	FILE *file = fopen(TRACE_FILE, "rb");
	long size;
	*length = 0;
	if (file == NULL)
	{
		return NULL;
	}
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = (uint8_t*) malloc((size_t) size + 1);
	*length = fread(data, 1, (size_t) size, file);
	fclose(file);
	return data;
}

int main(int argc, char **argv)
{
	FNA3D_PresentationParameters parameters;
	SDL_Window *windowA, *windowB;
	Trace expected;
	uint8_t *trace;
	size_t traceLength;
	int before, after;
	uint32_t flags;

	(void) argc;
	(void) argv;
	remove(TRACE_FILE);
	SDL_GetOriginalMemoryFunctions(&realMalloc, &realCalloc, &realRealloc, &realFree);
	SDL_SetMemoryFunctions(CountingMalloc, CountingCalloc, CountingRealloc, CountingFree);
	FNA3D_HookLogFunctions(LogToStderr, LogToStderr, LogToStderr);
	/* The hint and log lookups a trace session makes allocate the first time; make them now. */
	(void) SDL_GetHintBoolean("FNA3D_DISABLE_TRACING", false);
	SDL_Log("trace_overlap_tests: tracing a failed CreateDevice, then two devices");

	/* 1. No driver is selected yet, so this CreateDevice makes no device. */
	Parameters(&parameters, NULL, 64);
	before = SDL_GetAtomicInt(&liveAllocations);
	CHECK(FNA3D_CreateDevice(&parameters, 0) == NULL,
		"CreateDevice made a device before FNA3D_PrepareWindowAttributes");
	after = SDL_GetAtomicInt(&liveAllocations);
	CHECK(after == before,
		"a CreateDevice that made no device left %d SDL allocation(s) behind", after - before);
	remove(TRACE_FILE);

	/* 2. Two overlapping devices, once to warm up and once measured. */
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	flags = FNA3D_PrepareWindowAttributes() | SDL_WINDOW_HIDDEN;
	windowA = SDL_CreateWindow("fna3d_cna_trace_overlap_tests A", 64, 64, flags);
	windowB = SDL_CreateWindow("fna3d_cna_trace_overlap_tests B", 32, 32, flags);
	if (windowA == NULL || windowB == NULL)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}
	expected.length = 0;
	if (!Overlap(windowA, windowB, &expected))
	{
		return 1;
	}
	remove(TRACE_FILE);
	expected.length = 0;
	before = SDL_GetAtomicInt(&liveAllocations);
	if (!Overlap(windowA, windowB, &expected))
	{
		return 1;
	}
	after = SDL_GetAtomicInt(&liveAllocations);
	CHECK(after == before,
		"two overlapping devices left %d SDL allocation(s) behind", after - before);

	trace = ReadTrace(&traceLength);
	CHECK(trace != NULL, "no %s was written", TRACE_FILE);
	if (trace != NULL)
	{
		CHECK(	traceLength == expected.length &&
			memcmp(trace, expected.bytes, expected.length) == 0,
			"the trace (%zu bytes) is not the first device's CreateDevice, the two "
			"markers and one DestroyDevice (%zu bytes)", traceLength, expected.length);
		free(trace);
	}

	SDL_SetHint("FNA3D_DISABLE_TRACING", "1");
	{
		FNA3D_PresentationParameters a, b;
		FNA3D_Device *da, *db;
		Parameters(&a, windowA, 64);
		Parameters(&b, windowB, 32);
		da = FNA3D_CreateDevice(&a, 0);
		db = FNA3D_CreateDevice(&b, 0);
		CHECK(da && db, "two devices remain supported with tracing disabled");
		if (da && db)
		{
			FNA3D_SwapBuffers(da, NULL, NULL, windowB);
			FNA3D_SwapBuffers(db, NULL, NULL, windowA);
		}
		FNA3D_DestroyDevice(da);
		FNA3D_DestroyDevice(db);
	}
	SDL_DestroyWindow(windowB);
	SDL_DestroyWindow(windowA);
	SDL_Quit();
	printf("trace sessions: %d check(s), %d failed\n", checks, failures);
	return (failures == 0) ? 0 : 1;
}
