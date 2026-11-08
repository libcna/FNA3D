/* SPDX-License-Identifier: Zlib */
/* Window-free driver regressions. The real OpenGL entry points execute with GL
 * calls captured at their boundary, without creating a context or device.
 * Pixel qualification remains in vface_tests.c and mip_clamp_tests.c.
 */
#include <SDL3/SDL.h>
#include <stdio.h>

static bool StateWindowSize(SDL_Window *window, int *w, int *h);
static bool StateSwap(SDL_Window *window);
static bool StateMakeCurrent(SDL_Window *window, SDL_GLContext context);
#define SDL_GetWindowSizeInPixels StateWindowSize
#define SDL_GL_SwapWindow StateSwap
#define SDL_GL_MakeCurrent StateMakeCurrent
#define OpenGLDriver CNAState_OpenGLDriver
#define MOJOSHADER_glProgramPixelCenterInfo CNAState_PixelCenter
#include "FNA3D_Driver_OpenGL.c"
#undef MOJOSHADER_glProgramPixelCenterInfo
#undef OpenGLDriver
#undef SDL_GetWindowSizeInPixels
#undef SDL_GL_SwapWindow
#undef SDL_GL_MakeCurrent

static int checks, failures, draws, writes;
static GLenum functions[2], operations[2][3];
static GLint references[2], baseLevel;
static GLuint masks[2];
static const char *where;
#define CHECK(c, message) do { checks++; if (!(c)) { failures++; \
	fprintf(stderr, "FAILED %s: %s\n", where, message); } } while (0)

void CNAState_PixelCenter(float x, float y) { (void)x; (void)y; }
static int Face(GLenum face) { return face == GL_FRONT ? 0 : 1; }
static void GLAPIENTRY StencilFunc(GLenum face, GLenum func, GLuint ref, GLuint mask)
{
	int i = Face(face); functions[i] = func; references[i] = ref; masks[i] = mask;
}
static void GLAPIENTRY StencilOp(GLenum face, GLenum fail, GLenum zfail, GLenum pass)
{
	int i = Face(face); operations[i][0] = fail; operations[i][1] = zfail; operations[i][2] = pass;
}
static void GLAPIENTRY DrawArrays(GLenum mode, GLint first, GLsizei count)
{
	(void)mode; (void)first; (void)count; draws++;
}
static void GLAPIENTRY DrawRange(GLenum mode, GLuint start, GLuint end, GLsizei count,
	GLenum type, const void *indices)
{
	(void)mode; (void)start; (void)end; (void)count; (void)type; (void)indices; draws++;
}
static void GLAPIENTRY DrawInstances(GLenum mode, GLsizei count, GLenum type,
	const void *indices, GLsizei instances)
{
	(void)mode; (void)count; (void)type; (void)indices; (void)instances; draws++;
}
static void GLAPIENTRY TexParameter(GLenum target, GLenum name, GLint value)
{
	CHECK(target == GL_TEXTURE_2D && name == GL_TEXTURE_BASE_LEVEL, "only the base level changes");
	baseLevel = value; writes++;
}
static void Stencil(void)
{
	/* XNA DepthStencilState.Apply IL_0154..0242 installs ordinary and CCW
	 * operations independently of culling. A CW XNA triangle is GL_CW on
	 * the backbuffer, GL_CCW on a flipped render target (the VFACE oracle).
	 * Rows name the resulting GL front/back operation sets literally.
	 */
	static const struct { int target; GLenum front; int ordinaryFront; } cases[] = {
		{0, GL_CW, 1}, {0, GL_CCW, 0}, {1, GL_CW, 0}, {1, GL_CCW, 1}
	};
	static const GLenum ordinary[] = {GL_ZERO, GL_INCR_WRAP, GL_REPLACE};
	static const GLenum ccw[] = {GL_KEEP, GL_DECR_WRAP, GL_INVERT};
	size_t i;
	int draw, face;
	where = "two-sided stencil";
	for (i = 0; i < SDL_arraysize(cases); i++) for (draw = 0; draw < 3; draw++)
	{
		OpenGLRenderer r;
		OpenGLBuffer index;
		FNA3D_DepthStencilState state;
		SDL_zero(r); SDL_zero(index); SDL_zero(state);
		r.glStencilFuncSeparate = StencilFunc; r.glStencilOpSeparate = StencilOp;
		r.glDrawArrays = DrawArrays; r.glDrawRangeElements = DrawRange;
		r.glDrawElementsInstanced = DrawInstances; r.supports_ARB_draw_instanced = 1;
		r.renderTargetBound = cases[i].target; r.frontFace = cases[i].front;
		/* threadID=0 keeps Activate off the device-thread/context path. */
		r.stencilEnable = 1; r.stencilWriteMask = -1;
		state.stencilEnable = 1; state.twoSidedStencilMode = 1;
		state.stencilMask = 255; state.stencilWriteMask = -1; state.referenceStencil = 5;
		state.stencilFunction = FNA3D_COMPAREFUNCTION_EQUAL;
		state.ccwStencilFunction = FNA3D_COMPAREFUNCTION_NEVER;
		state.stencilFail = FNA3D_STENCILOPERATION_ZERO;
		state.stencilDepthBufferFail = FNA3D_STENCILOPERATION_INCREMENT;
		state.stencilPass = FNA3D_STENCILOPERATION_REPLACE;
		state.ccwStencilFail = FNA3D_STENCILOPERATION_KEEP;
		state.ccwStencilDepthBufferFail = FNA3D_STENCILOPERATION_DECREMENT;
		state.ccwStencilPass = FNA3D_STENCILOPERATION_INVERT;
		OPENGL_SetDepthStencilState((FNA3D_Renderer*)&r, &state);
		draws = 0;
		if (draw == 0) OPENGL_DrawPrimitives((FNA3D_Renderer*)&r, FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 1);
		else if (draw == 1) OPENGL_DrawIndexedPrimitives((FNA3D_Renderer*)&r,
			FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 0, 3, 0, 1, (FNA3D_Buffer*)&index,
			FNA3D_INDEXELEMENTSIZE_16BIT);
		else OPENGL_DrawInstancedPrimitives((FNA3D_Renderer*)&r,
			FNA3D_PRIMITIVETYPE_TRIANGLELIST, 0, 0, 3, 0, 1, 2, (FNA3D_Buffer*)&index,
			FNA3D_INDEXELEMENTSIZE_16BIT);
		CHECK(draws == 1, "each public draw route actually issues its draw");
		for (face = 0; face < 2; face++)
		{
			int isOrdinary = (face == 0) == cases[i].ordinaryFront;
			CHECK(functions[face] == (isOrdinary ? GL_EQUAL : GL_NEVER), "stencil comparison follows winding");
			CHECK(SDL_memcmp(operations[face], isOrdinary ? ordinary : ccw, sizeof(ordinary)) == 0,
				"all fail/depth-fail/pass operations follow winding");
			CHECK(references[face] == 5 && masks[face] == 255, "comparison reference/mask survive remapping");
		}
	}
}
static void Mips(void)
{
	/* XNA SamplerState.Apply IL_0342..0355 forwards uint32 MaxMipLevel.
	 * Stored levels of a 4x4 texture are 0,1,2: min(uint32(request),2).
	 * The capture checks texture completeness at the GL API, not GPU pixels.
	 */
	static const int requests[] = {0, 1, 2, -1, 99, 0};
	static const int expected[] = {0, 1, 2, 2, 2, 0};
	OpenGLRenderer r;
	OpenGLTexture tex;
	FNA3D_SamplerState sampler;
	size_t i;
	SDL_zero(r); SDL_zero(tex); SDL_zero(sampler);
	tex.target = GL_TEXTURE_2D; tex.levelCount = 3;
	r.textures[0] = &tex; r.glTexParameteri = TexParameter;
	baseLevel = 0; writes = 0; where = "stored mip clamp";
	for (i = 0; i < SDL_arraysize(requests); i++)
	{
		sampler.maxMipLevel = requests[i];
		OPENGL_VerifySampler((FNA3D_Renderer*)&r, 0, (FNA3D_Texture*)&tex, &sampler);
		if (baseLevel != expected[i]) fprintf(stderr, "request=%d GL base=%d expected=%d\n", requests[i], baseLevel, expected[i]);
		CHECK(baseLevel == expected[i], "base is an unsigned, stored mip level");
		CHECK(tex.maxMipmapLevel == expected[i], "cached applied base agrees with the GL state");
	}
	CHECK(writes > 0, "the fixture exercised real GL parameter calls");
}
/* ES 3.2 specification sections 12.6.1 and 15.2.3 name DepthRangef and
 * ClearDepthf; the desktop double entry points are not ES commands. A loader
 * can still return non-NULL desktop dispatch stubs in an ES context. */
static int doubleClears, floatClears, doubleRanges, floatRanges, depthClears;
static double clearValue, nearValue, farValue;
static void GLAPIENTRY ClearDepthDouble(GLdouble value)
{
	doubleClears++; clearValue = value;
}
static void GLAPIENTRY ClearDepthFloat(GLfloat value)
{
	floatClears++; clearValue = value;
}
static void GLAPIENTRY DepthRangeDouble(GLdouble nearDepth, GLdouble farDepth)
{
	doubleRanges++; nearValue = nearDepth; farValue = farDepth;
}
static void GLAPIENTRY DepthRangeFloat(GLfloat nearDepth, GLfloat farDepth)
{
	floatRanges++; nearValue = nearDepth; farValue = farDepth;
}
static void GLAPIENTRY ClearDepthBuffer(GLbitfield mask)
{
	CHECK(mask == GL_DEPTH_BUFFER_BIT, "depth-only clear forwards the depth bit");
	depthClears++;
}
static void DepthApi(void)
{
	int es, doubleSupport;
	where = "depth API profile";
	for (es = 0; es < 2; es++) for (doubleSupport = 0; doubleSupport < 2; doubleSupport++)
	{
		OpenGLRenderer r;
		FNA3D_Viewport viewport = {0, 0, 64, 32, .125f, .875f};
		FNA3D_Vec4 unused = {0, 0, 0, 0};
		/* Desktop keeps doubles when available; ES must use its float API even
		 * when both desktop pointers are non-NULL (as Mesa's dispatch can be). */
		const int wantDouble = !es && doubleSupport;
		SDL_zero(r);
		r.useES3 = es;
		r.supports_DoublePrecisionDepth = doubleSupport;
		r.glClearDepth = ClearDepthDouble; r.glClearDepthf = ClearDepthFloat;
		r.glDepthRange = DepthRangeDouble; r.glDepthRangef = DepthRangeFloat;
		r.glClear = ClearDepthBuffer;
		r.currentClearDepth = 1; r.zWriteEnable = 1;
		r.renderTargetBound = 1; r.viewport = viewport;
		r.depthRangeMin = 0; r.depthRangeMax = 1;
		doubleClears = floatClears = doubleRanges = floatRanges = depthClears = 0;
		/* Repeat identical values: the API choice and cached-state behavior both matter. */
		OPENGL_Clear((FNA3D_Renderer*)&r, FNA3D_CLEAROPTIONS_DEPTHBUFFER, &unused, .25f, 0);
		OPENGL_Clear((FNA3D_Renderer*)&r, FNA3D_CLEAROPTIONS_DEPTHBUFFER, &unused, .25f, 0);
		OPENGL_SetViewport((FNA3D_Renderer*)&r, &viewport);
		OPENGL_SetViewport((FNA3D_Renderer*)&r, &viewport);
		CHECK(doubleClears == wantDouble && floatClears == !wantDouble,
			"ES depth clear uses ClearDepthf despite a non-NULL desktop function");
		CHECK(doubleRanges == wantDouble && floatRanges == !wantDouble,
			"ES depth range uses DepthRangef despite a non-NULL desktop function");
		CHECK(depthClears == 2 && clearValue == .25, "both clears execute with the literal depth");
		CHECK(nearValue == .125 && farValue == .875, "literal viewport range reaches the selected API");
		CHECK(r.currentClearDepth == .25f && r.depthRangeMin == .125f && r.depthRangeMax == .875f,
			"cached depths match the submitted float values");
	}
}

/* FNA3D.h: Rect is xywh and Present updates a window region. FNA
 * GraphicsDevice.Present passes both rectangles unchanged. The SDL GPU driver
 * uses xywh with the override swapchain's dimensions. GL endpoints below are
 * independently derived: (x,H-y-h,x+w,H-y), for each surface's own H.
 */
static int physicalW, physicalH, swaps, blits, switches, disables, enables;
static int endpoints[2][8];
static GLenum filters[2];
static SDL_Window *currentWindow, *swappedWindow;
static bool StateWindowSize(SDL_Window *window, int *w, int *h)
{
	(void)window; *w = physicalW; *h = physicalH; return true;
}
static bool StateSwap(SDL_Window *window)
{
	CHECK(window == currentWindow, "swap uses the current drawable");
	swaps++; swappedWindow = window; return true;
}
static bool StateMakeCurrent(SDL_Window *window, SDL_GLContext context)
{
	(void)context; switches++; currentWindow = window; return true;
}
static void GLAPIENTRY PresentBlit(GLint x0, GLint y0, GLint x1, GLint y1,
	GLint dx0, GLint dy0, GLint dx1, GLint dy1, GLbitfield mask, GLenum filter)
{
	int values[8] = {x0,y0,x1,y1,dx0,dy0,dx1,dy1};
	CHECK(blits < 2 && mask == GL_COLOR_BUFFER_BIT, "at most resolve plus color presentation");
	if (blits < 2) { SDL_memcpy(endpoints[blits], values, sizeof(values)); filters[blits] = filter; }
	blits++;
}
static void GLAPIENTRY PresentBind(GLenum target, GLuint handle) { (void)target; (void)handle; }
static void GLAPIENTRY PresentGen(GLsizei count, GLuint *handles)
{
	int i; for (i = 0; i < count; i++) handles[i] = 7 + i;
}
static void GLAPIENTRY PresentStorage(GLenum target, GLenum format, GLsizei w, GLsizei h)
{ (void)target; (void)format; CHECK(w == 64 && h == 64, "logical allocation survives physical resize"); }
static void GLAPIENTRY PresentAttach(GLenum target, GLenum attachment, GLenum type, GLuint handle)
{ (void)target; (void)attachment; (void)type; (void)handle; }
static void GLAPIENTRY PresentTexture(GLenum target, GLenum attachment, GLenum type, GLuint handle, GLint level)
{ (void)target; (void)attachment; (void)type; (void)handle; (void)level; }
static void GLAPIENTRY PresentDisable(GLenum state) { CHECK(state == GL_SCISSOR_TEST, "only scissor disabled"); disables++; }
static void GLAPIENTRY PresentEnable(GLenum state) { CHECK(state == GL_SCISSOR_TEST, "only scissor restored"); enables++; }
static void Presentation(void)
{
	/* Literal endpoints: original witness, asymmetric crop+offset, and NULL
	 * rectangles on a differently sized override window. MSAA uses identical
	 * full-surface resolve bounds (0,0,64,64) before the same presentation.
	 */
	static const int expected[][8] = {
		{0,0,64,64,32,0,128,96},
		{8,28,32,60,17,31,89,79},
		{0,0,64,64,0,0,200,120}
	};
	static const int resolve[8] = {0,0,64,64,0,0,64,64};
	FNA3D_Rect source = {8,4,24,32}, destination = {32,0,96,96};
	FNA3D_Rect offset = {17,17,72,48};
	FNA3D_PresentationParameters p;
	OpenGLRenderer r;
	int windowA, windowB, i, msaa;
	where = "presentation";
	SDL_zero(r); SDL_zero(p);
	p.deviceWindowHandle = &windowA; p.backBufferWidth = p.backBufferHeight = 64;
	p.depthStencilFormat = FNA3D_DEPTHFORMAT_NONE;
	r.supports_EXT_framebuffer_blit = 1;
	r.glGenFramebuffers = PresentGen; r.glGenRenderbuffers = PresentGen;
	r.glBindFramebuffer = PresentBind; r.glBindRenderbuffer = PresentBind;
	r.glRenderbufferStorage = PresentStorage; r.glFramebufferRenderbuffer = PresentAttach;
	r.glBlitFramebuffer = PresentBlit; r.glFramebufferTexture2D = PresentTexture;
	r.glDisable = PresentDisable; r.glEnable = PresentEnable;
	physicalW = physicalH = 64;
	OPENGL_INTERNAL_CreateBackbuffer(&r, &p);
	CHECK(r.backbuffer != NULL && r.backbuffer->type == BACKBUFFER_TYPE_OPENGL,
		"even an initially window-sized surface is independent of the drawable");
	/* Simulate a native-only resize, without ResetBackbuffer. */
	physicalW = 160; physicalH = 96;
	r.scissorTestEnable = 1; r.backbufferScaleMode = GL_LINEAR;
	r.resolveFramebufferDraw = 11;
	for (msaa = 0; msaa < 2; msaa++) for (i = 0; i < 3; i++)
	{
		FNA3D_Rect *src = i == 1 ? &source : NULL;
		FNA3D_Rect *dst = i == 0 ? &destination : i == 1 ? &offset : NULL;
		SDL_Window *window = i == 2 ? (SDL_Window*)&windowB : r.window;
		r.backbuffer->multiSampleCount = msaa ? 4 : 0;
		r.backbuffer->opengl.texture = 9; /* Already allocated resolve texture. */
		physicalW = i == 2 ? 200 : 160; physicalH = i == 2 ? 120 : 96;
		currentWindow = r.window;
		swaps = blits = switches = disables = enables = 0;
		OPENGL_SwapBuffers((FNA3D_Renderer*)&r, src, dst, window);
		CHECK(blits == 1 + msaa, "every frame blits, resolving MSAA first");
		CHECK(SDL_memcmp(endpoints[msaa], expected[i], sizeof(expected[i])) == 0,
			"source/destination top-left xywh reach GL as full endpoints");
		CHECK(filters[msaa] == GL_LINEAR, "presentation retains selected scaling filter");
		if (msaa) {
			CHECK(SDL_memcmp(endpoints[0], resolve, sizeof(resolve)) == 0 && filters[0] == GL_NEAREST,
				"MSAA resolves with identical bounds and ES3-compatible nearest filter");
		}
		CHECK(swaps == 1 && swappedWindow == window, "exactly one swap targets requested window");
		CHECK(currentWindow == r.window && switches == (i == 2 ? 2 : 0),
			"override drawable is selected then original drawable restored");
		CHECK(disables == 1 && enables == 1, "scissor cannot clip presentation and is restored");
		CHECK(r.currentReadFramebuffer == r.backbuffer->opengl.handle &&
			r.currentDrawFramebuffer == r.backbuffer->opengl.handle, "logical framebuffer restored");
	}
	SDL_free(r.backbuffer);
}

int main(int argc, char **argv)
{
	if (argc == 1 || SDL_strcmp(argv[1], "stencil") == 0) Stencil();
	if (argc == 1 || SDL_strcmp(argv[1], "mips") == 0) Mips();
	if (argc == 1 || SDL_strcmp(argv[1], "depth") == 0) DepthApi();
	if (argc == 1 || SDL_strcmp(argv[1], "presentation") == 0) Presentation();
	printf("OpenGL state boundary: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
