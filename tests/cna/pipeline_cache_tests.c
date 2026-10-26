/* libcna fork regression test: a blend state's pipeline-cache key tells every state apart.
 *
 * GetPackedBlendState packed its halves as int32_t: an alphaDestinationBlend of DestinationAlpha
 * or later, or a blend factor alpha of 128 or more (XNA's default factor is white), shifted into
 * the sign bit, and the sign-extended value overwrote the other half of the 64-bit key -- the
 * blend functions, or the multisample mask. Two states differing only there got one key, so the
 * SDL_GPU and D3D11 drivers reused a pipeline or blend state object made for the other one.
 * No device and no display: FNA3D_PipelineCache.c is compiled into the test.
 *
 * This file is part of the libcna fork of FNA3D; see tests/cna/README.md.
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "FNA3D_PipelineCache.h"

static int failures = 0;
static int checks = 0;

static int SameKey(PackedState x, PackedState y)
{
	return x.a == y.a && x.b == y.b;
}

static FNA3D_BlendState Opaque(void)
{
	FNA3D_BlendState state;
	SDL_memset(&state, '\0', sizeof(state));
	state.colorSourceBlend = FNA3D_BLEND_ONE;
	state.colorDestinationBlend = FNA3D_BLEND_ZERO;
	state.colorBlendFunction = FNA3D_BLENDFUNCTION_ADD;
	state.alphaSourceBlend = FNA3D_BLEND_ONE;
	state.alphaDestinationBlend = FNA3D_BLEND_ZERO;
	state.alphaBlendFunction = FNA3D_BLENDFUNCTION_ADD;
	state.colorWriteEnable = FNA3D_COLORWRITECHANNELS_ALL;
	state.colorWriteEnable1 = FNA3D_COLORWRITECHANNELS_ALL;
	state.colorWriteEnable2 = FNA3D_COLORWRITECHANNELS_ALL;
	state.colorWriteEnable3 = FNA3D_COLORWRITECHANNELS_ALL;
	state.blendFactor.r = 255;
	state.blendFactor.g = 255;
	state.blendFactor.b = 255;
	state.blendFactor.a = 255;
	state.multiSampleMask = -1;
	return state;
}

static void Expect(int condition, const char *what)
{
	checks += 1;
	if (!condition)
	{
		failures += 1;
		fprintf(stderr, "FAILED: %s\n", what);
	}
}

int main(int argc, char **argv)
{
	FNA3D_BlendState x, y;
	FNA3D_Blend blend;

	(void) argc;
	(void) argv;

	x = Opaque();
	y = Opaque();
	Expect(SameKey(GetPackedBlendState(x), GetPackedBlendState(y)), "equal states, equal keys");

	/* Every alpha destination blend: Add and Subtract are different states. */
	for (blend = FNA3D_BLEND_ONE; blend <= FNA3D_BLEND_SOURCEALPHASATURATION; blend += 1)
	{
		char what[96];
		x = Opaque();
		y = Opaque();
		x.alphaDestinationBlend = blend;
		y.alphaDestinationBlend = blend;
		y.colorBlendFunction = FNA3D_BLENDFUNCTION_SUBTRACT;
		SDL_snprintf(what, sizeof(what), "alphaDestinationBlend %d: colour Add and Subtract differ", (int) blend);
		Expect(!SameKey(GetPackedBlendState(x), GetPackedBlendState(y)), what);
		y = x;
		y.alphaBlendFunction = FNA3D_BLENDFUNCTION_REVERSESUBTRACT;
		SDL_snprintf(what, sizeof(what), "alphaDestinationBlend %d: alpha Add and ReverseSubtract differ", (int) blend);
		Expect(!SameKey(GetPackedBlendState(x), GetPackedBlendState(y)), what);
	}

	/* The multisample mask, under XNA's default white blend factor and under a dark one. */
	x = Opaque();
	y = Opaque();
	y.multiSampleMask = 0x0000000F;
	Expect(!SameKey(GetPackedBlendState(x), GetPackedBlendState(y)),
		"white blend factor: multisample masks -1 and 0xF differ");
	x.blendFactor.a = 0;
	y.blendFactor.a = 0;
	Expect(!SameKey(GetPackedBlendState(x), GetPackedBlendState(y)),
		"transparent blend factor: multisample masks -1 and 0xF differ");

	/* The blend factor's alpha itself. */
	x = Opaque();
	y = Opaque();
	y.blendFactor.a = 128;
	Expect(!SameKey(GetPackedBlendState(x), GetPackedBlendState(y)),
		"blend factor alpha 255 and 128 differ");

	printf("pipeline cache keys: %d check(s), %d failed\n", checks, failures);
	return (failures == 0) ? 0 : 1;
}
