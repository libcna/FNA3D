/* SPDX-License-Identifier: Zlib */
/* Window-free SDL GPU boundary regressions. The real driver executes against
 * captured submission/upload/sampler calls, not a second implementation.
 * Queue-order proof here complements texture_order_tests.c's owner-run pixels.
 */
#include <SDL3/SDL.h>
#include <stdio.h>

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
#define SDLGPUDriver CNACommand_SDLGPUDriver
#define SDL_CreateGPUSampler TestSampler
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
static int samplerCalls;
static bool cycled;
static SDL_GPUCommandBuffer *priorRender;
static SDL_GPUSamplerCreateInfo samplerInfo;
static SDL_GPUTextureRegion uploadedRegion;
static char commandTokens[64], samplerToken, copyToken, renderToken, textureToken, transferToken;
static uint8_t transferBytes[256];
static const char *where;
#define CHECK(c, message) do { checks++; if (!(c)) { failures++; \
	fprintf(stderr, "FAILED %s: %s\n", where, message); } } while (0)
void FNA3D_LogWarn(const char *fmt, ...) { (void)fmt; }
void FNA3D_LogError(const char *fmt, ...) { (void)fmt; }
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
	(void)command; (void)depth;
	CHECK(count == 1, "a deferred render pass has a real attachment");
	if (count == 1 && colors[0].load_op == SDL_GPU_LOADOP_CLEAR) clearAt = ++event;
	return (SDL_GPURenderPass*)&renderToken;
}
static void SDLCALL TestViewport(SDL_GPURenderPass *pass, const SDL_GPUViewport *viewport)
{
	(void)pass; (void)viewport;
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
	 * has to be materialized before its submission. Initial dummy uploads,
	 * before any render target exists, must not start an attachment-less pass.
	 */
	int kind;
	where = "upload API order";
	for (kind = 0; kind < 6; kind++)
	{
		SDLGPU_Renderer r;
		SDLGPU_TextureHandle texture, color;
		SDLGPU_TextureHandle *bound[4];
		uint8_t bytes[256] = {0};
		SDL_zero(r); SDL_zero(texture); SDL_zero(color);
		event = submitted = priorRenderAt = uploadedAt = clearAt = 0; acquired = 2;
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
		if (kind == 4) CHECK(clearAt > 0 && clearAt < priorRenderAt, "clear-only work is not lost at the split");
		CHECK(cycled == (kind != 1 && kind != 2 && kind != 3), "only complete single-level 2D updates cycle");
		CHECK(uploadedRegion.texture == texture.texture && uploadedRegion.w == 4 && uploadedRegion.h == 4,
			"upload destination and extent are preserved");
		if (kind == 2) CHECK(uploadedRegion.layer == FNA3D_CUBEMAPFACE_NEGATIVEZ, "the requested cube face is retained");
		if (kind == 3) CHECK(uploadedRegion.d == 4, "the volume extent is retained");
		SDL_DestroyMutex(r.commandLock);
	}
}
int main(int argc, char **argv)
{
	if (argc == 1 || SDL_strcmp(argv[1], "ordering") == 0) Ordering();
	if (argc == 1 || SDL_strcmp(argv[1], "mips") == 0) Mips();
	printf("SDL GPU command boundary: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
