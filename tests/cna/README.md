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
  multisampled RGBA8 and sRGB targets must resolve to their clear colour. On OpenGL, reads with
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
  Registered only with `BUILD_SHARED_LIBS`, as only a shared FNA3D installs.
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
  captured calls. Under AddressSanitizer the replay runs with `detect_stack_use_after_return=1`,
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
