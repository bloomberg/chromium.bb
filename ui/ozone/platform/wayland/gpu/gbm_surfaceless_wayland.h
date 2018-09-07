// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_SURFACELESS_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_SURFACELESS_WAYLAND_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace ui {

class WaylandSurfaceFactory;

// A GLSurface for Wayland Ozone platform that uses surfaceless drawing. Drawing
// and displaying happens directly through NativePixmap buffers. CC would call
// into SurfaceFactoryOzone to allocate the buffers and then call
// ScheduleOverlayPlane(..) to schedule the buffer for presentation.
class GbmSurfacelessWayland : public gl::SurfacelessEGL {
 public:
  GbmSurfacelessWayland(WaylandSurfaceFactory* surface_factory,
                        gfx::AcceleratedWidget widget);

  void QueueOverlayPlane(OverlayPlane plane);

  // gl::GLSurface:
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            gl::GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  bool IsOffscreen() override;
  bool SupportsPresentationCallback() override;
  bool SupportsAsyncSwap() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                const PresentationCallback& callback) override;
  void SwapBuffersAsync(
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback) override;
  void PostSubBufferAsync(
      int x,
      int y,
      int width,
      int height,
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback) override;
  EGLConfig GetConfig() override;

 private:
  ~GbmSurfacelessWayland() override;

  struct PendingFrame {
    PendingFrame();
    ~PendingFrame();

    bool ScheduleOverlayPlanes(gfx::AcceleratedWidget widget);
    void Flush();

    bool ready = false;
    gfx::SwapResult swap_result = gfx::SwapResult::SWAP_FAILED;
    std::vector<gl::GLSurfaceOverlay> overlays;
    SwapCompletionCallback completion_callback;
    PresentationCallback presentation_callback;
  };

  void SubmitFrame();

  EGLSyncKHR InsertFence(bool implicit);
  void FenceRetired(PendingFrame* frame);

  void OnScheduleBufferSwapDone(gfx::SwapResult result);
  void OnSubmission(gfx::SwapResult result,
                    std::unique_ptr<gfx::GpuFence> out_fence);
  void OnPresentation(const gfx::PresentationFeedback& feedback);

  WaylandSurfaceFactory* surface_factory_;
  std::vector<OverlayPlane> planes_;

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  gfx::AcceleratedWidget widget_;
  std::vector<std::unique_ptr<PendingFrame>> unsubmitted_frames_;
  std::unique_ptr<PendingFrame> submitted_frame_;
  bool has_implicit_external_sync_;
  bool last_swap_buffers_result_ = true;

  base::WeakPtrFactory<GbmSurfacelessWayland> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfacelessWayland);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_SURFACELESS_WAYLAND_H_
