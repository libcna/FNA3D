/* SPDX-License-Identifier: Zlib */
/* Window-free driver regressions. The real OpenGL entry points execute with GL
 * calls captured at their boundary, without creating a context or device.
 * Pixel qualification remains in vface_tests.c and mip_clamp_tests.c.
 */
#include <SDL3/SDL.h>
#include <stdio.h>

#define OpenGLDriver CNAState_OpenGLDriver
#define MOJOSHADER_glProgramPixelCenterInfo CNAState_PixelCenter
#include "FNA3D_Driver_OpenGL.c"
#undef MOJOSHADER_glProgramPixelCenterInfo
#undef OpenGLDriver

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
int main(int argc, char **argv)
{
	if (argc == 1 || SDL_strcmp(argv[1], "stencil") == 0) Stencil();
	if (argc == 1 || SDL_strcmp(argv[1], "mips") == 0) Mips();
	printf("OpenGL state boundary: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
