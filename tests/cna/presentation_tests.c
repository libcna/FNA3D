/* SPDX-License-Identifier: Zlib */
/* Native OpenGL presentation regression (TQ-FORK-GLPRESENT-01).
 * Compile the real driver with only SwapWindow wrapped. The wrapper reads the
 * window's default framebuffer immediately before the real swap: never the
 * logical backbuffer, a render target, or an asynchronous compositor snapshot.
 */
#include <SDL3/SDL.h>
#include <stdio.h>

static bool PresentSwap(SDL_Window *window);
#define SDL_GL_SwapWindow PresentSwap
#define OpenGLDriver CNAPresent_OpenGLDriver
#define OPENGL_CreateDevice CNAPresent_CreateDevice
#include "FNA3D_Driver_OpenGL.c"
#undef OPENGL_CreateDevice
#undef OpenGLDriver
#undef SDL_GL_SwapWindow

/* GL 3 / ES 3 registry values; the driver's private header omits query enums. */
#define GL_FRAMEBUFFER_BINDING_ENUM 0x8CA6
#define GL_READ_FRAMEBUFFER_BINDING_ENUM 0x8CAA
typedef GLenum (GLAPIENTRY *GetErrorFn)(void);
typedef GLboolean (GLAPIENTRY *IsEnabledFn)(GLenum);
static GetErrorFn getError;
static IsEnabledFn isEnabled;
static int checks, failures, observations;
static OpenGLRenderer *observed;
static const char *phase;
static const char *pixelStage = "presentation";
static FNA3D_Rect expectedDestination;
static int cropped;
#define CHECK(c, message) do { checks++; if (!(c)) { failures++; \
	fprintf(stderr, "FAILED %s: %s (SDL: %s)\n", phase, message, SDL_GetError()); } } while (0)

static void Pixel(int x, int y, int r, int g, int b, int windowH)
{
	unsigned char rgba[4] = {1,2,3,4};
	observed->glReadPixels(x, windowH - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	checks++;
	if (rgba[0] != r || rgba[1] != g || rgba[2] != b)
	{
		failures++;
		fprintf(stderr, "FAILED %s (%s): window (%d,%d) RGB=(%u,%u,%u), expected=(%d,%d,%d)\n",
			phase, pixelStage, x, y, rgba[0], rgba[1], rgba[2], r, g, b);
	}
}
static bool PresentSwap(SDL_Window *window)
{
	int w = 0, h = 0;
	GLint framebuffer = -1;
	const FNA3D_Rect *d = &expectedDestination;
	SDL_GetWindowSizeInPixels(window, &w, &h);
	observations++;
	CHECK(SDL_GL_GetCurrentWindow() == window, "requested window is the current GL drawable");
	observed->glGetIntegerv(GL_FRAMEBUFFER_BINDING_ENUM, &framebuffer);
	CHECK(framebuffer == (GLint)observed->realBackbufferFBO, "presentation writes the native default framebuffer");
	observed->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_ENUM, &framebuffer);
	CHECK(framebuffer == (GLint)observed->realBackbufferFBO, "observer reads the native default framebuffer");
	/* Independent oracle: paint four logical 32x32 quadrants red/green,
	 * yellow/magenta. FNA3D Rect is top-left xywh (FNA3D.h); FNA Present
	 * passes it unchanged, and SDLGPU uses the same xywh swapchain region.
	 * Whole-source quarter points select quadrant centers. For source
	 * (8,4,40,48), these points select x=18/38 and y=16/40, still the four
	 * different quadrants. The crop's red/green split is 24/40=3/5 across:
	 * 11/20 is red for the crop but green for an incorrect full-source blit.
	 * Its vertical split is 28/48=7/12: 13/24 is red, while a blit using
	 * unconverted source y0=4 expands the source to 56 rows and yields yellow.
	 */
	Pixel(d->x + d->w/4, d->y + d->h/4, 255,0,0,h);
	Pixel(d->x + 3*d->w/4, d->y + d->h/4, 0,255,0,h);
	Pixel(d->x + d->w/4, d->y + 3*d->h/4, 255,255,0,h);
	Pixel(d->x + 3*d->w/4, d->y + 3*d->h/4, 255,0,255,h);
	if (cropped) {
		Pixel(d->x + 11*d->w/20, d->y + d->h/4, 255,0,0,h);
		Pixel(d->x + d->w/4, d->y + 13*d->h/24, 255,0,0,h);
	}
	/* Default framebuffer was independently seeded blue before every frame.
	 * Present updates the requested region; it cannot paint these margins.
	 */
	if (d->x > 0) Pixel(d->x - 1, d->y + d->h/2, 0,0,255,h);
	if (d->x + d->w < w) Pixel(d->x + d->w, d->y + d->h/2, 0,0,255,h);
	if (d->y > 0) Pixel(d->x + d->w/2, d->y - 1, 0,0,255,h);
	if (d->y + d->h < h) Pixel(d->x + d->w/2, d->y + d->h, 0,0,255,h);
	CHECK(getError() == 0, "presentation and native pixel observation have no GL error");
	return SDL_GL_SwapWindow(window);
}
static int Resize(SDL_Window *window, int w, int h)
{
	Uint64 deadline = SDL_GetTicks() + 2000;
	int actualW = 0, actualH = 0;
	if (!SDL_SetWindowSize(window, w, h)) return 0;
	if (!SDL_SyncWindow(window)) return 0;
	do {
		SDL_PumpEvents();
		SDL_GetWindowSizeInPixels(window, &actualW, &actualH);
		if (actualW == w && actualH == h) return 1;
		SDL_Delay(10);
	} while (SDL_GetTicks() < deadline);
	return 0;
}
static void Paint(SDL_Window *window)
{
	OpenGLRenderer *r = observed;
	int x, y;
	/* Seed the actual presentation drawable, then paint the logical surface.
	 * Direct GL clears avoid involving shaders or CNA's rendering adapter.
	 * The tiny scissor left enabled must not clip either resolve or blit.
	 */
	pixelStage = "seed";
	CHECK(SDL_GL_MakeCurrent(window, r->context), "select drawable for blue seed");
	BindFramebuffer(r, r->realBackbufferFBO);
	r->glDisable(GL_SCISSOR_TEST);
	r->glClearColor(0,0,1,1);
	r->glClear(GL_COLOR_BUFFER_BIT);
	/* Establish the sentinel precondition on this drawable, independently of
	 * the later presentation checks. No buffer may be inferred from FBO name
	 * zero alone: each native window has its own default framebuffer.
	 */
	{
		int w, h;
		const FNA3D_Rect *d = &expectedDestination;
		SDL_GetWindowSizeInPixels(window, &w, &h);
		CHECK(SDL_GL_GetCurrentWindow() == window, "blue seed uses requested drawable");
		if (d->x > 0) Pixel(d->x - 1, d->y + d->h/2, 0,0,255,h);
		if (d->x + d->w < w) Pixel(d->x + d->w, d->y + d->h/2, 0,0,255,h);
		if (d->y > 0) Pixel(d->x + d->w/2, d->y - 1, 0,0,255,h);
		if (d->y + d->h < h) Pixel(d->x + d->w/2, d->y + d->h, 0,0,255,h);
		CHECK(getError() == 0, "blue seed and its native readback have no GL error");
	}
	pixelStage = "presentation";
	CHECK(SDL_GL_MakeCurrent(r->window, r->context), "restore device drawable before logical painting");
	BindFramebuffer(r, r->backbuffer->type == BACKBUFFER_TYPE_OPENGL ?
		r->backbuffer->opengl.handle : r->realBackbufferFBO);
	r->glEnable(GL_SCISSOR_TEST);
	for (y = 0; y < 2; y++) for (x = 0; x < 2; x++)
	{
		r->glScissor(x*32, 64-(y+1)*32, 32, 32);
		if (y == 0) r->glClearColor(x ? 0 : 1, x ? 1 : 0, 0, 1);
		else r->glClearColor(1, x ? 0 : 1, x ? 1 : 0, 1);
		r->glClear(GL_COLOR_BUFFER_BIT);
	}
	r->glScissor(1,1,1,1);
	r->scissorTestEnable = 1;
}
static void Frame(FNA3D_Device *device, SDL_Window *window,
	FNA3D_Rect *src, FNA3D_Rect *dst, const char *name)
{
	int w, h, before = observations;
	phase = name;
	SDL_GetWindowSizeInPixels(window, &w, &h);
	expectedDestination = dst ? *dst : (FNA3D_Rect){0,0,w,h};
	cropped = src != NULL;
	Paint(window);
	/* This device came from the compiled-in driver factory. Dispatch directly
	 * so a TRACING_SUPPORT library does not see an unregistered trace device. */
	device->SwapBuffers(device->driverData, src, dst, window);
	CHECK(observations == before + 1, "one native swap was observed");
	CHECK(SDL_GL_GetCurrentWindow() == observed->window, "device drawable restored after override");
	CHECK(observed->currentReadFramebuffer == observed->currentDrawFramebuffer,
		"read and draw bindings restored together");
	CHECK(isEnabled(GL_SCISSOR_TEST), "scissor enable restored after presentation");
}
static void Run(int samples)
{
	SDL_Window *window = NULL, *other = NULL;
	SDL_GLContext otherContext = NULL;
	FNA3D_Device *device = NULL;
	FNA3D_PresentationParameters p;
	FNA3D_Rect letterbox = {32,0,96,96}, offset = {17,11,80,72};
	FNA3D_Rect source = {8,4,40,48};
	uint32_t flags = 0;
	phase = samples ? "4x MSAA setup" : "single sample setup";
	CHECK(OPENGL_PrepareWindowAttributes(&flags), "OpenGL window attributes prepared");
	window = SDL_CreateWindow("FNA3D presentation", 64,64, flags | SDL_WINDOW_RESIZABLE);
	CHECK(window != NULL, "create original 64x64 window");
	if (!window) return;
	if (!Resize(window,64,64)) { CHECK(0, "initial physical window is 64x64"); goto done; }
	SDL_zero(p);
	p.backBufferWidth = p.backBufferHeight = 64;
	p.backBufferFormat = FNA3D_SURFACEFORMAT_COLOR;
	p.depthStencilFormat = FNA3D_DEPTHFORMAT_D24S8;
	p.multiSampleCount = samples;
	p.deviceWindowHandle = window;
	p.presentationInterval = FNA3D_PRESENTINTERVAL_IMMEDIATE;
	device = CNAPresent_CreateDevice(&p, 1);
	CHECK(device != NULL, "create native GL device");
	if (!device) goto done;
	observed = (OpenGLRenderer*)device->driverData;
	getError = (GetErrorFn)SDL_GL_GetProcAddress("glGetError");
	isEnabled = (IsEnabledFn)SDL_GL_GetProcAddress("glIsEnabled");
	CHECK(getError != NULL && isEnabled != NULL, "load native GL observation queries");
	if (!getError || !isEnabled) goto done;
	CHECK(observed->backbuffer->type == BACKBUFFER_TYPE_OPENGL, "window-sized creation retains logical framebuffer");
	if (samples) CHECK(FNA3D_GetBackbufferMultiSampleCount(device) > 0, "MSAA case really allocates multisampling");
	Frame(device, window, NULL, NULL, "initial identity");
	/* No FNA3D_ResetBackbuffer: this is the native-only resize defect. */
	if (!Resize(window,160,96)) { CHECK(0, "native resize reaches 160x96"); goto done; }
	Frame(device, window, NULL, &letterbox, "resize then letterbox");
	Frame(device, window, &source, &offset, "asymmetric source and destination");
	Frame(device, window, NULL, NULL, "resize then NULL rectangles");
	other = SDL_CreateWindow("FNA3D override", 200,120, flags);
	CHECK(other != NULL, "create override drawable");
	if (!other) goto done;
	if (!Resize(other,200,120)) { CHECK(0, "override physical size is 200x120"); goto done; }
	/* Retain the override's native drawable while the device context switches
	 * away after seeding. SDL's X11 GLX path binds a raw X window; Mesa may
	 * destroy that implicit GLX drawable (and its back buffer) when the last
	 * referencing context changes drawables. A context kept on this window
	 * gives the blue sentinel stable storage without changing Present.
	 */
	otherContext = SDL_GL_CreateContext(other);
	CHECK(otherContext != NULL, "retain override drawable through a dedicated context");
	if (!otherContext) goto done;
	CHECK(SDL_GL_MakeCurrent(window, observed->context), "restore device context after creating override context");
	Frame(device, other, &source, &offset, "override with rectangles");
	Frame(device, other, NULL, NULL, "override full drawable");
	Frame(device, window, NULL, &letterbox, "original window after override");
	done:
	if (device) device->DestroyDevice(device);
	if (otherContext) SDL_GL_DestroyContext(otherContext);
	if (other) SDL_DestroyWindow(other);
	SDL_DestroyWindow(window);
}
int main(void)
{
	phase = "SDL initialization";
	if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
	Run(0);
	Run(4);
	SDL_Quit();
	printf("OpenGL native presentation: %d checks, %d failed, %d default-framebuffer observations\n",
		checks, failures, observations);
	return failures ? 1 : 0;
}
