// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_FUCHSIA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_FUCHSIA_H_

#include <memory>
#include <vector>

#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "ui/ozone/public/overlay_plane.h"

namespace ui {
class PlatformWindowSurface;
}  // namespace ui

namespace viz {

class VIZ_SERVICE_EXPORT OutputPresenterFuchsia : public OutputPresenter {
 public:
  static std::unique_ptr<OutputPresenterFuchsia> Create(
      ui::PlatformWindowSurface* window_surface,
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageFactory* shared_image_factory,
      gpu::SharedImageRepresentationFactory* representation_factory);

  OutputPresenterFuchsia(
      ui::PlatformWindowSurface* window_surface,
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageFactory* shared_image_factory,
      gpu::SharedImageRepresentationFactory* representation_factory);
  ~OutputPresenterFuchsia() override;

  // OutputPresenter implementation:
  void InitializeCapabilities(OutputSurface::Capabilities* capabilities) final;
  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               gfx::OverlayTransform transform) final;
  std::vector<std::unique_ptr<Image>> AllocateImages(
      gfx::ColorSpace color_space,
      gfx::Size image_size,
      size_t num_images) final;
  void SwapBuffers(SwapCompletionCallback completion_callback,
                   BufferPresentedCallback presentation_callback) final;
  void PostSubBuffer(const gfx::Rect& rect,
                     SwapCompletionCallback completion_callback,
                     BufferPresentedCallback presentation_callback) final;
  void CommitOverlayPlanes(SwapCompletionCallback completion_callback,
                           BufferPresentedCallback presentation_callback) final;
  void SchedulePrimaryPlane(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane,
      Image* image,
      bool is_submitted) final;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays,
                        std::vector<ScopedOverlayAccess*> accesses) final;

 private:
  struct PendingFrame {
    PendingFrame();
    ~PendingFrame();

    PendingFrame(PendingFrame&&);
    PendingFrame& operator=(PendingFrame&&);

    scoped_refptr<gfx::NativePixmap> native_pixmap;

    std::vector<gfx::GpuFenceHandle> acquire_fences;
    std::vector<gfx::GpuFenceHandle> release_fences;

    SwapCompletionCallback completion_callback;
    BufferPresentedCallback presentation_callback;

    // Vector of overlays that are associated with this frame.
    std::vector<ui::OverlayPlane> overlays;
  };

  void PresentNextFrame();

  ui::PlatformWindowSurface* const window_surface_;
  SkiaOutputSurfaceDependency* const dependency_;
  gpu::SharedImageFactory* const shared_image_factory_;
  gpu::SharedImageRepresentationFactory* const
      shared_image_representation_factory_;

  gfx::Size frame_size_;
  gfx::BufferFormat buffer_format_ = gfx::BufferFormat::RGBA_8888;

  // The next frame to be submitted by SwapBuffers().
  absl::optional<PendingFrame> next_frame_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_OUTPUT_PRESENTER_FUCHSIA_H_
