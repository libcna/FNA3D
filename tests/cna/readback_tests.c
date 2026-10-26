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
#define GL_UNPACK_ALIGNMENT_ENUM	0x0CF5
#define GL_UNPACK_ROW_LENGTH_ENUM	0x0CF2
#define GL_UNPACK_SKIP_ROWS_ENUM	0x0CF3
#define GL_UNPACK_SKIP_PIXELS_ENUM	0x0CF4
#define GL_UNPACK_SKIP_IMAGES_ENUM	0x806D
#define GL_UNPACK_IMAGE_HEIGHT_ENUM	0x806E

/* The GL names the ES 3 read-layout query needs (tests do not include a GL header). */
#define GL_RED_ENUM		0x1903
#define GL_ALPHA_ENUM		0x1906
#define GL_RGB_ENUM		0x1907
#define GL_RGBA_ENUM		0x1908
#define GL_RG_ENUM		0x8227
#define GL_BGRA_ENUM		0x80E1
#define GL_BYTE_ENUM		0x1400
#define GL_UNSIGNED_BYTE_ENUM	0x1401
#define GL_UNSIGNED_SHORT_ENUM	0x1403
#define GL_UNSIGNED_SHORT_5_6_5_ENUM		0x8363
#define GL_UNSIGNED_SHORT_4_4_4_4_REV_ENUM	0x8365
#define GL_UNSIGNED_SHORT_1_5_5_5_REV_ENUM	0x8366
#define GL_UNSIGNED_INT_2_10_10_10_REV_ENUM	0x8368
#define GL_TEXTURE_2D_ENUM			0x0DE1
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ENUM	0x8515
#define GL_TEXTURE_BINDING_2D_ENUM		0x8069
#define GL_TEXTURE_BINDING_CUBE_MAP_ENUM	0x8514
#define GL_ACTIVE_TEXTURE_ENUM			0x84E0
#define GL_TEXTURE0_ENUM			0x84C0
#define GL_FRAMEBUFFER_ENUM			0x8D40
#define GL_READ_FRAMEBUFFER_ENUM		0x8CA8
#define GL_DRAW_FRAMEBUFFER_ENUM		0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING_ENUM	0x8CAA
#define GL_DRAW_FRAMEBUFFER_BINDING_ENUM	0x8CA6
#define GL_COLOR_ATTACHMENT0_ENUM		0x8CE0
#define GL_FRAMEBUFFER_COMPLETE_ENUM		0x8CD5
#define GL_IMPLEMENTATION_COLOR_READ_TYPE_ENUM	0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT_ENUM	0x8B9B

static int failures = 0;
static int checks = 0;
static int es3 = 0;

/* The driver the device runs on (see IdentifyDriver). */
typedef enum Driver
{
	DRIVER_UNKNOWN,
	DRIVER_OPENGL,
	DRIVER_D3D11,
	DRIVER_SDLGPU
} Driver;
static Driver driver = DRIVER_UNKNOWN;
static Driver announcedDriver = DRIVER_UNKNOWN;

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
	 * unsigned normalized textures). On ES 3 RGBA8, sRGB RGBA8 and the float formats are always
	 * read; any other format exactly when the implementation reads the level in the format's own
	 * layout (glFormat/glType), and is refused otherwise -- see Readable(). */
	uint8_t exactRoundTrip;
	/* A float format, read back on ES 3 too. */
	uint8_t isFloat;
	/* Halves rather than floats. */
	uint8_t isHalf;
	/* The texels' own GL layout, as FNA3D uploads them. */
	uint32_t glFormat;
	uint32_t glType;
} Format;

static const Format formats[] =
{
	{ FNA3D_SURFACEFORMAT_COLOR,		"Color",		4, 1, 0, 0,
	  GL_RGBA_ENUM, GL_UNSIGNED_BYTE_ENUM },
	{ FNA3D_SURFACEFORMAT_BGR565,		"Bgr565",		2, 1, 0, 0,
	  GL_RGB_ENUM, GL_UNSIGNED_SHORT_5_6_5_ENUM },
	{ FNA3D_SURFACEFORMAT_BGRA5551,		"Bgra5551",		2, 1, 0, 0,
	  GL_BGRA_ENUM, GL_UNSIGNED_SHORT_1_5_5_5_REV_ENUM },
	{ FNA3D_SURFACEFORMAT_BGRA4444,		"Bgra4444",		2, 1, 0, 0,
	  GL_BGRA_ENUM, GL_UNSIGNED_SHORT_4_4_4_4_REV_ENUM },
	{ FNA3D_SURFACEFORMAT_NORMALIZEDBYTE2,	"NormalizedByte2",	2, 0, 0, 0,
	  GL_RG_ENUM, GL_BYTE_ENUM },
	{ FNA3D_SURFACEFORMAT_NORMALIZEDBYTE4,	"NormalizedByte4",	4, 0, 0, 0,
	  GL_RGBA_ENUM, GL_BYTE_ENUM },
	{ FNA3D_SURFACEFORMAT_RGBA1010102,	"Rgba1010102",		4, 1, 0, 0,
	  GL_RGBA_ENUM, GL_UNSIGNED_INT_2_10_10_10_REV_ENUM },
	{ FNA3D_SURFACEFORMAT_RG32,		"Rg32",			4, 1, 0, 0,
	  GL_RG_ENUM, GL_UNSIGNED_SHORT_ENUM },
	{ FNA3D_SURFACEFORMAT_RGBA64,		"Rgba64",		8, 1, 0, 0,
	  GL_RGBA_ENUM, GL_UNSIGNED_SHORT_ENUM },
	{ FNA3D_SURFACEFORMAT_ALPHA8,		"Alpha8",		1, 1, 0, 0,
	  GL_ALPHA_ENUM, GL_UNSIGNED_BYTE_ENUM },
	{ FNA3D_SURFACEFORMAT_SINGLE,		"Single",		4, 1, 1, 0, 0, 0 },
	{ FNA3D_SURFACEFORMAT_VECTOR2,		"Vector2",		8, 1, 1, 0, 0, 0 },
	{ FNA3D_SURFACEFORMAT_VECTOR4,		"Vector4",		16, 1, 1, 0, 0, 0 },
	{ FNA3D_SURFACEFORMAT_HALFSINGLE,	"HalfSingle",		2, 1, 1, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_HALFVECTOR2,	"HalfVector2",		4, 1, 1, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_HALFVECTOR4,	"HalfVector4",		8, 1, 1, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_HDRBLENDABLE,	"HdrBlendable",		8, 1, 1, 1, 0, 0 },
	{ FNA3D_SURFACEFORMAT_COLORBGRA_EXT,	"ColorBgraEXT",		4, 1, 0, 0,
	  GL_BGRA_ENUM, GL_UNSIGNED_BYTE_ENUM },
	{ FNA3D_SURFACEFORMAT_COLORSRGB_EXT,	"ColorSrgbEXT",		4, 1, 0, 0,
	  GL_RGBA_ENUM, GL_UNSIGNED_BYTE_ENUM },
	{ FNA3D_SURFACEFORMAT_BYTE_EXT,		"ByteEXT",		1, 1, 0, 0,
	  GL_RED_ENUM, GL_UNSIGNED_BYTE_ENUM },
	{ FNA3D_SURFACEFORMAT_USHORT_EXT,	"UShortEXT",		2, 1, 0, 0,
	  GL_RED_ENUM, GL_UNSIGNED_SHORT_ENUM },
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

static Driver DriverNamed(const char *name)
{
	if (SDL_strcasecmp(name, "OpenGL") == 0)
	{
		return DRIVER_OPENGL;
	}
	if (SDL_strcasecmp(name, "D3D11") == 0)
	{
		return DRIVER_D3D11;
	}
	if (	SDL_strcasecmp(name, "SDLGPU") == 0 ||
		SDL_strcasecmp(name, "Vulkan") == 0 ||
		SDL_strcasecmp(name, "D3D12") == 0 ||
		SDL_strcasecmp(name, "Metal") == 0	)
	{
		return DRIVER_SDLGPU; /* FNA3D_PrepareWindowAttributes' aliases */
	}
	return DRIVER_UNKNOWN;
}

static const char *DriverLabel(Driver which)
{
	switch (which)
	{
		case DRIVER_OPENGL:	return es3 ? "OpenGL ES 3" : "OpenGL";
		case DRIVER_D3D11:	return "D3D11";
		case DRIVER_SDLGPU:	return "SDL_GPU";
		default:		return "an unknown driver";
	}
}

/* FNA3D's log, as FNA3D's own default sends it; the OpenGL and D3D11 drivers name themselves in
 * it ("FNA3D Driver: <name>") as they make the device.
 */
static void FNA3DCALL LogInfo(const char *message)
{
	const char prefix[] = "FNA3D Driver: ";
	if (SDL_strncmp(message, prefix, sizeof(prefix) - 1) == 0)
	{
		announcedDriver = DriverNamed(message + sizeof(prefix) - 1);
	}
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
}

static void FNA3DCALL LogWarn(const char *message)
{
	SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
}

static void FNA3DCALL LogError(const char *message)
{
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
}

/* Which driver made the device. The OpenGL and D3D11 drivers announce themselves; the SDL_GPU
 * driver announces no name, so a device no driver announced is SDL_GPU's. FNA3D_GetSysRendererEXT
 * would say, but a TRACING_SUPPORT build asserts in it. When FNA3D_FORCE_DRIVER names the driver
 * -- as every registered test does -- the device must be that driver's.
 */
static Driver IdentifyDriver(void)
{
	const char *forced = SDL_GetHint("FNA3D_FORCE_DRIVER");
	const Driver made = (announcedDriver != DRIVER_UNKNOWN) ? announcedDriver : DRIVER_SDLGPU;
	if (forced != NULL && forced[0] != '\0')
	{
		CHECK(DriverNamed(forced) == made,
			"FNA3D_FORCE_DRIVER=%s, but the device is %s's", forced, DriverLabel(made));
	}
	return made;
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

static void SetPackAlignment(int32_t alignment)
{
	typedef void (*PixelStorei)(uint32_t, int32_t);
	PixelStorei pixelStorei = (PixelStorei) SDL_GL_GetProcAddress("glPixelStorei");
	if (pixelStorei != NULL)
	{
		pixelStorei(GL_PACK_ALIGNMENT_ENUM, alignment);
	}
}

static void *GLFunction(const char *name)
{
	void *function = (void*) SDL_GL_GetProcAddress(name);
	if (function == NULL)
	{
		fprintf(stderr, "no %s in this GL context\n", name);
		exit(1);
	}
	return function;
}

static int32_t GLInteger(uint32_t name)
{
	typedef void (*GetIntegerv)(uint32_t, int32_t*);
	GetIntegerv getIntegerv = (GetIntegerv) GLFunction("glGetIntegerv");
	int32_t value = 0;
	getIntegerv(name, &value);
	return value;
}

/* OpenGL ES 3 only: the GL name of the texture FNA3D left bound to unit 0 for a binding point,
 * as each of its SetTextureData and CreateTexture calls leaves it.
 */
static uint32_t BoundTexture(uint32_t bindingName)
{
	if (!es3)
	{
		return 0;
	}
	if (GLInteger(GL_ACTIVE_TEXTURE_ENUM) != GL_TEXTURE0_ENUM)
	{
		fprintf(stderr, "FNA3D left texture unit %d active, not 0\n",
			GLInteger(GL_ACTIVE_TEXTURE_ENUM) - GL_TEXTURE0_ENUM);
		exit(1);
	}
	return (uint32_t) GLInteger(bindingName);
}

/* Whether this context must read the level back. Always, but on OpenGL ES 3 -- which has no
 * glGetTexImage, so GetData attaches the level and glReadPixels it -- only RGBA8, sRGB RGBA8 and
 * the float formats are always readable (as RGBA/UNSIGNED_BYTE and RGBA/FLOAT, ES 3.2 section
 * 16.1.2); any other format must be read when the implementation's own pair for the attached
 * level, GL_IMPLEMENTATION_COLOR_READ_FORMAT/TYPE, is the format's layout, and refused when it is
 * not, or when the level is no complete colour attachment. The pair is asked of the level
 * attached to a framebuffer of the test's own; FNA3D's framebuffer bindings are restored.
 */
static int Readable(const Format *format, uint32_t texture, uint32_t imageTarget, int32_t level)
{
	typedef void (*GenFramebuffers)(int32_t, uint32_t*);
	typedef void (*BindFramebuffer)(uint32_t, uint32_t);
	typedef void (*FramebufferTexture2D)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t);
	typedef uint32_t (*CheckFramebufferStatus)(uint32_t);
	typedef uint32_t (*GetError)(void);
	GenFramebuffers genFramebuffers;
	GenFramebuffers deleteFramebuffers;
	BindFramebuffer bindFramebuffer;
	FramebufferTexture2D framebufferTexture2D;
	CheckFramebufferStatus checkFramebufferStatus;
	GetError getError;
	uint32_t framebuffer = 0;
	int32_t previousRead, previousDraw;
	int32_t readFormat = 0, readType = 0;
	int complete;

	if (	!es3 ||
		format->isFloat ||
		format->format == FNA3D_SURFACEFORMAT_COLOR ||
		format->format == FNA3D_SURFACEFORMAT_COLORSRGB_EXT	)
	{
		return 1;
	}
	genFramebuffers = (GenFramebuffers) GLFunction("glGenFramebuffers");
	deleteFramebuffers = (GenFramebuffers) GLFunction("glDeleteFramebuffers");
	bindFramebuffer = (BindFramebuffer) GLFunction("glBindFramebuffer");
	framebufferTexture2D = (FramebufferTexture2D) GLFunction("glFramebufferTexture2D");
	checkFramebufferStatus = (CheckFramebufferStatus) GLFunction("glCheckFramebufferStatus");
	getError = (GetError) GLFunction("glGetError");

	previousRead = GLInteger(GL_READ_FRAMEBUFFER_BINDING_ENUM);
	previousDraw = GLInteger(GL_DRAW_FRAMEBUFFER_BINDING_ENUM);
	genFramebuffers(1, &framebuffer);
	bindFramebuffer(GL_FRAMEBUFFER_ENUM, framebuffer);
	framebufferTexture2D(GL_FRAMEBUFFER_ENUM, GL_COLOR_ATTACHMENT0_ENUM, imageTarget, texture,
			     level);
	complete = checkFramebufferStatus(GL_FRAMEBUFFER_ENUM) == GL_FRAMEBUFFER_COMPLETE_ENUM;
	if (complete)
	{
		readFormat = GLInteger(GL_IMPLEMENTATION_COLOR_READ_FORMAT_ENUM);
		readType = GLInteger(GL_IMPLEMENTATION_COLOR_READ_TYPE_ENUM);
	}
	bindFramebuffer(GL_READ_FRAMEBUFFER_ENUM, (uint32_t) previousRead);
	bindFramebuffer(GL_DRAW_FRAMEBUFFER_ENUM, (uint32_t) previousDraw);
	deleteFramebuffers(1, &framebuffer);
	while (getError() != 0)
	{
		/* Leave FNA3D no error of the test's. */
	}
	return	complete &&
		(uint32_t) readFormat == format->glFormat &&
		(uint32_t) readType == format->glType;
}

static int Untouched(const uint8_t *buffer, size_t bytes)
{
	return GuardIntact(buffer, 0, bytes);
}

/* Reads (x, y, w, h) of one level and checks the guard, and the bytes against the uploaded level
 * where the format keeps them exactly. */
static void CheckRead2D(
	FNA3D_Device *device,
	FNA3D_Texture *texture,
	uint32_t glTexture,
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
	const int readable = Readable(format, glTexture, GL_TEXTURE_2D_ENUM, level);
	int32_t row;

	FNA3D_GetTextureData2D(device, texture, x, y, w, h, level, buffer, (int32_t) bytes);
	CHECK(GuardIntact(buffer, bytes, GUARD_BYTES),
		"%s %s level %d (%d,%d %dx%d): wrote past %zu bytes",
		what, format->name, level, x, y, w, h, bytes);
	if (!readable)
	{
		CHECK(Untouched(buffer, bytes),
			"%s %s level %d (%d,%d %dx%d): OpenGL ES 3 reads it in another layout, "
			"but GetData wrote the destination",
			what, format->name, level, x, y, w, h);
	}
	else if (format->exactRoundTrip)
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
	uint32_t glTexture;
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
	glTexture = BoundTexture(GL_TEXTURE_BINDING_2D_ENUM);
	for (level = 0; level < levels; level += 1)
	{
		const int32_t w = MipSize(width, level);
		const int32_t h = MipSize(height, level);
		CheckRead2D(device, texture, glTexture, format, level, w, uploads[level],
			    0, 0, w, h, what);
		if (w > 1 && h > 1)
		{
			CheckRead2D(device, texture, glTexture, format, level, w, uploads[level],
				    1, 1, w - 1, h - 1, what);
		}
		CheckRead2D(device, texture, glTexture, format, level, w, uploads[level],
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
			int readable;
			FillPattern(format, uploaded, bytes, (uint32_t) (face * 13 + level));
			FNA3D_SetTextureDataCube(device, texture, 0, 0, s, s,
						 (FNA3D_CubeMapFace) face, level, uploaded,
						 (int32_t) bytes);
			readable = Readable(format, BoundTexture(GL_TEXTURE_BINDING_CUBE_MAP_ENUM),
					    GL_TEXTURE_CUBE_MAP_POSITIVE_X_ENUM + (uint32_t) face,
					    level);
			FNA3D_GetTextureDataCube(device, texture, 0, 0, s, s,
						 (FNA3D_CubeMapFace) face, level, buffer,
						 (int32_t) bytes);
			CHECK(GuardIntact(buffer, bytes, GUARD_BYTES),
				"cube %d %s face %d level %d: wrote past %zu bytes",
				size, format->name, face, level, bytes);
			if (!readable)
			{
				CHECK(Untouched(buffer, bytes),
					"cube %d %s face %d level %d: OpenGL ES 3 reads it in another "
					"layout, but GetData wrote the destination",
					size, format->name, face, level);
			}
			else if (format->exactRoundTrip)
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

/* A render target still attached to the framebuffer: the path that read RGBA8 regardless.
 *
 * The target is cleared to (0.25, 0.5, 0.75, 1); where that colour has one encoding -- RGBA8 and
 * BGRA8 to within rounding, sRGB RGBA8 sRGB-encoded to within rounding where the target encodes
 * (always on ES 3; elsewhere where FNA3D reports sRGB render targets), the float and half formats
 * exactly -- the texels must hold it, and any other format must at least have been written,
 * uniformly, so that a read which does nothing fails instead of passing on the untouched
 * destination. On ES 3 a format the implementation does not read in its own layout must be
 * refused, leaving the destination untouched (Readable()).
 */
static int ExpectedClear(const Format *format, int encodesSrgb, uint8_t *expected,
			 int *tolerance)
{
	const float floats[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
	const uint16_t halves[4] = { 0x3400, 0x3800, 0x3A00, 0x3C00 };
	const uint8_t rgba[4] = { 64, 128, 191, 255 };
	const uint8_t bgra[4] = { 191, 128, 64, 255 };
	const uint8_t srgb[4] = { 137, 188, 225, 255 };

	*tolerance = 0;
	if (format->isFloat && format->isHalf)
	{
		memcpy(expected, halves, (size_t) format->texelBytes);
		return 1;
	}
	if (format->isFloat)
	{
		memcpy(expected, floats, (size_t) format->texelBytes);
		return 1;
	}
	if (format->format == FNA3D_SURFACEFORMAT_COLOR)
	{
		memcpy(expected, rgba, 4);
		*tolerance = 1;
		return 1;
	}
	if (format->format == FNA3D_SURFACEFORMAT_COLORBGRA_EXT)
	{
		memcpy(expected, bgra, 4);
		*tolerance = 1;
		return 1;
	}
	if (format->format == FNA3D_SURFACEFORMAT_COLORSRGB_EXT && encodesSrgb)
	{
		memcpy(expected, srgb, 4);
		*tolerance = 1;
		return 1;
	}
	return 0;
}

static void TestAttachedTarget(FNA3D_Device *device, const Format *format)
{
	const int32_t width = 5, height = 3;
	const size_t bytes = (size_t) width * height * (size_t) format->texelBytes;
	FNA3D_Texture *texture = FNA3D_CreateTexture2D(device, format->format, width, height, 1, 1);
	FNA3D_RenderTargetBinding binding;
	FNA3D_Vec4 color = { 0.25f, 0.5f, 0.75f, 1.0f };
	uint8_t *buffer = GuardedBuffer(bytes);
	const int readable = Readable(format, BoundTexture(GL_TEXTURE_BINDING_2D_ENUM),
				      GL_TEXTURE_2D_ENUM, 0);
	const int encodesSrgb = es3 || FNA3D_SupportsSRGBRenderTargets(device);
	uint8_t expected[16];
	int tolerance = 0;
	size_t texel, i;

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
	if (!readable)
	{
		CHECK(Untouched(buffer, bytes),
			"attached target %s: OpenGL ES 3 reads it in another layout, "
			"but GetData wrote the destination", format->name);
	}
	else
	{
		CHECK(!Untouched(buffer, bytes),
			"attached target %s: GetData wrote nothing", format->name);
		for (texel = 1; texel < (size_t) width * height; texel += 1)
		{
			CHECK(memcmp(buffer, buffer + texel * format->texelBytes,
				     (size_t) format->texelBytes) == 0,
				"attached target %s: texel %zu differs from texel 0",
				format->name, texel);
		}
		if (ExpectedClear(format, encodesSrgb, expected, &tolerance))
		{
			for (i = 0; i < (size_t) format->texelBytes; i += 1)
			{
				const int difference = (int) buffer[i] - (int) expected[i];
				CHECK(difference >= -tolerance && difference <= tolerance,
					"attached target %s: byte %zu is %d, the clear colour gives %d",
					format->name, i, buffer[i], expected[i]);
			}
		}
	}
	FNA3D_SetRenderTargets(device, NULL, 0, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
	free(buffer);
	FNA3D_AddDisposeTexture(device, texture);
}

/* A 4x multisampled target, cleared and resolved, reads back as the clear colour: RGBA8 and sRGB
 * RGBA8, the sRGB one encoded where the target encodes. Its samples live in a renderbuffer, which
 * OpenGL creates from the format's internal format -- an unsized GL_SRGB_ALPHA_EXT, which no
 * renderbuffer takes (GL_INVALID_ENUM, desktop GL and ES 3 alike), left the sRGB target without
 * storage, so nothing was cleared or resolved.
 */
static void TestMultisampledTarget(FNA3D_Device *device, FNA3D_SurfaceFormat surfaceFormat,
				   const char *name)
{
	const int32_t width = 4, height = 2;
	const size_t bytes = (size_t) width * height * 4;
	FNA3D_Texture *texture = FNA3D_CreateTexture2D(device, surfaceFormat, width, height, 1, 1);
	FNA3D_Renderbuffer *renderbuffer = FNA3D_GenColorRenderbuffer(device, width, height,
								      surfaceFormat, 4, texture);
	FNA3D_RenderTargetBinding binding;
	FNA3D_Vec4 color = { 0.25f, 0.5f, 0.75f, 1.0f };
	const int srgb = surfaceFormat == FNA3D_SURFACEFORMAT_COLORSRGB_EXT;
	const uint8_t linear[4] = { 64, 128, 191, 255 };
	const uint8_t encoded[4] = { 137, 188, 225, 255 };
	const uint8_t *expected = (srgb && (es3 || FNA3D_SupportsSRGBRenderTargets(device))) ?
		encoded : linear;
	uint8_t *buffer = GuardedBuffer(bytes);
	size_t i;

	CHECK(renderbuffer != NULL, "4x multisampled %s target: no renderbuffer", name);
	SDL_memset(&binding, '\0', sizeof(binding));
	binding.type = FNA3D_RENDERTARGET_TYPE_2D;
	binding.twod.width = width;
	binding.twod.height = height;
	binding.levelCount = 1;
	binding.multiSampleCount = 4;
	binding.texture = texture;
	binding.colorBuffer = renderbuffer;
	FNA3D_SetRenderTargets(device, &binding, 1, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
	FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &color, 0.0f, 0);
	FNA3D_SetRenderTargets(device, NULL, 0, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
	FNA3D_ResolveTarget(device, &binding);
	FNA3D_GetTextureData2D(device, texture, 0, 0, width, height, 0, buffer, (int32_t) bytes);
	CHECK(GuardIntact(buffer, bytes, GUARD_BYTES),
		"4x multisampled %s target: wrote past %zu bytes", name, bytes);
	for (i = 0; i < bytes; i += 1)
	{
		const int difference = (int) buffer[i] - (int) expected[i % 4];
		if (difference < -1 || difference > 1)
		{
			CHECK(0, "4x multisampled %s target: byte %zu is %d, the clear colour gives %d",
			      name, i, buffer[i], expected[i % 4]);
			break;
		}
	}
	free(buffer);
	FNA3D_AddDisposeRenderbuffer(device, renderbuffer);
	FNA3D_AddDisposeTexture(device, texture);
}

/* OpenGL only: a read is tightly packed whatever pack alignment another user of the context left,
 * and leaves that alignment as it found it -- a texture level, an attached target and the
 * backbuffer, each with rows that an alignment of 8 would pad.
 */
static void TestForeignPackAlignment(FNA3D_Device *device)
{
	const Format *half = NULL;
	FNA3D_Texture *texture;
	FNA3D_RenderTargetBinding binding;
	FNA3D_Vec4 color = { 0.25f, 0.5f, 0.75f, 1.0f };
	uint8_t uploaded[12];
	uint8_t *buffer;
	size_t i;

	if (SDL_GL_GetCurrentContext() == NULL)
	{
		return;
	}
	for (i = 0; i < SDL_arraysize(formats); i += 1)
	{
		if (formats[i].format == FNA3D_SURFACEFORMAT_HALFSINGLE)
		{
			half = &formats[i];
		}
	}

	/* 3x2 HalfSingle: six-byte rows. */
	texture = FNA3D_CreateTexture2D(device, half->format, 3, 2, 1, 0);
	FillPattern(half, uploaded, sizeof(uploaded), 5);
	FNA3D_SetTextureData2D(device, texture, 0, 0, 3, 2, 0, uploaded, sizeof(uploaded));
	SetPackAlignment(8);
	buffer = GuardedBuffer(sizeof(uploaded));
	FNA3D_GetTextureData2D(device, texture, 0, 0, 3, 2, 0, buffer, sizeof(uploaded));
	CHECK(GuardIntact(buffer, sizeof(uploaded), GUARD_BYTES),
		"pack alignment 8: HalfSingle 3x2 read wrote past 12 bytes");
	CHECK(memcmp(buffer, uploaded, sizeof(uploaded)) == 0,
		"pack alignment 8: HalfSingle 3x2 read differs from the upload");
	CHECK(PackAlignment() == 8, "pack alignment 8: texture read left it at %d", PackAlignment());
	free(buffer);
	FNA3D_AddDisposeTexture(device, texture);

	/* 3x2 RGBA8 attached target: twelve-byte rows. */
	texture = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, 3, 2, 1, 1);
	SDL_memset(&binding, '\0', sizeof(binding));
	binding.type = FNA3D_RENDERTARGET_TYPE_2D;
	binding.twod.width = 3;
	binding.twod.height = 2;
	binding.levelCount = 1;
	binding.texture = texture;
	FNA3D_SetRenderTargets(device, &binding, 1, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
	FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &color, 0.0f, 0);
	SetPackAlignment(8);
	buffer = GuardedBuffer(24);
	FNA3D_GetTextureData2D(device, texture, 0, 0, 3, 2, 0, buffer, 24);
	CHECK(GuardIntact(buffer, 24, GUARD_BYTES),
		"pack alignment 8: attached 3x2 target read wrote past 24 bytes");
	CHECK(!Untouched(buffer, 24) && buffer[0] == buffer[20] && buffer[3] == 255,
		"pack alignment 8: attached 3x2 target read is not the clear colour, tightly packed");
	CHECK(PackAlignment() == 8, "pack alignment 8: target read left it at %d", PackAlignment());
	free(buffer);
	FNA3D_SetRenderTargets(device, NULL, 0, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
	FNA3D_AddDisposeTexture(device, texture);

	/* 3x2 of the RGBA8 backbuffer. */
	FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &color, 0.0f, 0);
	SetPackAlignment(8);
	buffer = GuardedBuffer(24);
	FNA3D_ReadBackbuffer(device, 1, 1, 3, 2, buffer, 24);
	CHECK(GuardIntact(buffer, 24, GUARD_BYTES),
		"pack alignment 8: backbuffer 3x2 read wrote past 24 bytes");
	CHECK(!Untouched(buffer, 24) && buffer[0] == buffer[20] && buffer[3] == 255,
		"pack alignment 8: backbuffer 3x2 read is not the clear colour, tightly packed");
	CHECK(PackAlignment() == 8, "pack alignment 8: backbuffer read left it at %d", PackAlignment());
	free(buffer);
	SetPackAlignment(4);
}

/* OpenGL only: an upload reads the caller's rows tightly packed whatever unpack state another user
 * of the context left, and leaves that state as it found it -- a 2D level, a cube face and a
 * volume, first with only GL_UNPACK_ALIGNMENT 8 (which pads the rows of every one of these), then
 * with a row length and skipped rows, pixels and images as well. Each upload comes from an exactly
 * sized allocation, so a read past it shows under AddressSanitizer; the 2D level and the face are
 * read back. The volume has no readback.
 */
static const uint32_t unpackNames[6] =
{
	GL_UNPACK_ALIGNMENT_ENUM,
	GL_UNPACK_ROW_LENGTH_ENUM,
	GL_UNPACK_SKIP_ROWS_ENUM,
	GL_UNPACK_SKIP_PIXELS_ENUM,
	GL_UNPACK_IMAGE_HEIGHT_ENUM,
	GL_UNPACK_SKIP_IMAGES_ENUM
};

static void SetUnpackState(const int32_t *values)
{
	typedef void (*PixelStorei)(uint32_t, int32_t);
	PixelStorei pixelStorei = (PixelStorei) GLFunction("glPixelStorei");
	size_t i;
	for (i = 0; i < SDL_arraysize(unpackNames); i += 1)
	{
		pixelStorei(unpackNames[i], values[i]);
	}
}

static void CheckUnpackState(const int32_t *values, const char *what)
{
	size_t i;
	for (i = 0; i < SDL_arraysize(unpackNames); i += 1)
	{
		const int32_t value = GLInteger(unpackNames[i]);
		CHECK(value == values[i], "%s: unpack state 0x%04X left at %d, not %d",
			what, unpackNames[i], value, values[i]);
	}
}

static void ForeignUnpackUploads(FNA3D_Device *device, const Format *format,
				 const int32_t *foreign, const char *state)
{
	const size_t rowBytes = 3 * (size_t) format->texelBytes;
	FNA3D_Texture *texture;
	uint8_t *uploaded;
	uint8_t *buffer;
	char what[96];

	/* 3x2 level: rows of three texels, which an alignment of 8 pads. */
	SDL_snprintf(what, sizeof(what), "%s, %s 3x2 upload", state, format->name);
	texture = FNA3D_CreateTexture2D(device, format->format, 3, 2, 1, 0);
	uploaded = (uint8_t*) malloc(rowBytes * 2);
	FillPattern(format, uploaded, rowBytes * 2, 11);
	SetUnpackState(foreign);
	FNA3D_SetTextureData2D(device, texture, 0, 0, 3, 2, 0, uploaded, (int32_t) (rowBytes * 2));
	CheckUnpackState(foreign, what);
	buffer = GuardedBuffer(rowBytes * 2);
	FNA3D_GetTextureData2D(device, texture, 0, 0, 3, 2, 0, buffer, (int32_t) (rowBytes * 2));
	CHECK(memcmp(buffer, uploaded, rowBytes * 2) == 0, "%s: read back differs", what);
	free(buffer);
	free(uploaded);
	FNA3D_AddDisposeTexture(device, texture);

	/* 3x3 cube face. */
	SDL_snprintf(what, sizeof(what), "%s, %s 3x3 cube face upload", state, format->name);
	texture = FNA3D_CreateTextureCube(device, format->format, 3, 1, 0);
	uploaded = (uint8_t*) malloc(rowBytes * 3);
	FillPattern(format, uploaded, rowBytes * 3, 12);
	SetUnpackState(foreign);
	FNA3D_SetTextureDataCube(device, texture, 0, 0, 3, 3, FNA3D_CUBEMAPFACE_POSITIVEX, 0,
				 uploaded, (int32_t) (rowBytes * 3));
	CheckUnpackState(foreign, what);
	buffer = GuardedBuffer(rowBytes * 3);
	FNA3D_GetTextureDataCube(device, texture, 0, 0, 3, 3, FNA3D_CUBEMAPFACE_POSITIVEX, 0,
				 buffer, (int32_t) (rowBytes * 3));
	CHECK(memcmp(buffer, uploaded, rowBytes * 3) == 0, "%s: read back differs", what);
	free(buffer);
	free(uploaded);
	FNA3D_AddDisposeTexture(device, texture);

	/* 3x3x2 volume. */
	SDL_snprintf(what, sizeof(what), "%s, %s 3x3x2 volume upload", state, format->name);
	texture = FNA3D_CreateTexture3D(device, format->format, 3, 3, 2, 1);
	uploaded = (uint8_t*) malloc(rowBytes * 6);
	FillPattern(format, uploaded, rowBytes * 6, 13);
	SetUnpackState(foreign);
	FNA3D_SetTextureData3D(device, texture, 0, 0, 0, 3, 3, 2, 0, uploaded,
			       (int32_t) (rowBytes * 6));
	CheckUnpackState(foreign, what);
	free(uploaded);
	FNA3D_AddDisposeTexture(device, texture);
}

static void TestForeignUnpackState(FNA3D_Device *device)
{
	const int32_t defaults[6] = { 4, 0, 0, 0, 0, 0 };
	const int32_t alignment8[6] = { 8, 0, 0, 0, 0, 0 };
	const int32_t everything[6] = { 8, 7, 1, 1, 5, 1 };
	const Format *half = NULL;
	const Format *color = &formats[0];
	size_t i;

	if (SDL_GL_GetCurrentContext() == NULL)
	{
		return;
	}
	for (i = 0; i < SDL_arraysize(formats); i += 1)
	{
		if (formats[i].format == FNA3D_SURFACEFORMAT_HALFSINGLE)
		{
			half = &formats[i];
		}
	}
	/* HalfSingle: FNA3D sets an alignment of 2. Color: 4, which it used not to set at all. */
	ForeignUnpackUploads(device, half, alignment8, "unpack alignment 8");
	ForeignUnpackUploads(device, color, alignment8, "unpack alignment 8");
	ForeignUnpackUploads(device, half, everything, "foreign unpack state");
	ForeignUnpackUploads(device, color, everything, "foreign unpack state");
	SetUnpackState(defaults);
}

/* SDL_GPU only copies the rectangle out of its transfer buffer: a destination larger than the
 * rectangle keeps its tail, whatever an earlier, larger read left in the buffer.
 */
static void TestOversizedDestination(FNA3D_Device *device)
{
	const Format *format = &formats[0];
	FNA3D_Texture *texture = FNA3D_CreateTexture2D(device, format->format, 4, 4, 1, 0);
	uint8_t uploaded[64];
	uint8_t *buffer = GuardedBuffer(64);
	size_t i;
	int tailIntact = 1;

	FillPattern(format, uploaded, sizeof(uploaded), 3);
	FNA3D_SetTextureData2D(device, texture, 0, 0, 4, 4, 0, uploaded, sizeof(uploaded));
	/* A whole-level read first, so a transfer buffer holds 64 bytes of texels... */
	FNA3D_GetTextureData2D(device, texture, 0, 0, 4, 4, 0, buffer, 64);
	free(buffer);
	/* ...then a 2x2 read into a 64-byte destination: only its first 16 bytes may change. */
	buffer = GuardedBuffer(64);
	FNA3D_GetTextureData2D(device, texture, 0, 0, 2, 2, 0, buffer, 64);
	for (i = 16; i < 64; i += 1)
	{
		tailIntact = tailIntact && (buffer[i] == GUARD);
	}
	CHECK(tailIntact, "2x2 read into a 64-byte destination wrote past its 16 bytes");
	CHECK(GuardIntact(buffer, 64, GUARD_BYTES), "2x2 read wrote past the 64-byte destination");
	free(buffer);
	FNA3D_AddDisposeTexture(device, texture);
}

/* A block-compressed level is read in whole blocks: a 4x4 DXT1 level is one 8-byte block, an 8x8
 * one four. The SDL_GPU driver reads them back exactly; OpenGL and D3D11 have no compressed
 * readback and must refuse, leaving the destination untouched -- which driver this is comes from
 * IdentifyDriver(), not from whether a GL context exists, which D3D11 has no more than SDL_GPU.
 * Either way nothing may be written past the blocks -- CNA probes this with an 8-byte block on
 * the stack.
 */
static void TestCompressedReadback(FNA3D_Device *device)
{
	const int32_t sizes[2] = { 4, 8 };
	int i;
	for (i = 0; i < 2; i += 1)
	{
		const int32_t size = sizes[i];
		const size_t bytes = (size_t) ((size / 4) * (size / 4) * 8);
		FNA3D_Texture *texture = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_DXT1,
							       size, size, 1, 0);
		uint8_t blocks[32];
		uint8_t *buffer = GuardedBuffer(bytes);
		size_t b;
		for (b = 0; b < sizeof(blocks); b += 1)
		{
			blocks[b] = (uint8_t) (0x11 * (b % 8) + b / 8);
		}
		FNA3D_SetTextureData2D(device, texture, 0, 0, size, size, 0, blocks, (int32_t) bytes);
		FNA3D_GetTextureData2D(device, texture, 0, 0, size, size, 0, buffer, (int32_t) bytes);
		CHECK(GuardIntact(buffer, bytes, GUARD_BYTES),
			"DXT1 %dx%d: wrote past its %zu bytes of blocks", size, size, bytes);
		if (driver == DRIVER_SDLGPU)
		{
			CHECK(memcmp(buffer, blocks, bytes) == 0,
				"DXT1 %dx%d: the blocks read back differ from the upload", size, size);
		}
		else
		{
			CHECK(Untouched(buffer, bytes),
				"DXT1 %dx%d: %s has no compressed readback but wrote the destination",
				size, size, DriverLabel(driver));
		}
		free(buffer);
		FNA3D_AddDisposeTexture(device, texture);
	}
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
	FNA3D_HookLogFunctions(LogInfo, LogWarn, LogError);
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
	driver = IdentifyDriver();

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
	TestMultisampledTarget(device, FNA3D_SURFACEFORMAT_COLOR, "Color");
	TestMultisampledTarget(device, FNA3D_SURFACEFORMAT_COLORSRGB_EXT, "ColorSrgbEXT");
	TestRefusals(device);
	TestCompressedReadback(device);
	TestForeignPackAlignment(device);
	TestForeignUnpackState(device);
	TestOversizedDestination(device);
	label = DriverLabel(driver);

	FNA3D_DestroyDevice(device);
	SDL_DestroyWindow(window);
	SDL_Quit();

	printf("%s: %d check(s), %d failed\n", label, checks, failures);
	return (failures == 0) ? 0 : 1;
}
