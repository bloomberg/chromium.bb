// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_OUTPUT_SURFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_OUTPUT_SURFACE_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "third_party/skia/include/core/SkRefCnt.h"

#if defined(OS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

#if defined(OS_MACOSX)
#include "components/viz/service/display/ca_layer_overlay.h"
#endif

class SkCanvas;
class SkImage;

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace viz {

class OverlayCandidate;
class ContextLostObserver;
class CopyOutputRequest;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// This class extends the OutputSurface for SkiaRenderer needs. In future, the
// SkiaRenderer will be the only renderer. When other renderers are removed,
// we will replace OutputSurface with SkiaOutputSurface, and remove all
// OutputSurface's methods which are not useful for SkiaRenderer.
class VIZ_SERVICE_EXPORT SkiaOutputSurface : public OutputSurface,
                                             public ExternalUseClient {
 public:
#if defined(OS_ANDROID)
  using OverlayList = std::vector<OverlayCandidate>;
#elif defined(OS_MACOSX)
  using OverlayList = CALayerOverlayList;
#elif defined(OS_WIN)
  using OverlayList = DCLayerOverlayList;
#elif defined(USE_OZONE)
  using OverlayList = std::vector<OverlayCandidate>;
#else
  // Default.
  using OverlayList = std::vector<OverlayCandidate>;
#endif

  explicit SkiaOutputSurface(OutputSurface::Type type);
  ~SkiaOutputSurface() override;

  SkiaOutputSurface* AsSkiaOutputSurface() override;

  // Begin painting the current frame. This method will create a
  // SkDeferredDisplayListRecorder and return a SkCanvas of it.
  // The SkiaRenderer will use this SkCanvas to paint the current
  // frame.
  // And this SkCanvas may become invalid, when FinishPaintCurrentFrame is
  // called.
  virtual SkCanvas* BeginPaintCurrentFrame() = 0;

  // Make a promise SkImage from the given |image_context|. The SkiaRenderer can
  // use the image with SkCanvas returned by |GetSkCanvasForCurrentFrame|, but
  // Skia will not read the content of the resource until the |sync_token| in
  // the |image_context| is satisfied. The SwapBuffers should take care of this
  // by scheduling a GPU task with all resource sync tokens recorded by
  // MakePromiseSkImage for the current frame.
  virtual void MakePromiseSkImage(
      ExternalUseClient::ImageContext* image_context) = 0;

  // Make a promise SkImage from the given |contexts| and |image_color_space|.
  // For YUV format, at least three resource contexts should be provided.
  // contexts[0] contains pixels from y panel, contexts[1] contains pixels
  // from u panel, contexts[2] contains pixels from v panel. For NV12 format,
  // at least two resource contexts should be provided. contexts[0] contains
  // pixels from y panel, contexts[1] contains pixels from u and v panels. If
  // has_alpha is true, the last item in contexts contains alpha panel.
  virtual sk_sp<SkImage> MakePromiseSkImageFromYUV(
      const std::vector<ExternalUseClient::ImageContext*>& contexts,
      sk_sp<SkColorSpace> image_color_space,
      bool has_alpha) = 0;

  // Called if SwapBuffers() will be skipped.
  virtual void SwapBuffersSkipped() = 0;

  // TODO(weiliangc): This API should move to OverlayProcessor.
  // Schedule |output_surface_plane| as an overlay plane to be displayed.
  virtual void ScheduleOutputSurfaceAsOverlay(
      OverlayProcessorInterface::OutputSurfaceOverlayPlane
          output_surface_plane) = 0;

  // Begin painting a render pass. This method will create a
  // SkDeferredDisplayListRecorder and return a SkCanvas of it. The SkiaRenderer
  // will use this SkCanvas to paint the render pass.
  // Note: BeginPaintRenderPass cannot be called without finishing the prior
  // paint render pass.
  virtual SkCanvas* BeginPaintRenderPass(const RenderPassId& id,
                                         const gfx::Size& size,
                                         ResourceFormat format,
                                         bool mipmap,
                                         sk_sp<SkColorSpace> color_space) = 0;

  // Finish painting the current frame or current render pass, depends on which
  // BeginPaint function is called last. This method will schedule a GPU task to
  // play the DDL back on GPU thread on a cached SkSurface. This method returns
  // a sync token which can be waited on in a command buffer to ensure the paint
  // operation is completed. This token is released when the GPU ops from
  // painting the render pass have been seen and processed by the GPU main.
  // Optionally the caller may specify |on_finished| callback to be called after
  // the GPU has finished processing all submitted commands. The callback may be
  // called on a different thread.
  virtual gpu::SyncToken SubmitPaint(base::OnceClosure on_finished) = 0;

  // Make a promise SkImage from a render pass id. The render pass has been
  // painted with BeginPaintRenderPass and FinishPaintRenderPass. The format
  // and mipmap must match arguments used for BeginPaintRenderPass() to paint
  // this render pass.
  virtual sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const RenderPassId& id,
      const gfx::Size& size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space) = 0;

  // Remove cached resources generated by BeginPaintRenderPass and
  // FinishPaintRenderPass.
  virtual void RemoveRenderPassResource(std::vector<RenderPassId> ids) = 0;

  // Copy the output of the current frame if the |id| is zero, otherwise copy
  // the output of a cached SkSurface for the given |id|.
  virtual void CopyOutput(RenderPassId id,
                          const copy_output::RenderPassGeometry& geometry,
                          const gfx::ColorSpace& color_space,
                          std::unique_ptr<CopyOutputRequest> request) = 0;

  // Schedule drawing overlays at next SwapBuffers() call. Waits on
  // |sync_tokens| for the overlay textures to be ready before scheduling.
  virtual void ScheduleOverlays(OverlayList overlays,
                                std::vector<gpu::SyncToken> sync_tokens) = 0;

#if defined(OS_WIN)
  // Enables/disables drawing with DC layers. Should be enabled before
  // ScheduleDCLayers() will be called.
  virtual void SetEnableDCLayers(bool enable) = 0;
#endif

  // Add context lost observer.
  virtual void AddContextLostObserver(ContextLostObserver* observer) = 0;

  // Remove context lost observer.
  virtual void RemoveContextLostObserver(ContextLostObserver* observer) = 0;

  // Only used for SkiaOutputSurfaceImpl unit tests.
  virtual void ScheduleGpuTaskForTesting(
      base::OnceClosure callback,
      std::vector<gpu::SyncToken> sync_tokens) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurface);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_OUTPUT_SURFACE_H_
