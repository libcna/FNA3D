/* libcna fork regression test: dumpspirv links a Depth vertex element to the DEPTH input.
 *
 * dumpspirv carries its own copy of FNA3D_Driver.h's VertexAttribUsage(), which had no case for
 * VertexElementUsage.Depth: it asserted "Unrecognized VertexElementUsage!" and linked the element
 * as POSITION (2026-10-26 dependency audit A3a2 A-04). Linking patches an attribute's SPIR-V type
 * for the integer formats, so a Byte4 Depth element changes the DEPTH input -- or, linked as
 * POSITION, the position.
 *
 * Given the folder dumpspirv wrote for trace_record.c's trace, this links the depth effect's
 * shaders itself -- through the MojoShader in libFNA3D that dumpspirv uses, named and parsed as
 * dumpspirv parses an effect's shaders -- once as {POSITION Vector4, DEPTH Byte4} and once as
 * the defect did, {POSITION Vector4, POSITION Byte4}. The two must differ, the first must be in
 * the folder byte for byte, under the name dumpspirv gives it (its CRC-32), and the second must
 * not be there.
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

/* Links the depth effect's vertex shader with the given second attribute; returns its bytes as
 * dumpspirv writes them (an SDL_malloc'd copy) and their length.
 */
static uint8_t *LinkedVertexShader(MOJOSHADER_usage secondUsage, size_t *length)
{
	uint32_t vs[16], ps[16];
	const size_t vsCount = TestEffect_VertexShader(vs, MOJOSHADER_USAGE_DEPTH);
	const size_t psCount = TestEffect_PixelShader(ps);
	MOJOSHADER_vertexAttribute attributes[2];
	const MOJOSHADER_parseData *vertex, *pixel;
	uint8_t *result = NULL;
	int patchLength;

	/* An effect's shader objects are compiled as "ShaderFunction<object index>"; the test effect
	 * holds its vertex shader in object 1 and its pixel shader in object 2.
	 */
	vertex = MOJOSHADER_parse("spirv", "ShaderFunction1", (const unsigned char*) vs,
				  (unsigned int) (vsCount * 4), NULL, 0, NULL, 0, NULL, NULL, NULL);
	pixel = MOJOSHADER_parse("spirv", "ShaderFunction2", (const unsigned char*) ps,
				 (unsigned int) (psCount * 4), NULL, 0, NULL, 0, NULL, NULL, NULL);
	if (	vertex == NULL || pixel == NULL ||
		vertex->error_count > 0 || pixel->error_count > 0	)
	{
		fprintf(stderr, "FAILED: the test shaders did not parse: %s\n",
			(vertex != NULL && vertex->error_count > 0) ? vertex->errors[0].error :
			(pixel != NULL && pixel->error_count > 0) ? pixel->errors[0].error :
			"no parse data");
		return NULL;
	}

	attributes[0].usage = MOJOSHADER_USAGE_POSITION;
	attributes[0].vertexElementFormat = MOJOSHADER_VERTEXELEMENTFORMAT_VECTOR4;
	attributes[0].usageIndex = 0;
	attributes[1].usage = secondUsage;
	attributes[1].vertexElementFormat = MOJOSHADER_VERTEXELEMENTFORMAT_BYTE4;
	attributes[1].usageIndex = 0;
	patchLength = MOJOSHADER_linkSPIRVShaders(vertex, pixel, attributes, 2);

	*length = (size_t) (vertex->output_len - patchLength);
	result = (uint8_t*) SDL_malloc(*length);
	memcpy(result, vertex->output, *length);
	MOJOSHADER_freeParseData(vertex);
	MOJOSHADER_freeParseData(pixel);
	return result;
}

int main(int argc, char **argv)
{
	size_t expectedLength = 0, defectLength = 0, foundLength = 0;
	uint8_t *expected, *defect, *found;
	uint32_t expectedCrc, defectCrc;
	char path[1024];
	int failures = 0;

	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <the folder dumpspirv wrote>\n", argv[0]);
		return 2;
	}
	expected = LinkedVertexShader(MOJOSHADER_USAGE_DEPTH, &expectedLength);
	defect = LinkedVertexShader(MOJOSHADER_USAGE_POSITION, &defectLength);
	if (expected == NULL || defect == NULL)
	{
		return 1;
	}
	expectedCrc = SDL_crc32(0, expected, expectedLength);
	defectCrc = SDL_crc32(0, defect, defectLength);
	if (expectedLength == defectLength && memcmp(expected, defect, expectedLength) == 0)
	{
		fprintf(stderr, "FAILED: linking Depth as DEPTH and as POSITION gives the same "
			"shader; this check cannot tell them apart\n");
		return 1;
	}

	SDL_snprintf(path, sizeof(path), "%s/%x.vert.spv", argv[1], expectedCrc);
	found = (uint8_t*) SDL_LoadFile(path, &foundLength);
	if (found == NULL)
	{
		fprintf(stderr, "FAILED: dumpspirv wrote no %s, the vertex shader linked with "
			"the Depth element as DEPTH\n", path);
		failures += 1;
	}
	else if (foundLength != expectedLength || memcmp(found, expected, expectedLength) != 0)
	{
		fprintf(stderr, "FAILED: %s differs from the vertex shader linked with the Depth "
			"element as DEPTH\n", path);
		failures += 1;
	}
	else
	{
		printf("dumpspirv: %s is the vertex shader linked with Depth as DEPTH\n", path);
	}
	SDL_free(found);

	SDL_snprintf(path, sizeof(path), "%s/%x.vert.spv", argv[1], defectCrc);
	if (SDL_GetPathInfo(path, NULL))
	{
		fprintf(stderr, "FAILED: dumpspirv wrote %s, the vertex shader linked with the "
			"Depth element as POSITION\n", path);
		failures += 1;
	}

	SDL_free(expected);
	SDL_free(defect);
	return (failures == 0) ? 0 : 1;
}
