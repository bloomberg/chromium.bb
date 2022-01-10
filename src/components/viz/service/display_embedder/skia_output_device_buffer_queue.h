// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"

namespace viz {

class SkiaOutputSurfaceDependency;

class VIZ_SERVICE_EXPORT SkiaOutputDeviceBufferQueue : public SkiaOutputDevice {
 public:
  class OverlayData;

  SkiaOutputDeviceBufferQueue(
      std::unique_ptr<OutputPresenter> presenter,
      SkiaOutputSurfaceDependency* deps,
      gpu::SharedImageRepresentationFactory* representation_factory,
      gpu::MemoryTracker* memory_tracker,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
      bool needs_background_image,
      bool supports_non_backed_solid_color_images);

  ~SkiaOutputDeviceBufferQueue() override;

  SkiaOutputDeviceBufferQueue(const SkiaOutputDeviceBufferQueue&) = delete;
  SkiaOutputDeviceBufferQueue& operator=(const SkiaOutputDeviceBufferQueue&) =
      delete;

  // SkiaOutputDevice overrides.
  void Submit(bool sync_cpu, base::OnceClosure callback) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   OutputSurfaceFrame frame) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     OutputSurfaceFrame frame) override;
  void CommitOverlayPlanes(BufferPresentedCallback feedback,
                           OutputSurfaceFrame frame) override;
  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               gfx::OverlayTransform transform) override;
  SkSurface* BeginPaint(
      bool allocate_frame_buffer,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;
  bool AllocateFrameBuffers(size_t n) override;
  void ReleaseOneFrameBuffer() override;

  bool IsPrimaryPlaneOverlay() const override;
  void SchedulePrimaryPlane(
      const absl::optional<
          OverlayProcessorInterface::OutputSurfaceOverlayPlane>& plane)
      override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;

 private:
  friend class SkiaOutputDeviceBufferQueueTest;

  using CancelableSwapCompletionCallback =
      base::CancelableOnceCallback<void(gfx::SwapCompletionResult)>;

  OutputPresenter::Image* GetNextImage();
  void PageFlipComplete(OutputPresenter::Image* image,
                        gfx::GpuFenceHandle release_fence);
  void FreeAllSurfaces();
  // Used as callback for SwapBuffersAsync and PostSubBufferAsync to finish
  // operation
  void DoFinishSwapBuffers(const gfx::Size& size,
                           OutputSurfaceFrame frame,
                           const base::WeakPtr<OutputPresenter::Image>& image,
                           std::vector<gpu::Mailbox> overlay_mailboxes,
                           gfx::SwapCompletionResult result);

  gfx::Size GetSwapBuffersSize();
  bool RecreateImages();

  void MaybeAllocateBackgroundImages();
  void MaybeScheduleBackgroundImage();
  OutputPresenter::Image* GetNextBackgroundImage();

  std::unique_ptr<OutputPresenter> presenter_;

  scoped_refptr<gpu::SharedContextState> context_state_;
  const raw_ptr<gpu::SharedImageRepresentationFactory> representation_factory_;
  // Format of images
  gfx::ColorSpace color_space_;
  gfx::Size image_size_;
  gfx::OverlayTransform overlay_transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  // All allocated images.
  std::vector<std::unique_ptr<OutputPresenter::Image>> images_;
  // This image is currently used by Skia as RenderTarget. This may be nullptr
  // if there is no drawing for the current frame or if allocation failed.
  raw_ptr<OutputPresenter::Image> current_image_ = nullptr;
  // The last image submitted for presenting.
  raw_ptr<OutputPresenter::Image> submitted_image_ = nullptr;
  // The image currently on the screen, if any.
  raw_ptr<OutputPresenter::Image> displayed_image_ = nullptr;
  // These are free for use, and are not nullptr.
  base::circular_deque<OutputPresenter::Image*> available_images_;
  // These cancelable callbacks bind images that have been scheduled to display
  // but are not displayed yet. This deque will be cleared when represented
  // frames are destroyed. Use CancelableOnceCallback to prevent resources
  // from being destructed outside SkiaOutputDeviceBufferQueue life span.
  base::circular_deque<std::unique_ptr<CancelableSwapCompletionCallback>>
      swap_completion_callbacks_;
  // Mailboxes of scheduled overlays for the next SwapBuffers call.
  std::vector<gpu::Mailbox> pending_overlay_mailboxes_;
  // Mailboxes of committed overlays for the last SwapBuffers call.
  std::vector<gpu::Mailbox> committed_overlay_mailboxes_;

  class OverlayDataComparator {
   public:
    using is_transparent = void;
    bool operator()(const OverlayData& lhs, const OverlayData& rhs) const;
    bool operator()(const OverlayData& lhs, const gpu::Mailbox& rhs) const;
    bool operator()(const gpu::Mailbox& lhs, const OverlayData& rhs) const;
  };
  // A set for all overlays. The set uses overlay_data.mailbox() as the unique
  // key.
  base::flat_set<OverlayData, OverlayDataComparator> overlays_;

#if defined(USE_OZONE)
  const gpu::Mailbox GetImageMailboxForColor(const SkColor& color);

  // All in-flight solid color images are held in this container until a swap
  // buffer with the identifying mailbox releases them.
  base::flat_map<gpu::Mailbox,
                 std::pair<SkColor, std::unique_ptr<OutputPresenter::Image>>>
      solid_color_images_;

  std::unordered_multimap<SkColor, std::unique_ptr<OutputPresenter::Image>>
      solid_color_cache_;
#endif
  // Set to true if no image is to be used for the primary plane of this frame.
  bool current_frame_has_no_primary_plane_ = false;
  // Whether or not the platform needs occluded background images. Wayland needs
  // it for opaque accelerated widgets and event wiring. Please see details on
  // the number of background images below.
  bool needs_background_image_ = false;
  // 4x4 small images that will be scaled to cover an opaque region.
  // It's required to have two background images to be scheduled so that
  // Desktop Wayland compositors are able to apply state changes to root
  // surfaces. Otherwise, they unref the attached buffer after processing it
  // and never update the state changes of the root surface, which leads to
  // a broken resize opetion.
  std::vector<std::unique_ptr<OutputPresenter::Image>> background_images_;
  OutputPresenter::Image* current_background_image_ = nullptr;
  // Whether the platform supports non-backed solid color overlays. The Wayland
  // backend is able to delegate these overlays without buffer backings
  // depending on the availability of a certain protocol.
  bool supports_non_backed_solid_color_images_ = false;
  // Whether |SchedulePrimaryPlane| needs to wait for a paint before scheduling
  // This works around an edge case for unpromoting fullscreen quads.
  bool primary_plane_waiting_on_paint_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
