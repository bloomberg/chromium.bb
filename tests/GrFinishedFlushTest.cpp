/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Test.h"

#include "GrContext.h"
#include "GrContextPriv.h"
#include "GrGpu.h"
#include "SkSurface.h"

using namespace sk_gpu_test;

static void testing_finished_proc(void* ctx) {
    int* count = (int*)ctx;
    *count += 1;
}

DEF_GPUTEST_FOR_RENDERING_CONTEXTS(FlushFinishedProcTest, reporter, ctxInfo) {
    GrContext* ctx = ctxInfo.grContext();

    SkImageInfo info =
            SkImageInfo::Make(8, 8, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(ctx, SkBudgeted::kNo, info);
    SkCanvas* canvas = surface->getCanvas();

    // We flush the surface first just to get rid of any discards/clears that got recorded from
    // making the surface.
    surface->flush();
    GrFlushInfo flushInfoSyncCpu;
    flushInfoSyncCpu.fFlags = kSyncCpu_GrFlushFlag;
    ctx->flush(flushInfoSyncCpu);

    int count = 0;

    GrFlushInfo flushInfoFinishedProc;
    flushInfoFinishedProc.fFinishedProc = testing_finished_proc;
    flushInfoFinishedProc.fFinishedContext = (void*)&count;
    // There is no work on the surface so flushing should immediately call the finished proc.
    surface->flush(SkSurface::BackendSurfaceAccess::kNoAccess, flushInfoFinishedProc);

    REPORTER_ASSERT(reporter, count == 1);

    canvas->clear(SK_ColorRED);

    surface->flush(SkSurface::BackendSurfaceAccess::kNoAccess, flushInfoFinishedProc);

    bool isVulkan = ctx->backend() == GrBackendApi::kVulkan;
    if (isVulkan) {
        // On Vulkan the command buffer we just submitted may or may not have finished immediately
        // so the finish proc may not have been called.
        REPORTER_ASSERT(reporter, count == 1 || count == 2);
    } else {
        REPORTER_ASSERT(reporter, count == 2);
    }
    ctx->flush(flushInfoSyncCpu);
    REPORTER_ASSERT(reporter, count == 2);

    // Test flushing via the GrContext
    canvas->clear(SK_ColorBLUE);
    ctx->flush(flushInfoFinishedProc);
    if (isVulkan) {
        // On Vulkan the command buffer we just submitted may or may not have finished immediately
        // so the finish proc may not have been called.
        REPORTER_ASSERT(reporter, count == 2 || count == 3);
    } else {
        REPORTER_ASSERT(reporter, count == 3);
    }
    ctx->flush(flushInfoSyncCpu);
    REPORTER_ASSERT(reporter, count == 3);

    // There is no work on the surface so flushing should immediately call the finished proc.
    ctx->flush(flushInfoFinishedProc);
    REPORTER_ASSERT(reporter, count == 4);

    count = 0;
    int count2 = 0;
    canvas->clear(SK_ColorGREEN);
    surface->flush(SkSurface::BackendSurfaceAccess::kNoAccess, flushInfoFinishedProc);
    // There is no work to be flushed here so this will return immediately, but make sure the
    // finished call from this proc isn't called till the previous surface flush also is finished.
    flushInfoFinishedProc.fFinishedContext = (void*)&count2;
    ctx->flush(flushInfoFinishedProc);

    REPORTER_ASSERT(reporter, count == count2);

    ctx->flush(flushInfoSyncCpu);

    REPORTER_ASSERT(reporter, count == 1);
    REPORTER_ASSERT(reporter, count == count2);
}

