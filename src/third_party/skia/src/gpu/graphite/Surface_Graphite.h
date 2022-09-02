/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_Surface_Graphite_DEFINED
#define skgpu_graphite_Surface_Graphite_DEFINED

#include "src/image/SkSurface_Base.h"

namespace skgpu::graphite {

class Context;
class Device;
class Recorder;

class Surface final : public SkSurface_Base {
public:
    Surface(sk_sp<Device>);
    ~Surface() override;

    Recorder* onGetRecorder() override;
    SkCanvas* onNewCanvas() override;
    sk_sp<SkSurface> onNewSurface(const SkImageInfo&) override;
    sk_sp<SkImage> onNewImageSnapshot(const SkIRect* subset) override;
    void onWritePixels(const SkPixmap&, int x, int y) override;
    bool onCopyOnWrite(ContentChangeMode) override;
    bool onReadPixels(Context*, Recorder*, const SkPixmap& dst, int srcX, int srcY);
    sk_sp<const SkCapabilities> onCapabilities() override;

#if GRAPHITE_TEST_UTILS && SK_SUPPORT_GPU
    // TODO: The long-term for the public API around surfaces and flushing/submitting will likely
    // be replaced with explicit control over Recorders and submitting Recordings to the Context
    // directly. For now, internal tools often rely on surface/canvas flushing to control what's
    // being timed (nanobench or viewer's stats layer), so we flush any pending draws to a DrawPass.
    // While this won't measure actual conversion of the task list to backend command buffers, that
    // should be fairly negligible since most of the work is handled in DrawPass::Make().
    // Additionally flushing pending work here ensures we don't batch across or clear prior recorded
    // work when looping in a benchmark, as the controlling code expects.
    GrSemaphoresSubmitted onFlush(BackendSurfaceAccess access,
                                  const GrFlushInfo&,
                                  const GrBackendSurfaceMutableState*) override;
#endif

private:
    sk_sp<Device> fDevice;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_Surface_Graphite_DEFINED
