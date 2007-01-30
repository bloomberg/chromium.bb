
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <drm/drm.h>
#include "xf86dri.h"
#include "xf86drm.h"
#include "stdio.h"
#include "sys/types.h"
#include <unistd.h>
#include <string.h>
#include "sys/mman.h"

typedef struct
{
    enum
    {
	haveNothing,
	haveDisplay,
	haveConnection,
	haveDriverName,
	haveDeviceInfo,
	haveDRM,
	haveContext
    }
    state;

    Display *display;
    int screen;
    drm_handle_t sAreaOffset;
    char *curBusID;
    char *driverName;
    int drmFD;
    XVisualInfo visualInfo;
    XID id;
    drm_context_t hwContext;
    void *driPriv;
    int driPrivSize;
    int fbSize;
    int fbOrigin;
    int fbStride;
    drm_handle_t fbHandle;
    int ddxDriverMajor;
    int ddxDriverMinor;
    int ddxDriverPatch;
} TinyDRIContext;

#ifndef __x86_64__
static unsigned
fastrdtsc(void)
{
    unsigned eax;
    __asm__ volatile ("\t"
	"pushl  %%ebx\n\t"
	"cpuid\n\t" ".byte 0x0f, 0x31\n\t" "popl %%ebx\n":"=a" (eax)
	:"0"(0)
	:"ecx", "edx", "cc");

    return eax;
}
#else
static unsigned
fastrdtsc(void)
{
    unsigned eax;
    __asm__ volatile ("\t"
	"cpuid\n\t" ".byte 0x0f, 0x31\n\t" :"=a" (eax)
	:"0"(0)
		      :"ecx", "edx", "ebx", "cc");

    return eax;
}
#endif

static unsigned
time_diff(unsigned t, unsigned t2)
{
    return ((t < t2) ? t2 - t : 0xFFFFFFFFU - (t - t2 - 1));
}

static int
releaseContext(TinyDRIContext * ctx)
{
    switch (ctx->state) {
    case haveContext:
	uniDRIDestroyContext(ctx->display, ctx->screen, ctx->id);
    case haveDRM:
	drmClose(ctx->drmFD);
    case haveDeviceInfo:
	XFree(ctx->driPriv);
    case haveDriverName:
	XFree(ctx->driverName);
    case haveConnection:
	XFree(ctx->curBusID);
	uniDRICloseConnection(ctx->display, ctx->screen);
    case haveDisplay:
	XCloseDisplay(ctx->display);
    default:
	break;
    }
    return -1;
}

static void
testAGP(TinyDRIContext * ctx)
{
}

int
main()
{
    int ret, screen, isCapable;
    char *displayName = ":0";
    TinyDRIContext ctx;
    unsigned magic;

    ctx.screen = 0;
    ctx.state = haveNothing;
    ctx.display = XOpenDisplay(displayName);
    if (!ctx.display) {
	fprintf(stderr, "Could not open display\n");
	return releaseContext(&ctx);
    }
    ctx.state = haveDisplay;

    ret =
	uniDRIQueryDirectRenderingCapable(ctx.display, ctx.screen,
	&isCapable);
    if (!ret || !isCapable) {
	fprintf(stderr, "No DRI on this display:sceen\n");
	return releaseContext(&ctx);
    }

    if (!uniDRIOpenConnection(ctx.display, ctx.screen, &ctx.sAreaOffset,
	    &ctx.curBusID)) {
	fprintf(stderr, "Could not open DRI connection.\n");
	return releaseContext(&ctx);
    }
    ctx.state = haveConnection;

    if (!uniDRIGetClientDriverName(ctx.display, ctx.screen,
	    &ctx.ddxDriverMajor, &ctx.ddxDriverMinor,
	    &ctx.ddxDriverPatch, &ctx.driverName)) {
	fprintf(stderr, "Could not get DRI driver name.\n");
	return releaseContext(&ctx);
    }
    ctx.state = haveDriverName;

    if (!uniDRIGetDeviceInfo(ctx.display, ctx.screen,
	    &ctx.fbHandle, &ctx.fbOrigin, &ctx.fbSize,
	    &ctx.fbStride, &ctx.driPrivSize, &ctx.driPriv)) {
	fprintf(stderr, "Could not get DRI device info.\n");
	return releaseContext(&ctx);
    }
    ctx.state = haveDriverName;

    if ((ctx.drmFD = drmOpen(NULL, ctx.curBusID)) < 0) {
	perror("DRM Device could not be opened");
	return releaseContext(&ctx);
    }
    ctx.state = haveDRM;

    drmGetMagic(ctx.drmFD, &magic);
    if (!uniDRIAuthConnection(ctx.display, ctx.screen, magic)) {
	fprintf(stderr, "Could not get X server to authenticate us.\n");
	return releaseContext(&ctx);
    }

    ret = XMatchVisualInfo(ctx.display, ctx.screen, 24, TrueColor,
	&ctx.visualInfo);
    if (!ret) {
	ret = XMatchVisualInfo(ctx.display, ctx.screen, 16, TrueColor,
	    &ctx.visualInfo);
	if (!ret) {
	    fprintf(stderr, "Could not find a matching visual.\n");
	    return releaseContext(&ctx);
	}
    }

    if (!uniDRICreateContext(ctx.display, ctx.screen, ctx.visualInfo.visual,
	    &ctx.id, &ctx.hwContext)) {
	fprintf(stderr, "Could not create DRI context.\n");
	return releaseContext(&ctx);
    }
    ctx.state = haveContext;

    testAGP(&ctx);

    releaseContext(&ctx);
    printf("Terminating normally\n");
    return 0;
}
