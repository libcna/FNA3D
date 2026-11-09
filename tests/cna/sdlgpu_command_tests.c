/* SPDX-License-Identifier: Zlib */
/* Window-free SDL GPU boundary regressions. The real driver executes against
 * captured submission/upload/sampler calls, not a second implementation.
 * Queue-order proof here complements texture_order_tests.c's owner-run pixels.
 */
#include <SDL3/SDL.h>
#include <stdio.h>

static SDL_GPUTexture * SDLCALL TestTexture(SDL_GPUDevice*, const SDL_GPUTextureCreateInfo*);
static bool SDLCALL TestSampleSupport(SDL_GPUDevice*, SDL_GPUTextureFormat, SDL_GPUSampleCount);
static SDL_GPUSampler * SDLCALL TestSampler(SDL_GPUDevice*, const SDL_GPUSamplerCreateInfo*);
static SDL_GPUCommandBuffer * SDLCALL TestAcquire(SDL_GPUDevice*);
static bool SDLCALL TestSubmit(SDL_GPUCommandBuffer*);
static SDL_GPUCopyPass * SDLCALL TestCopy(SDL_GPUCommandBuffer*);
static void SDLCALL TestEndCopy(SDL_GPUCopyPass*);
static void SDLCALL TestEndRender(SDL_GPURenderPass*);
static SDL_GPURenderPass * SDLCALL TestRender(SDL_GPUCommandBuffer*, const SDL_GPUColorTargetInfo*, Uint32,
	const SDL_GPUDepthStencilTargetInfo*);
static void SDLCALL TestViewport(SDL_GPURenderPass*, const SDL_GPUViewport*);
static void SDLCALL TestScissor(SDL_GPURenderPass*, const SDL_Rect*);
static void * SDLCALL TestMap(SDL_GPUDevice*, SDL_GPUTransferBuffer*, bool);
static void SDLCALL TestUnmap(SDL_GPUDevice*, SDL_GPUTransferBuffer*);
static void SDLCALL TestUpload(SDL_GPUCopyPass*, const SDL_GPUTextureTransferInfo*, const SDL_GPUTextureRegion*, bool);
static bool SDLCALL TestAcquireSwapchain(SDL_GPUCommandBuffer*, SDL_Window*, SDL_GPUTexture**, Uint32*, Uint32*);
static void SDLCALL TestBlit(SDL_GPUCommandBuffer*, const SDL_GPUBlitInfo*);
static bool SDLCALL TestWindowPixels(SDL_Window*, int*, int*);
#define SDLGPUDriver CNACommand_SDLGPUDriver
#define SDL_WaitAndAcquireGPUSwapchainTexture TestAcquireSwapchain
#define SDL_BlitGPUTexture TestBlit
#define SDL_GetWindowSizeInPixels TestWindowPixels
#define SDL_CreateGPUSampler TestSampler
#define SDL_CreateGPUTexture TestTexture
#define SDL_GPUTextureSupportsSampleCount TestSampleSupport
#define SDL_AcquireGPUCommandBuffer TestAcquire
#define SDL_SubmitGPUCommandBuffer TestSubmit
#define SDL_BeginGPUCopyPass TestCopy
#define SDL_EndGPUCopyPass TestEndCopy
#define SDL_EndGPURenderPass TestEndRender
#define SDL_BeginGPURenderPass TestRender
#define SDL_SetGPUViewport TestViewport
#define SDL_SetGPUScissor TestScissor
#define SDL_MapGPUTransferBuffer TestMap
#define SDL_UnmapGPUTransferBuffer TestUnmap
#define SDL_UploadToGPUTexture TestUpload
#include "FNA3D_Driver_SDL.c"

static int checks, failures, event, submitted, priorRenderAt, uploadedAt, clearAt, acquired;
static int samplerCalls, renderPassCalls;
static bool cycled;
static SDL_GPUCommandBuffer *priorRender;
static SDL_GPUSamplerCreateInfo samplerInfo;
static SDL_GPUColorTargetInfo seenColor;
static SDL_GPUDepthStencilTargetInfo seenDepth;
static SDL_GPUTextureRegion uploadedRegion;
static char commandTokens[64], samplerToken, copyToken, renderToken, textureToken, transferToken;
static uint8_t transferBytes[256];
static const char *where;
#define CHECK(c, message) do { checks++; if (!(c)) { failures++; \
	fprintf(stderr, "FAILED %s: %s\n", where, message); } } while (0)
static int loggedErrors, swapchainAcquires, blits;
static SDL_GPUBlitInfo seenBlit;
static char windowToken, swapchainToken;
void FNA3D_LogWarn(const char *fmt, ...) { (void)fmt; }
void FNA3D_LogError(const char *fmt, ...) { (void)fmt; loggedErrors++; }
static bool SDLCALL TestAcquireSwapchain(SDL_GPUCommandBuffer *command, SDL_Window *window,
	SDL_GPUTexture **texture, Uint32 *w, Uint32 *h)
{
	(void)command; (void)window; swapchainAcquires++;
	*texture = (SDL_GPUTexture*)&swapchainToken; *w = 128; *h = 96;
	return true;
}
static void SDLCALL TestBlit(SDL_GPUCommandBuffer *command, const SDL_GPUBlitInfo *info)
{
	(void)command; blits++; seenBlit = *info;
}
static bool SDLCALL TestWindowPixels(SDL_Window *window, int *w, int *h)
{
	(void)window; *w = 128; *h = 96;
	return true;
}
static unsigned supportedSamples = 7;
static SDL_GPUTexture * SDLCALL TestTexture(SDL_GPUDevice *device, const SDL_GPUTextureCreateInfo *info)
{
    (void)device; (void)info;
    return (SDL_GPUTexture*)&textureToken;
}
static bool SDLCALL TestSampleSupport(SDL_GPUDevice *device, SDL_GPUTextureFormat format, SDL_GPUSampleCount count)
{
    (void)device;
    CHECK(format == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, "sample query preserves the requested color format");
    return count == SDL_GPU_SAMPLECOUNT_2 ? (supportedSamples & 1) != 0 :
           count == SDL_GPU_SAMPLECOUNT_4 ? (supportedSamples & 2) != 0 :
           count == SDL_GPU_SAMPLECOUNT_8 ? (supportedSamples & 4) != 0 : true;
}
static SDL_GPUSampler * SDLCALL TestSampler(SDL_GPUDevice *device, const SDL_GPUSamplerCreateInfo *info)
{
	(void)device; samplerInfo = *info; samplerCalls++; return (SDL_GPUSampler*)&samplerToken;
}
static SDL_GPUCommandBuffer * SDLCALL TestAcquire(SDL_GPUDevice *device)
{
	(void)device; return (SDL_GPUCommandBuffer*)&commandTokens[acquired++];
}
static bool SDLCALL TestSubmit(SDL_GPUCommandBuffer *command)
{
	event++; submitted++;
	if (command == priorRender) priorRenderAt = event;
	return true;
}
static SDL_GPUCopyPass * SDLCALL TestCopy(SDL_GPUCommandBuffer *command)
{
	(void)command; return (SDL_GPUCopyPass*)&copyToken;
}
static void SDLCALL TestEndCopy(SDL_GPUCopyPass *pass) { (void)pass; }
static void SDLCALL TestEndRender(SDL_GPURenderPass *pass) { (void)pass; }
static SDL_GPURenderPass * SDLCALL TestRender(SDL_GPUCommandBuffer *command,
	const SDL_GPUColorTargetInfo *colors, Uint32 count, const SDL_GPUDepthStencilTargetInfo *depth)
{
	(void)command; (void)depth; renderPassCalls++;
	CHECK(count == 1, "a deferred render pass has a real attachment");
	if (count == 1) seenColor = colors[0];
	if (depth != NULL) seenDepth = *depth;
	if (count == 1 && colors[0].load_op == SDL_GPU_LOADOP_CLEAR) clearAt = ++event;
	return (SDL_GPURenderPass*)&renderToken;
}
static void SDLCALL TestViewport(SDL_GPURenderPass *pass, const SDL_GPUViewport *viewport)
{
	(void)pass;
	CHECK(viewport->w > 0 && viewport->h > 0, "a submitted viewport has positive dimensions");
}
static void SDLCALL TestScissor(SDL_GPURenderPass *pass, const SDL_Rect *rect) { (void)pass; (void)rect; }
static void * SDLCALL TestMap(SDL_GPUDevice *device, SDL_GPUTransferBuffer *buffer, bool cycle)
{
	(void)device; (void)buffer; (void)cycle; return transferBytes;
}
static void SDLCALL TestUnmap(SDL_GPUDevice *device, SDL_GPUTransferBuffer *buffer)
{
	(void)device; (void)buffer;
}
static void SDLCALL TestUpload(SDL_GPUCopyPass *pass, const SDL_GPUTextureTransferInfo *source,
	const SDL_GPUTextureRegion *destination, bool cycle)
{
	(void)pass; (void)source; uploadedAt = ++event; cycled = cycle; uploadedRegion = *destination;
}
static void Mips(void)
{
	/* XNA SamplerState.Apply IL_0342..0355 forwards MaxMipLevel as uint32.
	 * A uint32-sized texture has at most 32 stored levels (indices 0..31).
	 * Any unsigned request >=32 must exclude every finer stored level and
	 * remain an ordered [min_lod,max_lod] SDL sampler range. No GPU is used.
	 */
	static const int requests[] = {0, 1, 2, -1, 99, 0};
	size_t i;
	where = "unsigned sampler mip clamp";
	for (i = 0; i < SDL_arraysize(requests); i++)
	{
		SDLGPU_Renderer r;
		FNA3D_SamplerState state;
		SDL_GPUSampler *sampler;
		SDL_zero(r); SDL_zero(state); samplerCalls = 0;
		state.maxMipLevel = requests[i];
		sampler = SDLGPU_INTERNAL_FetchSamplerState(&r, &state);
		CHECK(sampler != NULL && samplerCalls == 1, "the real cache miss creates a sampler");
		if (requests[i] >= 0 && requests[i] < 32)
			CHECK(samplerInfo.min_lod == (float)requests[i], "a valid mip index reaches SDL unchanged");
		else {
			if (samplerInfo.min_lod < 31) fprintf(stderr, "request=%d SDL min_lod=%g max_lod=%g\n", requests[i], samplerInfo.min_lod, samplerInfo.max_lod);
			CHECK(samplerInfo.min_lod >= 31, "an unsigned large mip excludes finer stored levels");
		}
		CHECK(samplerInfo.min_lod <= samplerInfo.max_lod, "the sampler range is valid");
		CHECK(SamplerStateHashArray_Fetch(&r.samplerStateArray, GetPackedSamplerState(state)) == sampler,
			"created state is retained by the native cache");
		CHECK(SDLGPU_INTERNAL_FetchSamplerState(&r, &state) == sampler && samplerCalls == 1,
			"same state reuses its native sampler");
		SDL_free(r.samplerStateArray.elements);
	}
}
static void Ordering(void)
{
	/* Ordered native SetData/draw contract: earlier uploads U0 then rendering
	 * R0 must be submitted BEFORE recording U1. A clear-only render pass also
	 * has to be materialized before its submission. Initial dummy uploads
	 * before attachments must not flush; after attachments exist they must not
	 * invent an empty render pass before the caller supplies a viewport.
	 */
	int kind;
	where = "upload API order";
	for (kind = 0; kind < 7; kind++)
	{
		SDLGPU_Renderer r;
		SDLGPU_TextureHandle texture, color;
		SDLGPU_TextureHandle *bound[4];
		uint8_t bytes[256] = {0};
		SDL_zero(r); SDL_zero(texture); SDL_zero(color);
		event = submitted = priorRenderAt = uploadedAt = clearAt = renderPassCalls = 0; acquired = 2;
		r.commandLock = SDL_CreateMutex();
		r.uploadCommandBuffer = (SDL_GPUCommandBuffer*)&commandTokens[0];
		r.renderCommandBuffer = priorRender = (SDL_GPUCommandBuffer*)&commandTokens[1];
		r.copyPass = (SDL_GPUCopyPass*)&copyToken;
		r.renderPass = (SDL_GPURenderPass*)&renderToken;
		r.textureUploadBuffer = (SDL_GPUTransferBuffer*)&transferToken;
		r.nextRenderPassColorAttachmentCount = kind == 5 ? 0 : 1;
		r.nextRenderPassColorAttachments[0] = &color;
		r.boundRenderTargets = bound; r.boundRenderTargetCapacity = 4;
		r.viewport.w = r.viewport.h = 4;
		if (kind == 6)
		{
			r.renderPass = NULL; r.needNewRenderPass = 1;
			r.viewport.w = r.viewport.h = 0;
		}
		texture.texture = color.texture = (SDL_GPUTexture*)&textureToken;
		texture.createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		texture.createInfo.width = texture.createInfo.height = 4;
		texture.createInfo.layer_count_or_depth = kind == 3 ? 4 : kind == 2 ? 6 : 1;
		texture.createInfo.num_levels = kind == 1 ? 3 : 1;
		if (kind == 4) { r.renderPass = NULL; r.needNewRenderPass = 1; r.shouldClearColorOnBeginPass = 1; }
		if (kind == 2) SDLGPU_SetTextureDataCube((FNA3D_Renderer*)&r, (FNA3D_Texture*)&texture,
			0, 0, 4, 4, FNA3D_CUBEMAPFACE_NEGATIVEZ, 0, bytes, 64);
		else if (kind == 3) SDLGPU_SetTextureData3D((FNA3D_Renderer*)&r, (FNA3D_Texture*)&texture,
			0, 0, 0, 4, 4, 4, 0, bytes, 256);
		else SDLGPU_SetTextureData2D((FNA3D_Renderer*)&r, (FNA3D_Texture*)&texture,
			0, 0, 4, 4, 0, bytes, 64);
		if (kind == 5) CHECK(submitted == 0, "startup uploads do not flush an uninitialized renderer");
		else
		{
			CHECK(submitted == 2, "both earlier upload and render command buffers are submitted");
			CHECK(priorRenderAt > 0 && priorRenderAt < uploadedAt, "prior rendering precedes the new upload");
			CHECK(r.needVertexBufferBind && r.needVertexSamplerBind && r.needFragmentSamplerBind,
				"the split invalidates bindings for the next render command buffer");
		}
		if (kind == 6) CHECK(renderPassCalls == 0, "uploads before the first viewport do not invent an empty render pass");
		if (kind == 4) CHECK(clearAt > 0 && clearAt < priorRenderAt, "clear-only work is not lost at the split");
		CHECK(cycled == (kind != 1 && kind != 2 && kind != 3), "only complete single-level 2D updates cycle");
		CHECK(uploadedRegion.texture == texture.texture && uploadedRegion.w == 4 && uploadedRegion.h == 4,
			"upload destination and extent are preserved");
		if (kind == 2) CHECK(uploadedRegion.layer == FNA3D_CUBEMAPFACE_NEGATIVEZ, "the requested cube face is retained");
		if (kind == 3) CHECK(uploadedRegion.d == 4, "the volume extent is retained");
		SDL_DestroyMutex(r.commandLock);
	}
}
static void DeferredPasses(void)
{
	/* EndRenderPass materializes exactly pending clears, including depth-only
	 * and stencil-only ones. With no draw or clear it must record nothing. */
	int mask;
	where = "deferred render pass";
	for (mask = 0; mask < 8; mask++)
	{
		SDLGPU_Renderer r;
		SDLGPU_TextureHandle color, depth;
		SDLGPU_TextureHandle *bound[4];
		SDL_zero(r); SDL_zero(color); SDL_zero(depth);
		SDL_zero(seenColor); SDL_zero(seenDepth);
		r.needNewRenderPass = 1;
		r.shouldClearColorOnBeginPass = (mask & 1) != 0;
		r.shouldClearDepthOnBeginPass = (mask & 2) != 0;
		r.shouldClearStencilOnBeginPass = (mask & 4) != 0;
		r.nextRenderPassColorAttachmentCount = 1;
		r.nextRenderPassColorAttachments[0] = &color;
		r.nextRenderPassDepthStencilAttachment = &depth;
		r.boundRenderTargets = bound; r.boundRenderTargetCapacity = 4;
		r.viewport.w = r.viewport.h = 4;
		color.texture = depth.texture = (SDL_GPUTexture*)&textureToken;
		color.createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		depth.createInfo.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
		color.createInfo.sample_count = depth.createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
		r.clearColorValue.r = 0.25f; r.clearDepthValue = 0.75f; r.clearStencilValue = 37;
		renderPassCalls = 0;
		SDLGPU_INTERNAL_EndRenderPass(&r);
		CHECK(renderPassCalls == (mask != 0), "only a pending clear starts a render pass");
		CHECK(r.renderPass == NULL && r.needNewRenderPass, "the materialized pass is closed");
		CHECK(!r.shouldClearColorOnBeginPass && !r.shouldClearDepthOnBeginPass &&
			!r.shouldClearStencilOnBeginPass, "every pending clear was consumed");
		if (mask != 0)
		{
			CHECK(seenColor.load_op == ((mask & 1) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD), "color clear flag reaches SDL");
			CHECK(seenDepth.load_op == ((mask & 2) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD), "depth clear flag reaches SDL");
			CHECK(seenDepth.stencil_load_op == ((mask & 4) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD), "stencil clear flag reaches SDL");
			if (mask & 1) CHECK(seenColor.clear_color.r == 0.25f, "literal clear color reaches SDL");
			CHECK(seenDepth.clear_depth == 0.75f && seenDepth.clear_stencil == 37, "literal depth and stencil clear values reach SDL");
		}
	}
}

/* XNA Refresh GraphicsAdapter.QueryMultiSampleCount IL_0000-000d, 0019-0044:
 * select the greatest supported integer <= request (cap16), <=1 means no MSAA.
 * SDL's supported domain is {2,4,8}; D3D9 may also have odd counts. Literal
 * full-support requests 3,5,7 therefore select 2,4,4, never 4,8,8. */
static void Samples(void)
{
    const int requests[] = {-1,0,1,2,3,4,5,7,8,9,16};
    const int fullExpected[] = {0,0,0,2,2,4,4,4,8,8,8};
    const SDL_GPUSampleCount nativeExpected[] = {SDL_GPU_SAMPLECOUNT_1, SDL_GPU_SAMPLECOUNT_1,
        SDL_GPU_SAMPLECOUNT_1, SDL_GPU_SAMPLECOUNT_2, SDL_GPU_SAMPLECOUNT_2,
        SDL_GPU_SAMPLECOUNT_4, SDL_GPU_SAMPLECOUNT_4, SDL_GPU_SAMPLECOUNT_4,
        SDL_GPU_SAMPLECOUNT_8, SDL_GPU_SAMPLECOUNT_8, SDL_GPU_SAMPLECOUNT_8};
    unsigned mask, i;
    where = "sample negotiation";
    for (mask = 0; mask < 8; mask++)
    {
        SDLGPU_Renderer r;
        SDL_zero(r); supportedSamples = mask;
        for (i = 0; i < SDL_arraysize(requests); i++)
        {
            /* Exhaustive set-membership oracle, including sparse support sets. */
            int expected = 0, candidate;
            for (candidate = 2; candidate <= requests[i] && candidate <= 8; candidate++)
                if ((candidate == 2 && (mask & 1)) || (candidate == 4 && (mask & 2)) ||
                    (candidate == 8 && (mask & 4))) expected = candidate;
            CHECK(SDLGPU_GetMaxMultiSampleCount((FNA3D_Renderer*)&r,
                FNA3D_SURFACEFORMAT_COLOR, requests[i]) == expected, "query returns an actual supported count <= request");
            CHECK(XNAToSDL_SampleCount(requests[i]) == nativeExpected[i], "native conversion rounds down within SDL's count domain");
            if (mask == 7) CHECK(expected == fullExpected[i], "literal 3/5/7 query controls agree with set oracle");
        }
    }
    supportedSamples = 7;
    for (i = 0; i < SDL_arraysize(requests); i++)
    {
        SDLGPU_Renderer r;
        FNA3D_PresentationParameters p;
        SDLGPU_Renderbuffer *color, *depth;
        SDL_zero(r); SDL_zero(p);
        r.commandLock = SDL_CreateMutex(); r.renderTargetInUse = 1;
        p.backBufferWidth = p.backBufferHeight = 4;
        p.backBufferFormat = FNA3D_SURFACEFORMAT_COLOR;
        p.depthStencilFormat = FNA3D_DEPTHFORMAT_D16;
        p.multiSampleCount = requests[i];
        SDLGPU_INTERNAL_CreateFauxBackbuffer(&r, &p);
        CHECK(p.multiSampleCount == fullExpected[i], "backbuffer parameters report the selected count");
        CHECK(SDLGPU_GetBackbufferMultiSampleCount((FNA3D_Renderer*)&r) == fullExpected[i],
              "backbuffer readback count describes allocation");
        CHECK(r.fauxBackbufferDepthStencil->createInfo.sample_count == nativeExpected[i],
              "backbuffer depth uses the selected sample count");
        CHECK((r.fauxBackbufferColorRenderbuffer != NULL) == (fullExpected[i] > 1),
              "only multisampled requests allocate a resolve source");
        if (r.fauxBackbufferColorRenderbuffer)
            CHECK(r.fauxBackbufferColorRenderbuffer->createInfo.sample_count == nativeExpected[i],
                  "backbuffer color allocation agrees with query and reporting");
        CHECK(r.fauxBackbufferColorTexture->createInfo.sample_count == SDL_GPU_SAMPLECOUNT_1,
              "the resolve texture stays single-sampled");
        color = (SDLGPU_Renderbuffer*)SDLGPU_GenColorRenderbuffer((FNA3D_Renderer*)&r,
            4, 4, FNA3D_SURFACEFORMAT_COLOR, requests[i], NULL);
        depth = (SDLGPU_Renderbuffer*)SDLGPU_GenDepthStencilRenderbuffer((FNA3D_Renderer*)&r,
            4, 4, FNA3D_DEPTHFORMAT_D16, requests[i]);
        CHECK(color && depth, "native renderbuffer handles are made");
        if (color) { CHECK(color->sampleCount == nativeExpected[i], "render-target color matches backbuffer rounding"); SDL_free(color->textureHandle); SDL_free(color); }
        if (depth) { CHECK(depth->sampleCount == nativeExpected[i], "render-target depth matches backbuffer rounding"); SDL_free(depth->textureHandle); SDL_free(depth); }
        SDL_free(r.fauxBackbufferColorRenderbuffer); SDL_free(r.fauxBackbufferColorTexture);
        SDL_free(r.fauxBackbufferDepthStencil); SDL_DestroyMutex(r.commandLock);
    }
}

/* CNA FNA3D-8: an acquired swapchain image holds whichever earlier frame last
 * used it, so SDL GPU cannot keep the pixels outside a destination rectangle.
 * The real SwapBuffers must refuse a rectangle that is not exactly the whole
 * drawable (no swapchain acquire, no blit, one logged error, the frame's
 * commands still submitted), and present the whole-drawable rectangle, no
 * rectangle and a source-only rectangle as before. No GPU or window is used.
 */
static void Presentation(void)
{
	static const struct
	{
		const char *name;
		int hasSource, hasDestination;
		FNA3D_Rect source, destination;
		int presents;
	} cases[] = {
		{ "no rectangles", 0, 0, {0, 0, 0, 0}, {0, 0, 0, 0}, 1 },
		{ "destination is the whole drawable", 0, 1, {0, 0, 0, 0}, {0, 0, 128, 96}, 1 },
		{ "source only", 1, 0, {0, 0, 64, 48}, {0, 0, 0, 0}, 1 },
		{ "destination is a region", 0, 1, {0, 0, 0, 0}, {32, 0, 96, 96}, 0 },
		{ "destination one row short", 0, 1, {0, 0, 0, 0}, {0, 0, 128, 95}, 0 },
		{ "destination shifted by one pixel", 0, 1, {0, 0, 0, 0}, {-1, 0, 128, 96}, 0 },
		{ "destination larger than the drawable", 1, 1, {0, 0, 64, 48}, {0, 0, 256, 192}, 0 },
	};
	size_t i;
	for (i = 0; i < SDL_arraysize(cases); i++)
	{
		SDLGPU_Renderer r;
		SDLGPU_TextureHandle backbuffer;
		SDL_Window *windows[1];
		FNA3D_Rect source = cases[i].source, destination = cases[i].destination;
		where = cases[i].name;
		SDL_zero(r); SDL_zero(backbuffer); SDL_zero(seenBlit);
		backbuffer.texture = (SDL_GPUTexture*)&textureToken;
		backbuffer.createInfo.width = 128; backbuffer.createInfo.height = 96;
		windows[0] = (SDL_Window*)&windowToken;
		r.commandLock = SDL_CreateMutex();
		r.fauxBackbufferColorTexture = &backbuffer;
		r.windows = windows; r.numWindows = 1;
		acquired = 0; submitted = 0; loggedErrors = 0; swapchainAcquires = 0; blits = 0;
		SDLGPU_SwapBuffers((FNA3D_Renderer*)&r,
			cases[i].hasSource ? &source : NULL,
			cases[i].hasDestination ? &destination : NULL,
			&windowToken);
		CHECK(swapchainAcquires == cases[i].presents, "a swapchain image is acquired only for a presentation");
		CHECK(blits == cases[i].presents, "only a presentation blits");
		CHECK(loggedErrors == (cases[i].presents ? 0 : 1), "a refused rectangle is reported once, a presentation never");
		CHECK(submitted == 2, "the frame's upload and render commands are submitted either way");
		CHECK(r.numWindows == 1 && r.windows == windows, "the already-claimed window is not claimed again");
		if (cases[i].presents && blits == 1)
		{
			CHECK(seenBlit.destination.texture == (SDL_GPUTexture*)&swapchainToken, "the blit targets the swapchain image");
			CHECK(seenBlit.destination.x == 0 && seenBlit.destination.y == 0 &&
				seenBlit.destination.w == 128 && seenBlit.destination.h == 96, "the blit covers the whole drawable");
			CHECK(seenBlit.source.texture == (SDL_GPUTexture*)&textureToken, "the blit reads the faux back buffer");
			CHECK(seenBlit.source.w == (cases[i].hasSource ? 64u : 128u) &&
				seenBlit.source.h == (cases[i].hasSource ? 48u : 96u), "the source region is the requested one");
		}
		SDL_DestroyMutex(r.commandLock);
	}
}

int main(int argc, char **argv)
{
	if (argc == 1 || SDL_strcmp(argv[1], "samples") == 0) Samples();
	if (argc == 1 || SDL_strcmp(argv[1], "ordering") == 0) Ordering();
	if (argc == 1 || SDL_strcmp(argv[1], "mips") == 0) Mips();
	if (argc == 1 || SDL_strcmp(argv[1], "passes") == 0) DeferredPasses();
	if (argc == 1 || SDL_strcmp(argv[1], "presentation") == 0) Presentation();
	printf("SDL GPU command boundary: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
