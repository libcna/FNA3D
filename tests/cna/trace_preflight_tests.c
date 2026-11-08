/* SPDX-License-Identifier: Zlib */
/* libcna device-free trace regression. The real public dispatch and trace writer
 * run against a counting driver; no SDL video or GPU is initialized.
 * Oracle: the legacy trace schema in replay.c has one device and no window IDs.
 */
#undef FNA3D_DRIVER_SDL
#undef FNA3D_DRIVER_OPENGL
#undef FNA3D_DRIVER_D3D11
#define FNA3D_DRIVER_SDL 0
#define FNA3D_DRIVER_OPENGL 0
#define FNA3D_DRIVER_D3D11 0
#define FNA3D_TRACING 1
#include "../../src/FNA3D.c"
#include "../../src/FNA3D_Tracing.c"
#include <stdio.h>

static int failures, creates, destroys, swaps, markers;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %d: %s\n", __LINE__, #c); failures++; } } while (0)
static FNA3D_Device devices[2];
static int refuseCreate;
static void Destroy(FNA3D_Device *device) { (void)device; destroys++; }
static void Swap(FNA3D_Renderer *r, FNA3D_Rect *s, FNA3D_Rect *d, void *w)
{ (void)r; (void)s; (void)d; (void)w; swaps++; }
static void Marker(FNA3D_Renderer *r, const char *s) { (void)r; (void)s; markers++; }
static FNA3D_Device *Create(FNA3D_PresentationParameters *p, uint8_t debug)
{
    FNA3D_Device *device = &devices[creates % 2];
    (void)p; (void)debug;
    creates++;
    if (refuseCreate) return NULL;
    device->DestroyDevice = Destroy;
    device->SwapBuffers = Swap;
    device->SetStringMarker = Marker;
    return device;
}
static const FNA3D_Driver driver = { "Counting", NULL, Create };
static void CheckBytes(const FNA3D_PresentationParameters *p)
{
    /* Independent schema oracle: mark 0 + nine parameters + debug, mark 56 +
     * length + "A\0", two mark-2 presents with absent rectangles, mark 1.
     * In particular there is no second creation, foreign present or device ID. */
    unsigned char expected[256], actual[256];
    size_t n = 0, size;
    FILE *file;
    uint8_t mark = 0, debug = 0, absent = 0;
    int32_t length = 2;
#define APPEND(value) do { memcpy(expected+n, &(value), sizeof(value)); n += sizeof(value); } while (0)
    APPEND(mark);
    APPEND(p->backBufferWidth); APPEND(p->backBufferHeight); APPEND(p->backBufferFormat);
    APPEND(p->multiSampleCount); APPEND(p->isFullScreen); APPEND(p->depthStencilFormat);
    APPEND(p->presentationInterval); APPEND(p->displayOrientation); APPEND(p->renderTargetUsage);
    APPEND(debug);
    mark = 56; APPEND(mark); APPEND(length); expected[n++] = 'A'; expected[n++] = 0;
    mark = 2;
    APPEND(mark); APPEND(absent); APPEND(absent);
    APPEND(mark); APPEND(absent); APPEND(absent);
    mark = 1; APPEND(mark);
#undef APPEND
    file = fopen("FNA3D_Trace.bin", "rb");
    CHECK(file != NULL);
    if (!file) return;
    size = fread(actual, 1, sizeof(actual), file);
    fclose(file);
    CHECK(size == n && memcmp(actual, expected, n) == 0);
}
int main(void)
{
    FNA3D_PresentationParameters p = {0}, q = {0};
    FNA3D_Device *a, *b;
    int windowA, windowB;
    p.backBufferWidth = 64; p.backBufferHeight = 32;
    p.deviceWindowHandle = &windowA;
    q = p; q.deviceWindowHandle = &windowB;
    drivers[0] = &driver; selectedDriver = 0;
    SDL_SetHint("FNA3D_DISABLE_TRACING", "0");
    refuseCreate = 1;
    CHECK(FNA3D_CreateDevice(&p, 0) == NULL);
    CHECK(traceDeviceCount == 0 && traceLock == NULL && traceBuffer == NULL);
    refuseCreate = 0;
    creates = 0;
    a = FNA3D_CreateDevice(&p, 0);
    b = FNA3D_CreateDevice(&q, 0);
    CHECK(a != NULL && b == NULL && creates == 1 && traceDeviceCount == 1);
    FNA3D_DestroyDevice(NULL);
    FNA3D_SetStringMarker(a, "A");
    FNA3D_SwapBuffers(a, NULL, NULL, &windowB);
    CHECK(swaps == 0);
    FNA3D_SwapBuffers(a, NULL, NULL, &windowA);
    FNA3D_SwapBuffers(a, NULL, NULL, NULL);
    CHECK(swaps == 2 && markers == 1);
    FNA3D_DestroyDevice(a);
    CHECK(destroys == 1 && traceDeviceCount == 0 && traceLock == NULL);
    CheckBytes(&p);
    /* Session restarts after successful or failed creation must release their lock/buffer. */
    a = FNA3D_CreateDevice(&q, 0);
    CHECK(a != NULL); FNA3D_DestroyDevice(a);
    SDL_SetHint("FNA3D_DISABLE_TRACING", "1");
    a = FNA3D_CreateDevice(&p, 0); b = FNA3D_CreateDevice(&q, 0);
    CHECK(a && b && a != b && traceDeviceCount == 2);
    FNA3D_SwapBuffers(a, NULL, NULL, &windowB);
    FNA3D_SwapBuffers(b, NULL, NULL, &windowA);
    CHECK(swaps == 4);
    FNA3D_DestroyDevice(a); FNA3D_DestroyDevice(b);
    CHECK(traceDeviceCount == 0 && traceLock == NULL && traceBuffer == NULL);
    remove("FNA3D_Trace.bin");
    SDL_Quit();
    printf("trace preflight: %d failures\n", failures);
    return failures ? 1 : 0;
}
