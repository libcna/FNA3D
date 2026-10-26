/* libcna fork regression test: texture readback writes exactly what it should.
 *
 * GetTextureData2D/GetTextureDataCube used to read back as RGBA8 whatever the format, leave
 * GL_PACK_ALIGNMENT at its default of 4 and size mip levels without the clamp to one texel, all
 * three of which wrote past the destination or past a temporary buffer (2026-10-26 dependency
 * audit B2-01, B2-02, B2-03). Every read here goes into a buffer with guard bytes after the exact
 * length the format and rectangle need, and every refusal is checked to have written nothing.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FNA3D.h"

#define GUARD 0xA5
#define GUARD_BYTES 64
#define GL_PACK_ALIGNMENT_ENUM 0x0D05

static int failures = 0;
static int checks = 0;
static int es3 = 0;

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

typedef struct Format
{
	FNA3D_SurfaceFormat format;
	const char *name;
	int32_t texelBytes;
	/* The texel bytes come back as uploaded (not for the signed formats, which FNA3D stores in
	 * unsigned normalized textures, nor on ES 3, whose only supported reads are RGBA8 and the
	 * float formats). */
	uint8_t exactRoundTrip;
	/* A float format, read back on ES 3 too. */
	uint8_t isFloat;
	/* Halves rather than floats. */
	uint8_t isHalf;
} Format;

static const Format formats[] =
{
	{ FNA3D_SURFACEFORMAT_COLOR,		"Color",		4, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_BGR565,		"Bgr565",		2, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_BGRA5551,		"Bgra5551",		2, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_BGRA4444,		"Bgra4444",		2, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_NORMALIZEDBYTE2,	"NormalizedByte2",	2, 0, 0, 0 },
	{ FNA3D_SURFACEFORMAT_NORMALIZEDBYTE4,	"NormalizedByte4",	4, 0, 0, 0 },
	{ FNA3D_SURFACEFORMAT_RGBA1010102,	"Rgba1010102",		4, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_RG32,		"Rg32",			4, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_RGBA64,		"Rgba64",		8, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_ALPHA8,		"Alpha8",		1, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_SINGLE,		"Single",		4, 1, 1, 0 },
	{ FNA3D_SURFACEFORMAT_VECTOR2,		"Vector2",		8, 1, 1, 0 },
	{ FNA3D_SURFACEFORMAT_VECTOR4,		"Vector4",		16, 1, 1, 0 },
	{ FNA3D_SURFACEFORMAT_HALFSINGLE,	"HalfSingle",		2, 1, 1, 1 },
	{ FNA3D_SURFACEFORMAT_HALFVECTOR2,	"HalfVector2",		4, 1, 1, 1 },
	{ FNA3D_SURFACEFORMAT_HALFVECTOR4,	"HalfVector4",		8, 1, 1, 1 },
	{ FNA3D_SURFACEFORMAT_HDRBLENDABLE,	"HdrBlendable",		8, 1, 1, 1 },
};

/* Distinct, finite, exactly representable values: halves 1.0 + n/1024, floats 1.0 + n/2^20, and
 * bytes that keep every packed channel meaningful. */
static void FillPattern(const Format *format, uint8_t *data, size_t bytes, uint32_t seed)
{
	size_t i;
	if (format->isFloat && format->isHalf)
	{
		for (i = 0; i + 1 < bytes; i += 2)
		{
			const uint16_t half = (uint16_t) (0x3C00 | ((seed + i / 2) & 0x3FF));
			memcpy(data + i, &half, 2);
		}
	}
	else if (format->isFloat)
	{
		for (i = 0; i + 3 < bytes; i += 4)
		{
			const uint32_t bits = 0x3F800000u | ((seed + (uint32_t) (i / 4)) & 0xFFFFF);
			memcpy(data + i, &bits, 4);
		}
	}
	else
	{
		for (i = 0; i < bytes; i += 1)
		{
			data[i] = (uint8_t) ((seed * 31u + (uint32_t) i * 7u + 3u) & 0x7F);
		}
	}
}

static uint8_t *GuardedBuffer(size_t bytes)
{
	uint8_t *buffer = (uint8_t*) malloc(bytes + GUARD_BYTES);
	memset(buffer, GUARD, bytes + GUARD_BYTES);
	return buffer;
}

static int GuardIntact(const uint8_t *buffer, size_t bytes, size_t extent)
{
	size_t i;
	for (i = bytes; i < bytes + extent; i += 1)
	{
		if (buffer[i] != GUARD)
		{
			return 0;
		}
	}
	return 1;
}

static int32_t MipSize(int32_t size, int32_t level)
{
	size >>= level;
	return (size < 1) ? 1 : size;
}

static int32_t PackAlignment(void)
{
	typedef void (*GetIntegerv)(uint32_t, int32_t*);
	GetIntegerv getIntegerv = (GetIntegerv) SDL_GL_GetProcAddress("glGetIntegerv");
	int32_t alignment = -1;
	if (getIntegerv != NULL)
	{
		getIntegerv(GL_PACK_ALIGNMENT_ENUM, &alignment);
	}
	return alignment;
}

/* Whether this context can read the format back at all (ES 3 reads only RGBA8 and floats). */
static int Readable(const Format *format)
{
	return !es3 || format->isFloat || (format->format == FNA3D_SURFACEFORMAT_COLOR);
}

/* Reads (x, y, w, h) of one level and checks the guard, and the bytes against the uploaded level
 * where the format keeps them exactly. */
static void CheckRead2D(
	FNA3D_Device *device,
	FNA3D_Texture *texture,
	const Format *format,
	int32_t level,
	int32_t levelWidth,
	const uint8_t *uploaded,
	int32_t x,
	int32_t y,
	int32_t w,
	int32_t h,
	const char *what
) {
	const size_t bytes = (size_t) w * (size_t) h * (size_t) format->texelBytes;
	uint8_t *buffer = GuardedBuffer(bytes);
	int32_t row;

	FNA3D_GetTextureData2D(device, texture, x, y, w, h, level, buffer, (int32_t) bytes);
	CHECK(GuardIntact(buffer, bytes, GUARD_BYTES),
		"%s %s level %d (%d,%d %dx%d): wrote past %zu bytes",
		what, format->name, level, x, y, w, h, bytes);
	if (format->exactRoundTrip && Readable(format))
	{
		for (row = 0; row < h; row += 1)
		{
			const uint8_t *expected = uploaded +
				(((size_t) (y + row) * (size_t) levelWidth) + (size_t) x) *
				(size_t) format->texelBytes;
			const uint8_t *actual = buffer + (size_t) row * (size_t) w *
				(size_t) format->texelBytes;
			CHECK(memcmp(expected, actual, (size_t) w * (size_t) format->texelBytes) == 0,
				"%s %s level %d (%d,%d %dx%d): row %d differs from the upload",
				what, format->name, level, x, y, w, h, row);
		}
	}
	free(buffer);
}

static void TestTexture2D(FNA3D_Device *device, const Format *format, int32_t width,
			  int32_t height, int32_t levels)
{
	FNA3D_Texture *texture = FNA3D_CreateTexture2D(device, format->format, width, height,
						       levels, 0);
	uint8_t *uploads[16];
	int32_t level;
	char what[64];

	SDL_snprintf(what, sizeof(what), "2D %dx%d", width, height);
	for (level = 0; level < levels; level += 1)
	{
		const int32_t w = MipSize(width, level);
		const int32_t h = MipSize(height, level);
		const size_t bytes = (size_t) w * (size_t) h * (size_t) format->texelBytes;
		uploads[level] = (uint8_t*) malloc(bytes);
		FillPattern(format, uploads[level], bytes, (uint32_t) (level * 101 + width));
		FNA3D_SetTextureData2D(device, texture, 0, 0, w, h, level, uploads[level],
				       (int32_t) bytes);
	}
	for (level = 0; level < levels; level += 1)
	{
		const int32_t w = MipSize(width, level);
		const int32_t h = MipSize(height, level);
		CheckRead2D(device, texture, format, level, w, uploads[level], 0, 0, w, h, what);
		if (w > 1 && h > 1)
		{
			CheckRead2D(device, texture, format, level, w, uploads[level],
				    1, 1, w - 1, h - 1, what);
		}
		CheckRead2D(device, texture, format, level, w, uploads[level],
			    w - 1, h - 1, 1, 1, what);
	}
	if (SDL_GL_GetCurrentContext() != NULL) /* only the OpenGL driver has pixel-store state */
	{
		CHECK(PackAlignment() == 4, "%s %s: GL_PACK_ALIGNMENT left at %d",
			what, format->name, PackAlignment());
	}
	for (level = 0; level < levels; level += 1)
	{
		free(uploads[level]);
	}
	FNA3D_AddDisposeTexture(device, texture);
}

static void TestCube(FNA3D_Device *device, const Format *format, int32_t size, int32_t levels)
{
	FNA3D_Texture *texture = FNA3D_CreateTextureCube(device, format->format, size, levels, 0);
	int32_t face, level;
	for (face = 0; face < 6; face += 1)
	{
		for (level = 0; level < levels; level += 1)
		{
			const int32_t s = MipSize(size, level);
			const size_t bytes = (size_t) s * (size_t) s * (size_t) format->texelBytes;
			uint8_t *uploaded = (uint8_t*) malloc(bytes);
			uint8_t *buffer = GuardedBuffer(bytes);
			FillPattern(format, uploaded, bytes, (uint32_t) (face * 13 + level));
			FNA3D_SetTextureDataCube(device, texture, 0, 0, s, s,
						 (FNA3D_CubeMapFace) face, level, uploaded,
						 (int32_t) bytes);
			FNA3D_GetTextureDataCube(device, texture, 0, 0, s, s,
						 (FNA3D_CubeMapFace) face, level, buffer,
						 (int32_t) bytes);
			CHECK(GuardIntact(buffer, bytes, GUARD_BYTES),
				"cube %d %s face %d level %d: wrote past %zu bytes",
				size, format->name, face, level, bytes);
			if (format->exactRoundTrip && Readable(format))
			{
				CHECK(memcmp(uploaded, buffer, bytes) == 0,
					"cube %d %s face %d level %d: differs from the upload",
					size, format->name, face, level);
			}
			free(buffer);
			free(uploaded);
		}
	}
	FNA3D_AddDisposeTexture(device, texture);
}

/* Volume uploads read exactly their tightly packed bytes (each one in its own, exactly sized
 * allocation, so AddressSanitizer sees a read past it). No driver has a 3D readback:
 * GetTextureData3D must refuse and leave the destination alone.
 */
static void TestVolume(FNA3D_Device *device, const Format *format, int32_t size, int32_t depth,
		       int32_t levels)
{
	FNA3D_Texture *texture = FNA3D_CreateTexture3D(device, format->format, size, size, depth,
						       levels);
	int32_t level;
	for (level = 0; level < levels; level += 1)
	{
		const int32_t s = MipSize(size, level);
		const int32_t d = MipSize(depth, level);
		const size_t bytes = (size_t) s * (size_t) s * (size_t) d * (size_t) format->texelBytes;
		uint8_t *uploaded = (uint8_t*) malloc(bytes);
		FillPattern(format, uploaded, bytes, (uint32_t) (level + 7));
		FNA3D_SetTextureData3D(device, texture, 0, 0, 0, s, s, d, level, uploaded,
				       (int32_t) bytes);
		if (!es3)
		{
			uint8_t *buffer = GuardedBuffer(bytes);
			uint8_t *untouched = GuardedBuffer(bytes);
			FNA3D_GetTextureData3D(device, texture, 0, 0, 0, s, s, d, level, buffer,
					       (int32_t) bytes);
			CHECK(memcmp(buffer, untouched, bytes + GUARD_BYTES) == 0,
				"volume %dx%dx%d %s level %d: refused readback wrote anyway",
				size, size, depth, format->name, level);
			free(untouched);
			free(buffer);
		}
		free(uploaded);
	}
	FNA3D_AddDisposeTexture(device, texture);
}

/* A render target still attached to the framebuffer: the path that read RGBA8 regardless. */
static void TestAttachedTarget(FNA3D_Device *device, const Format *format)
{
	const int32_t width = 5, height = 3;
	const size_t bytes = (size_t) width * height * (size_t) format->texelBytes;
	FNA3D_Texture *texture = FNA3D_CreateTexture2D(device, format->format, width, height, 1, 1);
	FNA3D_RenderTargetBinding binding;
	FNA3D_Vec4 color = { 0.25f, 0.5f, 0.75f, 1.0f };
	uint8_t *buffer = GuardedBuffer(bytes);
	size_t texel;

	SDL_memset(&binding, '\0', sizeof(binding));
	binding.type = FNA3D_RENDERTARGET_TYPE_2D;
	binding.twod.width = width;
	binding.twod.height = height;
	binding.levelCount = 1;
	binding.texture = texture;
	FNA3D_SetRenderTargets(device, &binding, 1, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
	FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &color, 0.0f, 0);
	FNA3D_GetTextureData2D(device, texture, 0, 0, width, height, 0, buffer, (int32_t) bytes);
	CHECK(GuardIntact(buffer, bytes, GUARD_BYTES),
		"attached target %s: wrote past %zu bytes", format->name, bytes);
	if (Readable(format))
	{
		for (texel = 1; texel < (size_t) width * height; texel += 1)
		{
			CHECK(memcmp(buffer, buffer + texel * format->texelBytes,
				     (size_t) format->texelBytes) == 0,
				"attached target %s: texel %zu differs from texel 0",
				format->name, texel);
		}
	}
	FNA3D_SetRenderTargets(device, NULL, 0, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
	free(buffer);
	FNA3D_AddDisposeTexture(device, texture);
}

/* A level, rectangle or destination that does not fit is refused, and nothing is written. */
static void TestRefusals(FNA3D_Device *device)
{
	const Format *format = &formats[0];
	FNA3D_Texture *texture = FNA3D_CreateTexture2D(device, format->format, 4, 4, 2, 0);
	uint8_t pattern[64];
	uint8_t *buffer = GuardedBuffer(64);

	FillPattern(format, pattern, sizeof(pattern), 7);
	FNA3D_SetTextureData2D(device, texture, 0, 0, 4, 4, 0, pattern, sizeof(pattern));

	/* Destination one texel short. */
	FNA3D_GetTextureData2D(device, texture, 0, 0, 4, 4, 0, buffer, 60);
	CHECK(GuardIntact(buffer, 0, 64 + GUARD_BYTES), "short destination was written");
	/* Rectangle past the right edge, past the bottom, negative. */
	FNA3D_GetTextureData2D(device, texture, 2, 0, 3, 1, 0, buffer, 64);
	CHECK(GuardIntact(buffer, 0, 64 + GUARD_BYTES), "rectangle past the edge was read");
	FNA3D_GetTextureData2D(device, texture, 0, 3, 1, 2, 0, buffer, 64);
	CHECK(GuardIntact(buffer, 0, 64 + GUARD_BYTES), "rectangle past the bottom was read");
	FNA3D_GetTextureData2D(device, texture, -1, 0, 1, 1, 0, buffer, 64);
	CHECK(GuardIntact(buffer, 0, 64 + GUARD_BYTES), "negative rectangle was read");
	/* Level the texture does not have; level 1 is 2x2 so 4x4 does not fit it either. */
	FNA3D_GetTextureData2D(device, texture, 0, 0, 1, 1, 2, buffer, 64);
	CHECK(GuardIntact(buffer, 0, 64 + GUARD_BYTES), "missing level was read");
	FNA3D_GetTextureData2D(device, texture, 0, 0, 4, 4, 1, buffer, 64);
	CHECK(GuardIntact(buffer, 0, 64 + GUARD_BYTES), "rectangle larger than level 1 was read");

	free(buffer);
	FNA3D_AddDisposeTexture(device, texture);
}

int main(int argc, char **argv)
{
	FNA3D_PresentationParameters parameters;
	FNA3D_Device *device;
	SDL_Window *window;
	const char *label;
	size_t i;

	(void) argc;
	(void) argv;
	es3 = SDL_GetHintBoolean("FNA3D_OPENGL_FORCE_ES3", false);
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	window = SDL_CreateWindow("fna3d_cna_readback_tests", 64, 64,
				  FNA3D_PrepareWindowAttributes() | SDL_WINDOW_HIDDEN);
	if (window == NULL)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return 1;
	}

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
		return 1;
	}

	for (i = 0; i < SDL_arraysize(formats); i += 1)
	{
		const Format *format = &formats[i];
		TestTexture2D(device, format, 3, 2, 1);
		TestTexture2D(device, format, 5, 3, 1);
		TestTexture2D(device, format, 8, 2, 4);
		TestTexture2D(device, format, 2, 8, 4);
		TestCube(device, format, 3, 1);
		TestCube(device, format, 4, 3);
		TestVolume(device, format, 3, 3, 2);
		TestVolume(device, format, 5, 2, 3);
		TestAttachedTarget(device, format);
	}
	TestRefusals(device);
	label = (SDL_GL_GetCurrentContext() == NULL) ? "Non-GL driver" : (es3 ? "OpenGL ES 3" : "OpenGL");

	FNA3D_DestroyDevice(device);
	SDL_DestroyWindow(window);
	SDL_Quit();

	printf("%s: %d check(s), %d failed\n", label, checks, failures);
	return (failures == 0) ? 0 : 1;
}
