# libcna fork regression tests

Tests for defects fixed on this fork's `cna` branch, built only with `-DFNA3D_CNA_TESTS=ON`. They
open an SDL window and create a real device, so they need a display (in CNA's checkout:
`tools/platform/run_gpu_tests_private.sh --exec ctest --test-dir <build>`).

- `readback_tests.c`: `GetTextureData2D`/`GetTextureDataCube` write exactly the bytes a format and
  rectangle occupy -- every uncompressed format, odd widths, partial rectangles, non-square mip
  chains, cube faces, an attached render target -- into buffers with guard bytes behind them, and
  refuse a level, rectangle or destination that does not fit. Cube-face and volume uploads come
  from exactly sized allocations, so a read past the caller's data shows under AddressSanitizer;
  no driver has a volume readback, and each must leave the destination untouched. Build with
  `-DCMAKE_C_FLAGS="-fsanitize=address,undefined"` to have the sanitizers watch the driver's own
  temporary buffers as well.
- `dispose_tests.c`: a texture, two buffers, a renderbuffer and a query disposed from another
  thread wait for the next `SwapBuffers`; destroying the device first must still free them, and
  the device its own caches. The test counts live SDL allocations through `SDL_SetMemoryFunctions`
  around a device's lifetime, on OpenGL, OpenGL ES 3 and the SDL_GPU driver.
- `backbuffer_tests.c`: `ReadBackbuffer` returns the rectangle asked for, in the backbuffer's
  top-left origin. Every pixel is painted its own colour (row and half) by drawing through a
  scissor rectangle per row half, the scissor test left on as a draw leaves it; full and
  asymmetric partial reads must then hold exactly those colours in order and write nothing past
  `w * h * 4` bytes -- on a window-sized backbuffer, a smaller faux one and 4x multisampled ones,
  whose resolve the scissor must not clip.
- `test_effect.h`: the minimal Effect the drawing tests use, built in memory from the Effects
  Framework format (no Effect compiler runs here): a vs_2_0 that passes POSITION0 to `oPos` and
  its COLOR0 or DEPTH0 input to `oD0`, and a ps_2_0 that writes that colour, in one technique of
  up to four passes.
