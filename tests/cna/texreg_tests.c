/* SPDX-License-Identifier: Zlib */
/* D3D9 TEXREG2AR/GB/RGB sample the destination stage using source.AR/GB/RGB.
 * The first texture is (32,96,160,224). Point sampling a 4x4 grid therefore selects
 * (3,0), (1,2), (0,1): floor(4 * selected byte / 255). RGB_bx2 has
 * negative x/y and clamps to (0,0). These are hand-derived texels.
 * Instruction contracts: learn.microsoft.com/en-us/windows/win32/direct3dhlsl/
 * texreg2ar---ps, texreg2gb---ps, texreg2rgb---ps, texdp3---ps, texdp3tex---ps.
 * Dot coordinates for stage k are (2+k/4,1,-1/2,9). With the byte source,
 * dot=(80+8k)/255; _bx2 makes it negative. DP3 outputs RGBA88/96/104 by stage.
 * DP3TEX selects lookup texel (1,0), or (0,0) with _bx2. */
#include "texture_fixture.h"

static int TexRegProgram(int opcode, int source, int destination, int modifier)
{
	uint32_t vs[] = {0xFFFE0200, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A,
					 0x900F0001, 0x02000001, 0xC00F0000, 0x90E40000, 0x02000001, 0xE00F0000,
					 0x90E40001, 0x02000001, 0xE00F0001, 0x90E40001, 0x02000001, 0xE00F0002,
					 0x90E40001, 0x02000001, 0xE00F0003, 0x90E40001, 0x0000FFFF};
	uint32_t dotVs[] = {0xFFFE0200, 0x0200001F, 0x80000000, 0x900F0000, 0x0200001F, 0x8000000A,
						0x900F0001, 0x05000051, 0xA00F0000, 0x3e800000, 0,			0,
						0,			0x05000051, 0xA00F0001, 0x3f000000, 0,			0,
						0,			0x05000051, 0xA00F0002, 0x3f400000, 0,			0,
						0,			0x02000001, 0xC00F0000, 0x90E40000, 0x02000001, 0xE00F0000,
						0x90E40001, 0x03000002, 0xE00F0001, 0x90E40001, 0xA0E40000, 0x03000002,
						0xE00F0002, 0x90E40001, 0xA0E40001, 0x03000002, 0xE00F0003, 0x90E40001,
						0xA0E40002, 0x0000FFFF};
	uint32_t ps[] = {0xFFFF0102,
					 66,
					 0xB00F0000u | (uint32_t)source,
					 (uint32_t)opcode,
					 0xB00F0000u | (uint32_t)destination,
					 0xB0E40000u | (uint32_t)source | ((uint32_t)modifier << 24),
					 1,
					 0x800F0000,
					 0xB0E40000u | (uint32_t)destination,
					 0x0000FFFF};
	const float ordinary[4] = {.5f, .5f, 0, 1};
	const float dotCoordinates[4] = {2, 1, -.5f, 9};
	const int isDot = opcode == 83 || opcode == 85;
	TestEffect code;
	TestEffect_Vertex vertices[3];
	MOJOSHADER_effect *data = NULL;
	TestEffect_BuildPrograms(&code, isDot ? dotVs : vs,
							 isDot ? SDL_arraysize(dotVs) : SDL_arraysize(vs), ps,
							 SDL_arraysize(ps), 1);
	FNA3D_CreateEffect(device, (uint8_t *)code.words, code.length, &effect, &data);
	Check(effect != NULL && data != NULL && data->error_count == 0, "texreg effect compiles");
	if (effect == NULL || data == NULL || data->error_count != 0)
	{
		if (data != NULL && data->error_count > 0)
			fprintf(stderr, "%s\n", data->errors[0].error);
		if (effect != NULL)
			FNA3D_AddDisposeEffect(device, effect);
		return 0;
	}
	TestEffect_FullscreenTriangle(vertices, isDot ? dotCoordinates : ordinary);
	vb = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_WRITEONLY, sizeof(vertices));
	Check(vb != NULL, "texreg vertex buffer exists");
	if (vb == NULL)
	{
		FNA3D_AddDisposeEffect(device, effect);
		return 0;
	}
	FNA3D_SetVertexBufferData(device, vb, 0, vertices, 3, sizeof(vertices[0]), sizeof(vertices[0]),
							  FNA3D_SETDATAOPTIONS_NONE);
	return 1;
}

/* Microsoft texm3x2depth: byte source gives 1/12, 191/252 with both _bx2,
 * 191/384 with PAD-only _bx2 and numerator row -X, or 8/63 with DEPTH-only
 * _bx2 and row -X. Zero denominator gives 1. Bracket each defined fraction
 * with depth clears; color is magenta or the distinct nonzero clear sentinel.
 * Rasterized depth is .9 so missing shader depth cannot pass a positive probe.
 * Out-of-range z/w is deliberately excluded: Microsoft defines no pixel oracle. */
static int depth_tokens_only;
static int TexDepthProgram(int source, int pad, int padmod, int depthmod, int zero)
{
	uint32_t vs[64] = {
		0xFFFE0200, 0x0200001F, 0x80000000, 0x900F0000,
		0x0200001F, 0x8000000A, 0x900F0001,
		0x05000051, 0xA00F0000, 0x3f800000, 0, 0, 0x41100000,
		0x05000051, 0xA00F0001, 0, 0x40800000, 0, 0x40e00000,
		0x02000001, 0xC00F0000, 0x90E40000
	};
	uint32_t ps[] = {
		0xFFFF0103, 81, 0xA00F0000, 0x3f800000, 0, 0x3f800000, 0x3f800000,
		66, 0xB00F0000u | (uint32_t)source,
		71, 0xB00F0000u | (uint32_t)pad, 0xB0E40000u | (uint32_t)source | ((uint32_t)padmod << 24),
		84, 0xB00F0000u | (uint32_t)(pad+1), 0xB0E40000u | (uint32_t)source | ((uint32_t)depthmod << 24),
		1, 0x800F0000, 0xA0E40000, 0x0000FFFF
	};
	const float uv[4] = {.5f, .5f, 0, 1};
	TestEffect code;
	TestEffect_Vertex vertices[3];
	MOJOSHADER_effect *data = NULL;
	size_t n = 22;
	int stage;
	if (padmod != depthmod) vs[9] = 0xbf800000; /* Reverse the numerator row for mixed modifiers. */
	if (zero) vs[16] = 0;
	for (stage = 0; stage < 4; stage++)
	{
		vs[n++] = 0x02000001; vs[n++] = 0xE00F0000u | (uint32_t)stage; vs[n++] = 0x90E40001;
	}
	vs[n++] = 0x02000001; vs[n++] = 0xE00F0000u | (uint32_t)pad; vs[n++] = 0xA0E40000;
	vs[n++] = 0x02000001; vs[n++] = 0xE00F0000u | (uint32_t)(pad+1); vs[n++] = 0xA0E40001;
	vs[n++] = 0x0000FFFF;
	TestEffect_BuildPrograms(&code, vs, n, ps, SDL_arraysize(ps), 1);
	if (depth_tokens_only)
	{
		static const char *profiles[] = {"spirv", "glsl120", "glsles3"};
		int profile, shader;
		for (profile = 0; profile < 3; profile++) for (shader = 0; shader < 2; shader++)
		{
			const MOJOSHADER_parseData *p = MOJOSHADER_parse(profiles[profile], "main",
				(const unsigned char *)(shader ? ps : vs), (unsigned int)(shader ? sizeof(ps) : n*4),
				NULL, 0, NULL, 0, NULL, NULL, NULL);
			Check(p != NULL && p->error_count == 0, "native depth witness tokens translate without a device");
			if (p != NULL && p->error_count) fprintf(stderr, "%s: %s\n", profiles[profile], p->errors[0].error);
			MOJOSHADER_freeParseData(p);
		}
		return 1;
	}
	FNA3D_CreateEffect(device, (uint8_t *)code.words, code.length, &effect, &data);
	Check(effect != NULL && data != NULL && data->error_count == 0, "TEXM3X2DEPTH effect compiles");
	if (effect == NULL || data == NULL || data->error_count != 0)
	{
		if (data != NULL && data->error_count > 0) fprintf(stderr, "%s\n", data->errors[0].error);
		if (effect != NULL) FNA3D_AddDisposeEffect(device, effect);
		return 0;
	}
	TestEffect_FullscreenTriangle(vertices, uv);
	for (stage = 0; stage < 3; stage++) vertices[stage].position[2] = .9f;
	vb = FNA3D_GenVertexBuffer(device, 0, FNA3D_BUFFERUSAGE_WRITEONLY, sizeof(vertices));
	Check(vb != NULL, "depth witness vertex buffer exists");
	if (vb == NULL) { FNA3D_AddDisposeEffect(device, effect); return 0; }
	FNA3D_SetVertexBufferData(device, vb, 0, vertices, 3, sizeof(vertices[0]), sizeof(vertices[0]), FNA3D_SETDATAOPTIONS_NONE);
	return 1;
}
static void TexDepthPixels(FNA3D_Texture *sourceTexture, FNA3D_SamplerState *sampler)
{
	static const uint8_t magenta[4] = {255, 0, 255, 255}, cleared[4] = {51, 102, 153, 204};
	/* Strict inequalities: 1/16 < 1/12 < 1/8; 31/64 < 191/384 < 33/64;
	 * 1/8 < 8/63 < 9/64; 3/4 < 191/252 < 13/16. No boundary rounding ties. */
	static const float low[4] = {1.0f/16, 31.0f/64, 1.0f/8, 3.0f/4};
	static const float high[4] = {1.0f/8, 33.0f/64, 9.0f/64, 13.0f/16};
	FNA3D_RenderTargetBinding targetBinding;
	FNA3D_Texture *target = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, TW, TH, 1, 1);
	FNA3D_Renderbuffer *depth = FNA3D_GenDepthStencilRenderbuffer(device, TW, TH, FNA3D_DEPTHFORMAT_D24S8, 0);
	FNA3D_Vec4 clear = {.2f, .4f, .6f, .8f};
	uint8_t pixels[TW * TH * 4];
	int pad, source, variant, probe, cases = 0;
	Check(target != NULL && depth != NULL, "depth witness color and depth attachments exist");
	if (target == NULL || depth == NULL) goto done;
	SDL_zero(targetBinding);
	targetBinding.type = FNA3D_RENDERTARGET_TYPE_2D;
	targetBinding.twod.width = TW; targetBinding.twod.height = TH;
	targetBinding.levelCount = 1; targetBinding.texture = target;
	FNA3D_SetRenderTargets(device, &targetBinding, 1, depth, FNA3D_DEPTHFORMAT_D24S8, 1);
	for (pad = 1; pad <= 2; pad++) for (source = 0; source < pad; source++)
	for (variant = 0; variant < 8; variant++)
	{
		int a = (variant & 1) ? 4 : 0, b = (variant & 2) ? 4 : 0, zero = variant >= 4;
		printf("texm3x2depth source=%d pad=%d pad_bx2=%d depth_bx2=%d zero=%d\n", source, pad, a != 0, b != 0, zero);
		if (!TexDepthProgram(source, pad, a, b, zero)) continue;
		for (probe = 0; probe < 2; probe++)
		{
			FNA3D_VertexElement elements[2] = {
				{0, FNA3D_VERTEXELEMENTFORMAT_VECTOR4, FNA3D_VERTEXELEMENTUSAGE_POSITION, 0},
				{16, FNA3D_VERTEXELEMENTFORMAT_VECTOR4, FNA3D_VERTEXELEMENTUSAGE_COLOR, 0}
			};
			FNA3D_VertexBufferBinding binding;
			MOJOSHADER_effectStateChanges changes;
			FNA3D_DepthStencilState state;
			const uint8_t *wanted = probe ? magenta : cleared;
			int i, same = 1;
			TestEffect_ApplyDrawStates(device, TW, TH, 0);
			SDL_zero(binding); SDL_zero(changes); SDL_zero(state);
			binding.vertexBuffer = vb; binding.vertexDeclaration.vertexStride = sizeof(TestEffect_Vertex);
			binding.vertexDeclaration.elementCount = 2; binding.vertexDeclaration.elements = elements;
			state.depthBufferEnable = state.depthBufferWriteEnable = 1;
			state.depthBufferFunction = zero && probe ? FNA3D_COMPAREFUNCTION_GREATER : FNA3D_COMPAREFUNCTION_LESS;
			FNA3D_SetDepthStencilState(device, &state);
			FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET | FNA3D_CLEAROPTIONS_DEPTHBUFFER, &clear,
				zero ? 15.0f/16 : probe ? high[variant] : low[variant], 0);
			FNA3D_ApplyEffect(device, effect, 0, &changes);
			FNA3D_VerifySampler(device, source, sourceTexture, sampler);
			FNA3D_ApplyVertexBufferBindings(device, &binding, 1, 1, 0);
			FNA3D_DrawPrimitives(device, FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 1);
			FNA3D_GetTextureData2D(device, target, 0, 0, TW, TH, 0, pixels, sizeof(pixels));
			for (i = 0; i < TW * TH; i++) if (SDL_memcmp(pixels+i*4, wanted, 4)) { same = 0; break; }
			if (!same) fprintf(stderr, "depth probe=%d pixel%d=(%u,%u,%u,%u), expected=(%u,%u,%u,%u)\n", probe, i,
				pixels[i*4], pixels[i*4+1], pixels[i*4+2], pixels[i*4+3], wanted[0], wanted[1], wanted[2], wanted[3]);
			Check(same, "every TEXM3X2DEPTH pixel matches the independent depth comparison");
			cases++;
		}
		EndProgram();
	}
	Check(cases == 48, "all 48 depth comparison draws executed");
	FNA3D_SetRenderTargets(device, NULL, 0, NULL, FNA3D_DEPTHFORMAT_NONE, 0);
 done:
	if (target != NULL) FNA3D_AddDisposeTexture(device, target);
	if (depth != NULL) FNA3D_AddDisposeRenderbuffer(device, depth);
}

int main(int argc, char **argv)
{
	static uint8_t first[] = {32, 96, 160, 224};
	static const uint8_t expected[][4] = {{160, 40, 91, 255}, {80, 120, 49, 255}, {40, 80, 14, 255},
										  {40, 40, 7, 255},	  {80, 40, 35, 255},  {40, 40, 7, 255},
										  {0, 0, 0, 0},		  {0, 0, 0, 0}};
	static const int opcodes[] = {69, 70, 82, 82, 83, 83, 85, 85};
	static const int modifiers[] = {0, 0, 0, 4, 0, 4, 0, 4};
	uint8_t grid[4 * 4 * 4], pixels[TW * TH * 4];
	FNA3D_Texture *sourceTexture, *lookupTexture;
	FNA3D_SamplerState sampler;
	FNA3D_Vec4 clear = {.2f, .4f, .6f, .8f};
	int x, y, variant, source, destination, stage, cases = 0;
	if (argc == 2 && SDL_strcmp(argv[1], "--depth-tokens-only") == 0)
	{
		int pad, src;
		depth_tokens_only = 1;
		for (pad = 1; pad <= 2; pad++) for (src = 0; src < pad; src++)
		for (variant = 0; variant < 8; variant++)
			TexDepthProgram(src, pad, (variant&1) ? 4 : 0, (variant&2) ? 4 : 0, variant >= 4);
		printf("native depth token controls: %d failure(s)\n", failures);
		return failures ? 1 : 0;
	}
	if (!Start())
		return 1;
	TestEffect_ApplyDrawStates(device, TW, TH, 1);
	for (y = 0; y < 4; y++)
		for (x = 0; x < 4; x++)
		{
			uint8_t *p = grid + (y * 4 + x) * 4;
			p[0] = (uint8_t)((x + 1) * 40);
			p[1] = (uint8_t)((y + 1) * 40);
			p[2] = (uint8_t)((x * 4 + y + 1) * 7);
			p[3] = 255;
		}
	sourceTexture = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, 1, 1, 1, 0);
	lookupTexture = FNA3D_CreateTexture2D(device, FNA3D_SURFACEFORMAT_COLOR, 4, 4, 1, 0);
	Check(sourceTexture != NULL && lookupTexture != NULL, "both test textures exist");
	if (sourceTexture == NULL || lookupTexture == NULL)
	{
		if (sourceTexture != NULL)
			FNA3D_AddDisposeTexture(device, sourceTexture);
		if (lookupTexture != NULL)
			FNA3D_AddDisposeTexture(device, lookupTexture);
		return Finish();
	}
	FNA3D_SetTextureData2D(device, sourceTexture, 0, 0, 1, 1, 0, first, sizeof(first));
	FNA3D_SetTextureData2D(device, lookupTexture, 0, 0, 4, 4, 0, grid, sizeof(grid));
	SDL_zero(sampler);
	sampler.filter = FNA3D_TEXTUREFILTER_POINT;
	sampler.addressU = sampler.addressV = sampler.addressW = FNA3D_TEXTUREADDRESSMODE_CLAMP;
	sampler.maxAnisotropy = 1;
	for (variant = 0; variant < 8; variant++)
		for (destination = 1; destination <= 3; destination++)
			for (source = 0; source < destination; source++)
			{
				int same = 1, i;
				uint8_t dotColor[4];
				const uint8_t *wanted = expected[variant];
				if (variant == 6)
				{
					SDL_memset(dotColor, 80 + 8 * destination, 4);
					wanted = dotColor;
				}
				printf("texreg opcode=%d src=%d dst=%d signed=%d\n", opcodes[variant], source,
					   destination, modifiers[variant] != 0);
				if (!TexRegProgram(opcodes[variant], source, destination, modifiers[variant]))
					continue;
				FNA3D_Clear(device, FNA3D_CLEAROPTIONS_TARGET, &clear, 1, 0);
				for (stage = 0; stage < 4; stage++)
					FNA3D_VerifySampler(device, stage, sourceTexture, &sampler);
				FNA3D_VerifySampler(device, destination, lookupTexture, &sampler);
				Draw(sourceTexture, 0, 0, TW);
				FNA3D_ReadBackbuffer(device, 0, 0, TW, TH, pixels, sizeof(pixels));
				for (i = 0; i < TW * TH; i++)
					if (SDL_memcmp(pixels + i * 4, wanted, 4))
					{
						same = 0;
						break;
					}
				if (!same)
					fprintf(stderr, "pixel%d=(%u,%u,%u,%u), expected(%u,%u,%u,%u)\n", i,
							pixels[i * 4], pixels[i * 4 + 1], pixels[i * 4 + 2], pixels[i * 4 + 3],
							wanted[0], wanted[1], wanted[2], wanted[3]);
				Check(same, "every texture-instruction pixel matches the literal expected result");
				EndProgram();
				cases++;
			}
	Check(cases == 48, "all opcode/register/signed variants rendered");
	TexDepthPixels(sourceTexture, &sampler);
	FNA3D_AddDisposeTexture(device, sourceTexture);
	FNA3D_AddDisposeTexture(device, lookupTexture);
	return Finish();
}
