# libcna fork regression tests

Tests for defects fixed on this fork's `cna` branch, built only with `-DFNA3D_CNA_TESTS=ON`. Most
open an SDL window and create a real device, so they need a display (in CNA's checkout:
`tools/platform/run_gpu_tests_private.sh --exec ctest --test-dir <build>`); the entries marked
"no display" below do not.

- `readback_tests.c`: `GetTextureData2D`/`GetTextureDataCube` write exactly the bytes a format and
  rectangle occupy -- every uncompressed format, odd widths, partial rectangles, non-square mip
  chains, cube faces, an attached render target -- into buffers with guard bytes behind them, and
  refuse a level, rectangle or destination that does not fit. Cube-face and volume uploads come
  from exactly sized allocations, so a read past the caller's data shows under AddressSanitizer;
  no driver has a volume readback, and each must leave the destination untouched. On OpenGL
  ES 3 RGBA8, sRGB RGBA8 and the float formats must read back; any other format must read back
  exactly when `GL_IMPLEMENTATION_COLOR_READ_FORMAT`/`_TYPE`, asked of the level attached to a
  framebuffer of the test's own, are the format's layout, and must be refused otherwise. 4x
  multisampled RGBA8 and sRGB targets must resolve to their clear colour. DXT1 levels must read
  back as their blocks on SDL_GPU and be refused, the destination untouched, on OpenGL and D3D11;
  the driver is the one FNA3D announces (SDL_GPU announces none), checked against
  `FNA3D_FORCE_DRIVER`. On OpenGL, reads with
  `GL_PACK_ALIGNMENT` 8 and uploads (2D, cube face, volume) with `GL_UNPACK_ALIGNMENT` 8 -- and
  then a row length and skipped rows, pixels and images too -- left on the context must stay
  tightly packed and leave that state in place. Build with
  `-DCMAKE_C_FLAGS="-fsanitize=address,undefined"` to have the sanitizers watch the driver's own
  temporary buffers as well.
- `dispose_tests.c`: a texture, two buffers, a renderbuffer and a query disposed from another
  thread wait for the next `SwapBuffers`; destroying the device first must still free them, and
  the device its own caches -- a sampler, and vertex buffer bindings applied through an effect
  (`test_effect.h`). The test counts live SDL allocations through `SDL_SetMemoryFunctions`
  around a device's lifetime, on OpenGL, OpenGL ES 3 and the SDL_GPU driver.
- `backbuffer_tests.c`: `ReadBackbuffer` returns the rectangle asked for, in the backbuffer's
  top-left origin. Every pixel is painted its own colour (row and half) by drawing through a
  scissor rectangle per row half, the scissor test left on as a draw leaves it; full and
  asymmetric partial reads must then hold exactly those colours in order and write nothing past
  `w * h * 4` bytes -- on a window-sized backbuffer, a smaller faux one and 4x multisampled ones,
  whose resolve the scissor must not clip.
- `vertex_usage_tests.c`: a `Depth` vertex element feeds the shader's DEPTH input. A vertex
  shader that declares `dcl_depth v1` and outputs v1 as its colour must paint a whole-viewport
  triangle in the colour its Depth stream carries, with the element after Position and before
  it; the same draw through `dcl_color v1` and a Color element is the control.
- `vface_tests.c`: a ps_3_0 shader paints green where VFACE >= 0 and red where it is negative; a
  clockwise and a counter-clockwise triangle under each cull mode, into the backbuffer and a render
  target, must show Direct3D 9's picture -- VFACE positive on the clockwise triangle, which
  CullClockwiseFace removes.
- `pipeline_cache_tests.c` (no display): a blend state's pipeline-cache key tells apart states
  that differ only in a blend function under an alpha destination blend of DestinationAlpha or
  later, or only in the multisample mask under a white blend factor -- which the int32_t packing's
  sign extension merged into one key.
- `sdlgpu_properties_tests.c` (no display): the SDL_GPU driver's device-creation properties say
  exactly the shader formats `MOJOSHADER_sdlGetShaderFormats()` offers -- every single format, an
  SDL_shadercross with FXC but no DXC's SPIR-V, DXBC and MSL, and all of them -- DXBC included,
  without which SDL's D3D12 backend refuses a device that has no DXIL. The driver is compiled
  into the test with that query replaced.
- `installed_consumer.cmake` (no display): `cmake --install` into the build tree, the
  installation moved away from its prefix, then the Effect client in `installed_consumer/` --
  it declares `MOJOSHADER_effectStateChanges` and reads `MOJOSHADER_effect` -- configured,
  built and run through `find_package(FNA3D)` and, when pkg-config is present, through
  `pkg-config --define-prefix`; no path into FNA3D's source or build tree may reach its compiler.
  The configuration ctest runs (`-C`, which a multi-config tree such as Ninja Multi-Config needs)
  is the one installed and built, and the consumer names its own executable's path.
  Registered only with `BUILD_SHARED_LIBS`, as only a shared FNA3D installs.
- `static_shadercross.cmake` (no display; `static_shadercross/`): `MOJOSHADER_STATIC_SPIRVCROSS`
  against a stand-in static `SDL3_shadercross` package, installed from `static_shadercross/` --
  a C archive with the `SDL_ShaderCross_*` functions linking, through `$<LINK_ONLY:>`, a C++
  archive in place of SPIRV-Cross. FNA3D's own source is configured with the option against it
  and its library built and installed (in the test's work directory, incrementally after the
  first run): `mojoshader_sdlgpu.c` must be compiled with
  `CNA_MOJOSHADER_SDL_GPU_SHADERCROSS_STATIC`, and a C client linked by the C driver must link,
  load and see the stand-in's DXBC and METALLIB in `MOJOSHADER_sdlGetShaderFormats()` -- which
  takes the archive called from inside libFNA3D and the C++ runtime linked into it. It proves
  FNA3D's selector and linkage, not the real package (whose config also asks for SPIRV-Cross's
  packages and DirectXShaderCompiler) or any translation. Registered for GCC/Clang on Linux-like
  systems with a C++ compiler.
- `fna3d_cna_replay_sdl2_gnu99`/`_gnu17` (no display, `CMakeLists.txt` only): `replay/replay.c`
  must compile against SDL 2, as `BUILD_SDL3=OFF TRACING_SUPPORT=ON` builds it -- a
  `-fsyntax-only` check with this directory's definitions minus the SDL 3 ones, plus
  `FNA3D_TRACING`. Registered only when SDL 2's CMake package is found.
- `trace_tests.cmake` (`TRACING_SUPPORT` builds only; `fna3d_cna_trace_opengl`, `_opengl_es3`,
  `_sdlgpu`): `trace_record.c` records three frames -- the CNA extension calls (marks 200 and
  201), each right after a string marker, then a later frame changing the pass of an applied
  effect, then a Depth vertex element in Byte4 -- and finds each extension record, with its
  arguments, after its marker. `fna3d_replay` replays the trace with `trace_interposer.c`
  preloaded, which logs what it hands the two extension entry points; the log must be the
  captured calls. The interposer is built only where it can see those calls -- a shared FNA3D
  on a platform whose loader takes `LD_PRELOAD` and has `dlfcn.h`; elsewhere (a static FNA3D,
  Windows, macOS) the replay runs without it and the log comparison is reported as skipped, with
  the reason. Under AddressSanitizer the replay runs with `detect_stack_use_after_return=1`,
  which catches a pass change writing through a dead frame's state-change record; without it that
  write cannot show. `fna3d_dumpspirv` then dumps the trace and `spirv_check.c`, linking the same
  shaders through libFNA3D's MojoShader, finds the Depth element linked as DEPTH, not as
  POSITION. `SDL_ASSERT=abort` turns either tool's assertions into failures.
- `trace_overlap_tests.c` (`TRACING_SUPPORT` builds only; `fna3d_cna_trace_overlap_opengl`,
  `_opengl_es3`, `_sdlgpu`, each in a `trace-overlap-<driver>` directory of its own): a
  `CreateDevice` that makes no device (before `FNA3D_PrepareWindowAttributes`) must leave no SDL
  allocation behind; then two overlapping devices -- a marker through each, `DestroyDevice(NULL)`,
  the first destroyed, a marker through the second -- must leave none either, and
  `FNA3D_Trace.bin` must be exactly the first device's `CreateDevice` record, the three markers
  and one `DestroyDevice` record. Live allocations are counted as in `dispose_tests.c`.
- `test_effect.h`: the minimal Effect the drawing tests use, built in memory from the Effects
  Framework format (no Effect compiler runs here): a vs_2_0 that passes POSITION0 to `oPos` and
  its COLOR0 or DEPTH0 input to `oD0`, and a ps_2_0 that writes that colour, in one technique of
  up to four passes.

### 2026-11-07 device startup and deferred clears

- `startup_tests.c` creates a real debug device before the caller supplies any
  viewport, effect or draw. `startup_tests.cmake` rejects native validation messages
  on stdout/stderr, including Vulkan output that bypasses SDL's log callback. All
  three drivers run it. Initial texture uploads must not invent an empty render
  pass with an unset viewport.
- `sdlgpu_command_tests.c` also covers the already-attached faux backbuffer before
  the first viewport. Its `passes` group executes every combination of pending
  color/depth/stencil clears: no work starts no pass, and a pending clear starts
  one pass with the exact load operations and literal clear values. Existing upload
  ordering, clear-only submission and mip assertions remain in the default suite.

### 2026-11-07 dependent register texture reads

- `texreg_tests.c`: `texreg2ar`, `texreg2gb`, `texreg2rgb` and `texreg2rgb_bx2`
  each render all six legal source/destination pairs among t0..t3. A one-texel source
  `(32,96,160,224)` selects a hand-derived texel in a distinct 4x4 point-clamped lookup;
  all 64x32 pixels must match its literal RGBA bytes. The three native drivers run
  24 draws each. A vs_2_0 supplies every implicit ps_1_2 texture input. No CNA adapter,
  stock effect or tolerance is involved. This public FNA3D path tests 2D textures;
  cube/volume mappings are covered separately by MojoShader's sampler-map tests.

### 2026-11-06 CNA quality regressions

- `opengl_state_tests.c` (no display): compiles the actual OpenGL driver into the test and captures
  its GL calls. Ordinary, indexed and instanced draw entry points must map all two-sided stencil
  operations and comparisons onto XNA's clockwise face for both GL front-face values and both
  destinations. Its mip cases send 0, 1, 2, -1, 99, 0 to a three-level texture, requiring stored
  base levels 0, 1, 2, 2, 2, 0 and the same cached state. XNA Refresh `DepthStencilState.Apply`
  IL_0154–0242 and `SamplerState.Apply` IL_0342–0355 are the managed oracles; no GL context is used.
- `sdlgpu_command_tests.c` (no display): executes the actual SDL GPU driver with submission,
  upload and sampler creation calls captured. Existing uploads/rendering, including a deferred
  clear, must be submitted before a later 2D/mipmapped/cube/volume update is recorded. The split
  must invalidate draw bindings, retain upload extents/face, and leave startup dummy uploads
  alone before attachments exist. Sampler cases require unsigned mip bounds and native cache
  reuse. `presentation`: `SwapBuffers` clears the swapchain to opaque black whenever the
  destination rectangle does not cover it exactly (a letterbox, a region Present), because an
  acquired swapchain image holds an older frame and SDL GPU cannot keep the pixels outside it
  (CNA FNA3D-8); a full-coverage blit stays DONT_CARE. These call-boundary checks complement
  rather than replace GPU pixels. Both window-free
  executables run their whole set by default; `stencil`/`mips` and `ordering`/`mips` arguments
  respectively select a case group for baseline/mutation evidence.
- `vface_tests.c` also probes actual two-sided stencil pixels on the backbuffer and a D24S8
  render target, after each prior cull mode. Ordinary Replace=5 and CCW Invert(0)=255 select the
  literal green/red triangle centers independently of VFACE's own sign correction.
- `texture_order_tests.c` (display required): direct SM2 texture sampling draws red left, changes
  the texture to green, draws right, and only then reads pixels. The whole single-level 2D case
  controls cycling; mipmapped 2D, cube and volume cases cover the shared upload-order boundary.
  Higher-mip cases also require mip sampling to work; use `mip_clamp_tests.c` to distinguish a
  sampling failure from upload ordering. No vertex-buffer data changes occur between draws.
- `mip_clamp_tests.c` (display required): verifies native per-mip upload/readback identity before
  direct sampling of distinct red/green/blue mips. Requests 0,1,2,-1,99,0 must select stored levels
  0,1,2,2,2,0. This bypasses CNA's SpriteBatch and cannot by itself attribute a CNA-only failure.
- `malformed_effect_tests.c` (display required; investigation only): takes an existing
  provenance-tracked `BasicEffect.fxb` argument; checks its byte 25018 is 0x6f, compiles a valid
  control, then XORs 0x10 and requires a normal `Shader parameter not found in effect.` error
  and normal disposal. Supply `FNA3D_CNA_BAD_SYMBOL_FIXTURE` to register its OpenGL/SDLGPU CTests,
  or run `fna3d_cna_malformed_effect_tests <fixture>` with explicit `FNA3D_FORCE_DRIVER`. Markers
  surround compilation/disposal for an ASan/gdb trace. No FNA3D-7 production fix is implied.

Window-free tests, after a focused build, can run as:

    env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
      ctest --test-dir build -R '^fna3d_cna_(opengl_state|sdlgpu_command|pipeline_cache|sdlgpu_properties)$' --output-on-failure

The native tests require the owner's private GPU runner; a headless pass is not native-pixel
qualification. FNA3D-3 sample-mask support remains outside these repairs; presentation coverage
is described below.

### 2026-11-07 dependent texture dots

`texreg_tests.c` additionally exercises `texdp3` and `texdp3tex`, with and without
source `_bx2`, through every legal t0..t3 source/destination pair. Distinct per-stage
coordinates yield literal RGBA88/96/104 for TEXDP3, or zero after signed clamping.
TEXDP3TEX selects literal point-clamped lookup texels at (1,0), or (0,0) signed.
Every 64x32 pixel must match. These add 24 draws per driver to the existing 24
register-remapping draws, for 48 per driver across OpenGL, OpenGL ES 3 and SDL_GPU.
The old MojoShader pin passes both GLSL controls but refuses the 24 new SDL_GPU
programs. This is the public FNA3D Effect path, with no CNA adapter involved.

### Zero-result pixel observation (TQ-TEXREG-ZERO-01)

Native targets start with a distinct nonzero RGBA clear color. A signed TEXDP3
result of zero therefore requires an actual fragment write. A valid SPIR-V
control-flow mutation that kills only those fragments passed with the former
zero background; the nonzero background catches all six dropped-output cases.
All existing literal color expectations remain in place. The mutation is restored
byte-for-byte before the pristine native harness is run.


### Matrix depth witness (TQ-TEXM3X2DEPTH-01)

`fna3d_cna_texreg_tests` adds 48 depth comparison draws per driver to the 48
retained texture/dot draws. The public Effect path samples RGBA (32,96,160,224)
and supplies independent numerator/denominator rows. Known clear depths bracket
1/12, 191/252, and the mixed-modifier fractions 191/384 and 8/63. Zero denominator
must produce 1, checked with both LESS and GREATER against 15/16. The rasterized
depth is .9; the color clear is the nonzero (51,102,153,204) sentinel and expected
passing output is opaque magenta. Every 64x32 pixel is checked, including all
three legal source/PAD-stage pairs and all four modifier combinations. No
out-of-range native depth expectation is asserted.

`fna3d_cna_texreg_tests --depth-tokens-only` validates the exact 24 new native
program pairs through SPIR-V, GLSL 1.20 and GLSL ES 3 without initializing SDL
video or creating a device. It is a translation control, not a GPU witness.


The ES3 depth probes also exposed a driver API-selection defect: a loader may
return non-NULL desktop `glClearDepth`/`glDepthRange` stubs for an ES context.
ES3 must use `glClearDepthf`/`glDepthRangef` (ES 3.2 sections 15.2.3 and 12.6.1).
`fna3d_cna_opengl_state_tests depth` captures the actual driver boundary without
a device: all four ES/desktop and double-pointer availability combinations,
literal clear/range values, and redundant-state caching. Restoring either wrong
API choice fails its respective assertion. The native depth fractions, clear
brackets and color expectations remain unchanged.

### OpenGL presentation after native resize (TQ-FORK-GLPRESENT-01)

- `presentation_tests.c` compiles the actual OpenGL driver with only the native
  swap call wrapped. It inspects the window/default framebuffer just before the
  real swap, rather than reading the logical backbuffer. A 64x64 logical surface
  survives native resize to 160x96 without ResetBackbuffer, then presents into
  (32,0,96,96). Four independently painted quadrants check scaling and orientation;
  an asymmetric source crop and offset destination check xywh endpoints and both
  Y origins. NULL rectangles fill the actual drawable; a differently sized second
  window checks override selection and restoration. Both single-sample and 4x MSAA
  run on desktop OpenGL and ES3. Margins retain a separately seeded blue sentinel.
  Seed pixels are checked before presentation too. The override retains a dedicated
  context so SDL/GLX's implicit drawable and its sentinel storage survive the device
  context switching away; the presentation observer still uses the device context
  on the requested native window. No expectation or tolerance changes.
- The `presentation` group of `opengl_state_tests.c` runs without a display. It
  executes real backbuffer creation and SwapBuffers with captured GL/SDL calls,
  checking literal hand-derived endpoints, persistent logical allocation,
  equal-bound nearest MSAA resolve, scissor state and framebuffer restoration,
  and override window selection. Its oracle is FNA3D's public xywh contract,
  FNA GraphicsDevice.Present's unchanged forwarding, and SDLGPU's swapchain blit.

These supply fork coverage for FNA3D-8; native qualification must still run through
CNA's private GPU runner. A passing call-boundary test alone does not qualify pixels.
