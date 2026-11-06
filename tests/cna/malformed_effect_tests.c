/* SPDX-License-Identifier: Zlib */
/* Investigation for FNA3D-7: run on an explicit driver with the provenance-tracked
 * BasicEffect.fxb argument. Failure attribution comes from a sanitizer/backtrace,
 * never from the existence of this reproducer.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FNA3D.h"
#include "mojoshader.h"
int main(int argc, char **argv)
{
	SDL_Window *window;
	FNA3D_Device *device;
	FNA3D_PresentationParameters p;
	FNA3D_Effect *effect;
	MOJOSHADER_effect *data;
	FILE *file;
	long length;
	uint8_t *bytes;
	int pass, errors = 0, found, i;
	const char *driver = SDL_getenv("FNA3D_FORCE_DRIVER");
	if (argc != 2 || driver == NULL) return 1;
	file = fopen(argv[1], "rb");
	if (!file) return 1;
	fseek(file, 0, SEEK_END); length = ftell(file); rewind(file);
	if (length <= 25018 || length > 1024 * 1024) { fclose(file); return 1; }
	bytes = (uint8_t*) malloc((size_t) length);
	if (!bytes || fread(bytes, 1, (size_t) length, file) != (size_t) length) return 1;
	fclose(file);
	if (bytes[25018] != 0x6f) { fprintf(stderr, "fixture identity mismatch at 25018\n"); return 1; }
	if (!SDL_Init(SDL_INIT_VIDEO)) return 1;
	window = SDL_CreateWindow("malformed Effect investigation", 64, 64,
		FNA3D_PrepareWindowAttributes() | SDL_WINDOW_HIDDEN);
	if (!window) return 1;
	SDL_zero(p); p.backBufferWidth = p.backBufferHeight = 64;
	p.backBufferFormat = FNA3D_SURFACEFORMAT_COLOR; p.deviceWindowHandle = window;
	device = FNA3D_CreateDevice(&p, 1);
	if (!device) return 1;
	for (pass = 0; pass < 2; pass += 1)
	{
		effect = NULL; data = NULL;
		if (pass) bytes[25018] ^= 0x10;
		fprintf(stderr, "%s: %s: before CreateEffect\n", driver, pass ? "missing symbol" : "valid control");
		fflush(stderr);
		FNA3D_CreateEffect(device, bytes, (uint32_t) length, &effect, &data);
		fprintf(stderr, "after CreateEffect: effect=%p data=%p errors=%d\n",
			(void*) effect, (void*) data, data ? data->error_count : -1);
		fflush(stderr);
		if (!effect || !data) errors += 1;
		else if (!pass) { if (data->error_count != 0) errors += 1; }
		else
		{
			found = 0;
			for (i = 0; i < data->error_count; i += 1)
				if (strstr(data->errors[i].error, "Shader parameter not found in effect.")) found = 1;
			if (!found) errors += 1;
		}
		fprintf(stderr, "before AddDisposeEffect\n"); fflush(stderr);
		if (effect) FNA3D_AddDisposeEffect(device, effect);
		fprintf(stderr, "after AddDisposeEffect\n"); fflush(stderr);
	}
	FNA3D_DestroyDevice(device); SDL_DestroyWindow(window); SDL_Quit(); free(bytes);
	return errors ? 1 : 0;
}
