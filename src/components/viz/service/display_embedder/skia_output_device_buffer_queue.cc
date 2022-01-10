// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include <iterator>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/cxx20_erase.h"
#include "base/debug/alias.h"
#include "build/build_config.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

namespace {
base::TimeTicks g_last_reshape_failure = base::TimeTicks();

NOINLINE void CheckForLoopFailuresBufferQueue() {
  const auto threshold = base::Seconds(1);
  auto now = base::TimeTicks::Now();
  if (!g_last_reshape_failure.is_null() &&
      now - g_last_reshape_failure < threshold) {
    CHECK(false);
  }
  g_last_reshape_failure = now;
}

// See |needs_background_image| for details.
constexpr size_t kNumberOfBackgroundImages = 2u;
constexpr gfx::Size kBackgroundImageSize(4, 4);

}  // namespace

namespace viz {

class SkiaOutputDeviceBufferQueue::OverlayData {
 public:
  OverlayData() = default;

  OverlayData(
      std::unique_ptr<gpu::SharedImageRepresentationOverlay> representation,
      std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
          scoped_read_access)
      : representation_(std::move(representation)),
        scoped_read_access_(std::move(scoped_read_access)),
        ref_(1) {
    DCHECK(representation_);
    DCHECK(scoped_read_access_);
  }

  OverlayData(OverlayData&& other) { *this = std::move(other); }

  ~OverlayData() { Reset(); }

  OverlayData& operator=(OverlayData&& other) {
    DCHECK(!IsInUseByWindowServer());
    DCHECK(!ref_);
    DCHECK(!scoped_read_access_);
    DCHECK(!representation_);
    scoped_read_access_ = std::move(other.scoped_read_access_);
    representation_ = std::move(other.representation_);
    ref_ = other.ref_;
    other.ref_ = 0;
    return *this;
  }

  bool IsInUseByWindowServer() const {
    if (!scoped_read_access_)
      return false;
    auto* gl_image = scoped_read_access_->gl_image();
    if (!gl_image)
      return false;
    return gl_image->IsInUseByWindowServer();
  }

  void Ref() { ++ref_; }

  void Unref() {
    DCHECK_GT(ref_, 0);
    if (ref_ > 1) {
      --ref_;
    } else if (ref_ == 1) {
      DCHECK(!IsInUseByWindowServer());
      Reset();
    }
  }

  void OnContextLost() { representation_->OnContextLost(); }

  bool unique() const { return ref_ == 1; }
  const gpu::Mailbox& mailbox() const { return representation_->mailbox(); }
  gpu::SharedImageRepresentationOverlay::ScopedReadAccess* scoped_read_access()
      const {
    return scoped_read_access_.get();
  }

 private:
  void Reset() {
    scoped_read_access_.reset();
    representation_.reset();
    ref_ = 0;
  }

  std::unique_ptr<gpu::SharedImageRepresentationOverlay> representation_;
  std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
      scoped_read_access_;
  int ref_ = 0;
};

SkiaOutputDeviceBufferQueue::SkiaOutputDeviceBufferQueue(
    std::unique_ptr<OutputPresenter> presenter,
    SkiaOutputSurfaceDependency* deps,
    gpu::SharedImageRepresentationFactory* representation_factory,
    gpu::MemoryTracker* memory_tracker,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    bool needs_background_image,
    bool supports_non_backed_solid_color_images)
    : SkiaOutputDevice(deps->GetSharedContextState()->gr_context(),
                       memory_tracker,
                       did_swap_buffer_complete_callback),
      presenter_(std::move(presenter)),
      context_state_(deps->GetSharedContextState()),
      representation_factory_(representation_factory),
      needs_background_image_(needs_background_image),
      supports_non_backed_solid_color_images_(
          supports_non_backed_solid_color_images) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.preserve_buffer_content = true;
  capabilities_.only_invalidates_damage_rect = false;
  capabilities_.number_of_buffers = 3;
#if defined(OS_ANDROID)
  if (::features::IncreaseBufferCountForHighFrameRate()) {
    capabilities_.number_of_buffers = 5;
  }
#endif
  capabilities_.orientation_mode = OutputSurface::OrientationMode::kHardware;

  // Force the number of max pending frames to one when the switch
  // "double-buffer-compositing" is passed.
  // This will keep compositing in double buffered mode assuming |buffer_queue|
  // allocates at most one additional buffer.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDoubleBufferCompositing))
    capabilities_.number_of_buffers = 2;
  capabilities_.pending_swap_params.max_pending_swaps =
      capabilities_.number_of_buffers - 1;
#if defined(OS_ANDROID)
  if (::features::IncreaseBufferCountForHighFrameRate() &&
      capabilities_.number_of_buffers == 5) {
    capabilities_.pending_swap_params.max_pending_swaps = 2;
    capabilities_.pending_swap_params.max_pending_swaps_90hz = 3;
    capabilities_.pending_swap_params.max_pending_swaps_120hz = 4;
  }
#endif
  DCHECK_LT(capabilities_.pending_swap_params.max_pending_swaps,
            capabilities_.number_of_buffers);
  DCHECK_LT(
      capabilities_.pending_swap_params.max_pending_swaps_90hz.value_or(0),
      capabilities_.number_of_buffers);
  DCHECK_LT(
      capabilities_.pending_swap_params.max_pending_swaps_120hz.value_or(0),
      capabilities_.number_of_buffers);

  presenter_->InitializeCapabilities(&capabilities_);

  if (capabilities_.supports_post_sub_buffer)
    capabilities_.supports_target_damage = true;
}

SkiaOutputDeviceBufferQueue::~SkiaOutputDeviceBufferQueue() {
  // TODO(vasilyt): We should not need this when we stop using
  // SharedImageBackingGLImage.
  if (context_state_->context_lost()) {
    for (auto& overlay : overlays_) {
      overlay.OnContextLost();
    }

    for (auto& image : images_) {
      image->OnContextLost();
    }
  }

  FreeAllSurfaces();
  // Clear and cancel swap_completion_callbacks_ to free all resource bind to
  // callbacks.
  swap_completion_callbacks_.clear();
}

OutputPresenter::Image* SkiaOutputDeviceBufferQueue::GetNextImage() {
  CHECK(!available_images_.empty());
  auto* image = available_images_.front();
  available_images_.pop_front();
  return image;
}

void SkiaOutputDeviceBufferQueue::PageFlipComplete(
    OutputPresenter::Image* image,
    gfx::GpuFenceHandle release_fence) {
  if (displayed_image_) {
    DCHECK_EQ(displayed_image_->skia_representation()->size(), image_size_);
    DCHECK_EQ(displayed_image_->GetPresentCount() > 1,
              displayed_image_ == image);
    // MakeCurrent is necessary for inserting release fences and for
    // BeginWriteSkia below.
    context_state_->MakeCurrent(/*surface=*/nullptr);
    displayed_image_->EndPresent(std::move(release_fence));
    if (!displayed_image_->GetPresentCount()) {
      available_images_.push_back(displayed_image_);
      // Call BeginWriteSkia() for the next frame here to avoid some expensive
      // operations on the critical code path.
      if (!available_images_.front()->sk_surface()) {
        // BeginWriteSkia() may alter GL's state.
        context_state_->set_need_context_state_reset(true);
        available_images_.front()->BeginWriteSkia();
      }
    }
  }

  displayed_image_ = image;
  swap_completion_callbacks_.pop_front();
}

void SkiaOutputDeviceBufferQueue::FreeAllSurfaces() {
  images_.clear();
  current_image_ = nullptr;
  submitted_image_ = nullptr;
  displayed_image_ = nullptr;
  available_images_.clear();
  primary_plane_waiting_on_paint_ = true;
}

bool SkiaOutputDeviceBufferQueue::IsPrimaryPlaneOverlay() const {
  return true;
}

void SkiaOutputDeviceBufferQueue::SchedulePrimaryPlane(
    const absl::optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>&
        plane) {
  // See |needs_background_image|.
  MaybeScheduleBackgroundImage();

  if (plane) {
    // If the current_image_ is nullptr, it means there is no change on the
    // primary plane. So we just need to schedule the last submitted image.
    auto* image =
        current_image_ ? current_image_.get() : submitted_image_.get();
    // |image| can be null if there was a fullscreen overlay last frame (e.g.
    // no primary plane). If the fullscreen quad suddenly fails the fullscreen
    // overlay check this frame (e.g. TestPageFlip failing) and then gets
    // promoted via a different strategy like single-on-top, the quad's damage
    // is still removed from the primary plane's damage. With no damage, we
    // never invoke |BeginPaint| which initializes a new image. Since there
    // still really isn't any primary plane content, it's fine to early-exit.
    if (!image && primary_plane_waiting_on_paint_)
      return;
    DCHECK(image);

    image->BeginPresent();
    presenter_->SchedulePrimaryPlane(plane.value(), image,
                                     image == submitted_image_);
  } else {
    primary_plane_waiting_on_paint_ =  true;
    current_frame_has_no_primary_plane_ = true;
    // Even if there is no primary plane, |current_image_| may be non-null if
    // an overlay just transitioned from an underlay strategy to a fullscreen
    // strategy (e.g. a the media controls disappearing on a fullscreen video).
    // In this case, there is still damage which triggers a render pass, but
    // since we promote via fullscreen, we remove the primary plane in the end.
    // We need to recycle |current_image_| to avoid a use-after-free error.
    if (current_image_) {
      available_images_.push_back(current_image_);
      current_image_ = nullptr;
    }
  }
}

#if defined(USE_OZONE)
const gpu::Mailbox SkiaOutputDeviceBufferQueue::GetImageMailboxForColor(
    const SkColor& color) {
  // Currently the Wayland protocol does not have protocol to support solid
  // color quads natively as surfaces. Here we create tiny 4x4 image buffers
  // in the color space of the frame buffer and clear them to the quad's solid
  // color. These freshly created buffers are then treated like any other
  // overlay via the mailbox interface.
  std::unique_ptr<OutputPresenter::Image> solid_color = nullptr;
  // First try for an existing same color image.
  auto it = solid_color_cache_.find(color);
  if (it != solid_color_cache_.end()) {
    // This is a prefect color match so use this directly.
    solid_color = std::move(it->second);
    solid_color_cache_.erase(it);
  } else {
    // Try to reuse an existing image even if the color is different.
    // Only do this if there are more cached images than those in flight (a
    // sensible upper bound).
    if (!solid_color_cache_.empty() &&
        solid_color_cache_.size() > solid_color_images_.size()) {
      auto it = solid_color_cache_.begin();
      solid_color = std::move(it->second);
      solid_color_cache_.erase(it);
    } else {
      // Worst case allocate a new image. This definitely will occur on startup.
      solid_color =
          presenter_->AllocateSingleImage(color_space_, gfx::Size(4, 4));
    }
    solid_color->BeginWriteSkia();
    solid_color->sk_surface()->getCanvas()->clear(color);
    solid_color->EndWriteSkia(/*force_flush*/ true);
  }
  DCHECK(solid_color);
  auto image_mailbox = solid_color->skia_representation()->mailbox();
  solid_color_images_.insert(std::make_pair(
      image_mailbox, std::make_pair(color, std::move(solid_color))));
  return image_mailbox;
}

#endif
void SkiaOutputDeviceBufferQueue::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  DCHECK(pending_overlay_mailboxes_.empty());
  std::vector<OutputPresenter::ScopedOverlayAccess*> accesses(overlays.size());
  for (size_t i = 0; i < overlays.size(); ++i) {
    auto& overlay = overlays[i];

#if defined(USE_OZONE)
    if (overlay.solid_color.has_value()) {
      // TODO(msisov): reconsider this once Linux Wayland compositors also
      // support that. See https://bit.ly/2ZqUO0w.
      if (!supports_non_backed_solid_color_images_) {
        overlay.mailbox = GetImageMailboxForColor(overlay.solid_color.value());
      } else {
        accesses[i] = nullptr;
        continue;
      }
    }
#endif

    if (!overlay.mailbox.IsSharedImage())
      continue;

    auto it = overlays_.find(overlay.mailbox);
    if (it != overlays_.end()) {
      // If the overlay is in |overlays_|, we will reuse it, and a ref will be
      // added to keep it alive. This ref will be removed, when the overlay is
      // replaced by a new frame.
      it->Ref();
      accesses[i] = it->scoped_read_access();
      pending_overlay_mailboxes_.emplace_back(overlay.mailbox);
      continue;
    }

    auto shared_image =
        representation_factory_->ProduceOverlay(overlay.mailbox);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image) {
      LOG(ERROR) << "Invalid mailbox.";
      continue;
    }

    // Fuchsia does not provide a GLImage overlay.
#if defined(OS_FUCHSIA)
    const bool needs_gl_image = false;
#else
    const bool needs_gl_image = true;
#endif  // defined(OS_FUCHSIA)

    // TODO(penghuang): do not depend on GLImage.
    auto shared_image_access =
        shared_image->BeginScopedReadAccess(needs_gl_image);
    if (!shared_image_access) {
      LOG(ERROR) << "Could not access SharedImage for read.";
      continue;
    }

    // TODO(penghuang): do not depend on GLImage.
    DLOG_IF(FATAL, needs_gl_image && !shared_image_access->gl_image())
        << "Cannot get GLImage.";

    bool result;
    std::tie(it, result) = overlays_.emplace(std::move(shared_image),
                                             std::move(shared_image_access));
    DCHECK(result);
    DCHECK(it->unique());

    // Add an extra ref to keep it alive. This extra ref will be removed when
    // the backing is not used by system compositor anymore.
    it->Ref();
    accesses[i] = it->scoped_read_access();
    pending_overlay_mailboxes_.emplace_back(overlay.mailbox);
  }

  presenter_->ScheduleOverlays(std::move(overlays), std::move(accesses));
}

void SkiaOutputDeviceBufferQueue::Submit(bool sync_cpu,
                                         base::OnceClosure callback) {
  // The current image may be missing, for example during WebXR presentation.
  // The SkSurface may also be missing due to a rare edge case (seen at ~1CPM
  // on CrOS)- if we end up skipping the swap for a frame and don't have
  // damage in the next frame (e.g.fullscreen overlay),
  // |current_image_->BeginWriteSkia| won't get called before |Submit|. In
  // this case, we shouldn't call |PreGrContextSubmit| since there's no active
  // surface to flush.
  if (current_image_ && current_image_->sk_surface())
    current_image_->PreGrContextSubmit();

  SkiaOutputDevice::Submit(sync_cpu, std::move(callback));
}

void SkiaOutputDeviceBufferQueue::SwapBuffers(BufferPresentedCallback feedback,
                                              OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  if (current_frame_has_no_primary_plane_) {
    DCHECK(!current_image_);
    submitted_image_ = nullptr;
    current_frame_has_no_primary_plane_ = false;
  } else {
    DCHECK(current_image_);
    submitted_image_ = current_image_;
    current_image_ = nullptr;
  }

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), GetSwapBuffersSize(), std::move(frame),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlay_mailboxes_))));
  committed_overlay_mailboxes_.clear();

  presenter_->SwapBuffers(swap_completion_callbacks_.back()->callback(),
                          std::move(feedback));
  std::swap(committed_overlay_mailboxes_, pending_overlay_mailboxes_);
}

void SkiaOutputDeviceBufferQueue::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  if (current_frame_has_no_primary_plane_) {
    DCHECK(!current_image_);
    submitted_image_ = nullptr;
    current_frame_has_no_primary_plane_ = false;
  } else {
    if (current_image_) {
      submitted_image_ = current_image_;
      current_image_ = nullptr;
    }
    DCHECK(submitted_image_);
  }

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), GetSwapBuffersSize(), std::move(frame),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlay_mailboxes_))));
  committed_overlay_mailboxes_.clear();

  presenter_->PostSubBuffer(rect, swap_completion_callbacks_.back()->callback(),
                            std::move(feedback));
  std::swap(committed_overlay_mailboxes_, pending_overlay_mailboxes_);
}

void SkiaOutputDeviceBufferQueue::CommitOverlayPlanes(
    BufferPresentedCallback feedback,
    OutputSurfaceFrame frame) {
  StartSwapBuffers({});

  // There is no drawing for this frame on the main buffer.
  DCHECK(!current_image_);
  if (current_frame_has_no_primary_plane_) {
    submitted_image_ = nullptr;
    current_frame_has_no_primary_plane_ = false;
  } else {
    DCHECK(submitted_image_);
  }

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), GetSwapBuffersSize(), std::move(frame),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlay_mailboxes_))));
  committed_overlay_mailboxes_.clear();

  presenter_->CommitOverlayPlanes(swap_completion_callbacks_.back()->callback(),
                                  std::move(feedback));
  std::swap(committed_overlay_mailboxes_, pending_overlay_mailboxes_);
}

void SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers(
    const gfx::Size& size,
    OutputSurfaceFrame frame,
    const base::WeakPtr<OutputPresenter::Image>& image,
    std::vector<gpu::Mailbox> overlay_mailboxes,
    gfx::SwapCompletionResult result) {
  // |overlay_mailboxes| are for overlays used by previous frame, they should
  // have been replaced.
  for (const auto& mailbox : overlay_mailboxes) {
    auto it = overlays_.find(mailbox);
    DCHECK(it != overlays_.end());
    if (!result.release_fence.is_null())
      it->scoped_read_access()->SetReleaseFence(result.release_fence.Clone());

    it->Unref();
  }

#if defined(USE_OZONE)
  std::set<gpu::Mailbox> released_solid_color_overlays;
  for (const auto& mailbox : overlay_mailboxes) {
    auto it = solid_color_images_.find(mailbox);
    if (it != solid_color_images_.end()) {
      released_solid_color_overlays.insert(mailbox);
      solid_color_cache_.insert(
          std::make_pair(it->second.first, std::move(it->second.second)));
      solid_color_images_.erase(it);
    }
  }
#endif

  // Code below can destroy last representation of the overlay shared image. On
  // MacOS it needs context to be current.
#if defined(OS_APPLE)
  // TODO(vasilyt): We shouldn't need this after we stop using
  // SharedImageBackingGLImage as backing.
  if (!context_state_->MakeCurrent(nullptr)) {
    for (auto& overlay : overlays_) {
      overlay.OnContextLost();
    }
  }
#endif

  std::vector<gpu::Mailbox> released_overlays;
  auto on_overlay_release =
#if defined(OS_APPLE)
      [&released_overlays](const OverlayData& overlay) {
        // Right now, only macOS needs to return maliboxes of released
        // overlays, so SkiaRenderer can unlock resources for them.
        released_overlays.push_back(overlay.mailbox());
      };
#elif defined(USE_OZONE)
      [&released_overlays,
       &released_solid_color_overlays](const OverlayData& overlay) {
        // Delegated compositing on Ozone needs to return mailboxes of released
        // overlays, so SkiaRenderer can unlock resources for them. However, the
        // solid color buffers originating in this class and should not
        // propagate up to SkiaRenderer.
        if (released_solid_color_overlays.find(overlay.mailbox()) ==
            released_solid_color_overlays.end()) {
          released_overlays.push_back(overlay.mailbox());
        }
      };
#else
      [](const OverlayData& overlay) {};
#endif

  // Go through backings of all overlays, and release overlay backings which are
  // not used.
  base::EraseIf(overlays_, [&on_overlay_release](auto& overlay) {
    if (!overlay.unique())
      return false;
    if (overlay.IsInUseByWindowServer())
      return false;
    on_overlay_release(overlay);
    overlay.Unref();
    return true;
  });

  bool should_reallocate =
      result.swap_result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS;

  const auto& mailbox =
      image ? image->skia_representation()->mailbox() : gpu::Mailbox();
  auto release_fence = result.release_fence.Clone();
  FinishSwapBuffers(std::move(result), size, std::move(frame),
                    /*damage_area=*/absl::nullopt, std::move(released_overlays),
                    mailbox);
  PageFlipComplete(image.get(), std::move(release_fence));

  if (should_reallocate)
    RecreateImages();
}

gfx::Size SkiaOutputDeviceBufferQueue::GetSwapBuffersSize() {
  switch (overlay_transform_) {
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return gfx::Size(image_size_.height(), image_size_.width());
    case gfx::OVERLAY_TRANSFORM_INVALID:
    case gfx::OVERLAY_TRANSFORM_NONE:
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return image_size_;
  }
}

bool SkiaOutputDeviceBufferQueue::Reshape(const gfx::Size& size,
                                          float device_scale_factor,
                                          const gfx::ColorSpace& color_space,
                                          gfx::BufferFormat format,
                                          gfx::OverlayTransform transform) {
  DCHECK(pending_overlay_mailboxes_.empty());
  if (!presenter_->Reshape(size, device_scale_factor, color_space, format,
                           transform)) {
    LOG(ERROR) << "Failed to resize.";
    CheckForLoopFailuresBufferQueue();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
    return false;
  }

  overlay_transform_ = transform;

  // See |needs_background_image|.
  MaybeAllocateBackgroundImages();

  if (color_space_ == color_space && image_size_ == size)
    return true;
  color_space_ = color_space;
  image_size_ = size;

  bool success = RecreateImages();
  if (!success) {
    CheckForLoopFailuresBufferQueue();
    // To prevent tail call, so we can see the stack.
    base::debug::Alias(nullptr);
  }
  return success;
}

bool SkiaOutputDeviceBufferQueue::RecreateImages() {
  size_t existing_number_of_buffers = images_.size();
  FreeAllSurfaces();
  size_t number_to_allocate = capabilities_.use_dynamic_frame_buffer_allocation
                                  ? existing_number_of_buffers
                                  : capabilities_.number_of_buffers;
  if (!number_to_allocate)
    return true;

  images_ =
      presenter_->AllocateImages(color_space_, image_size_, number_to_allocate);
  for (auto& image : images_) {
    available_images_.push_back(image.get());
  }

  DCHECK(images_.empty() || images_.size() == number_to_allocate);
  return !images_.empty();
}

void SkiaOutputDeviceBufferQueue::MaybeAllocateBackgroundImages() {
  if (!needs_background_image_ || !background_images_.empty())
    return;

  background_images_ = presenter_->AllocateImages(
      color_space_, kBackgroundImageSize, kNumberOfBackgroundImages);
  DCHECK(!background_images_.empty());

  // Clear the background images to avoid undesired artifacts.
  for (auto& image : background_images_) {
    image->BeginWriteSkia();
    image->sk_surface()->getCanvas()->clear(SkColors::kTransparent);
    image->EndWriteSkia(/*force_flush*/ true);
  }
}

void SkiaOutputDeviceBufferQueue::MaybeScheduleBackgroundImage() {
  if (!needs_background_image_)
    return;

  if (current_background_image_)
    current_background_image_->EndPresent({});
  current_background_image_ = GetNextBackgroundImage();
  current_background_image_->BeginPresent();
  presenter_->ScheduleBackground(current_background_image_);
}

OutputPresenter::Image* SkiaOutputDeviceBufferQueue::GetNextBackgroundImage() {
  DCHECK_EQ(background_images_.size(), kNumberOfBackgroundImages);
  return current_background_image_ == background_images_.front().get()
             ? background_images_.back().get()
             : background_images_.front().get();
}

SkSurface* SkiaOutputDeviceBufferQueue::BeginPaint(
    bool allocate_frame_buffer,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!capabilities_.use_dynamic_frame_buffer_allocation)
    DCHECK(!allocate_frame_buffer);

  primary_plane_waiting_on_paint_ = false;

  if (allocate_frame_buffer) {
    DCHECK(!current_image_);
    if (!AllocateFrameBuffers(1u))
      return nullptr;
  }
  if (!current_image_) {
    current_image_ = GetNextImage();
  }

  if (!current_image_->sk_surface())
    current_image_->BeginWriteSkia();
  *end_semaphores = current_image_->TakeEndWriteSkiaSemaphores();
  return current_image_->sk_surface();
}

void SkiaOutputDeviceBufferQueue::EndPaint() {
  DCHECK(current_image_);
  current_image_->EndWriteSkia();
}

bool SkiaOutputDeviceBufferQueue::AllocateFrameBuffers(size_t n) {
  std::vector<std::unique_ptr<OutputPresenter::Image>> new_images =
      presenter_->AllocateImages(color_space_, image_size_, n);
  if (new_images.size() != n) {
    LOG(ERROR) << "AllocateImages failed " << new_images.size() << " " << n;
    CheckForLoopFailuresBufferQueue();
    return false;
  }
  for (auto& image : new_images) {
    available_images_.push_front(image.get());
  }
  images_.insert(images_.end(), std::make_move_iterator(new_images.begin()),
                 std::make_move_iterator(new_images.end()));
  return true;
}

void SkiaOutputDeviceBufferQueue::ReleaseOneFrameBuffer() {
  DCHECK(capabilities_.use_dynamic_frame_buffer_allocation);
  CHECK_GE(available_images_.size(), 1u);
  OutputPresenter::Image* image_to_free = available_images_.back();
  DCHECK_NE(image_to_free, current_image_);
  DCHECK_NE(image_to_free, submitted_image_);
  DCHECK_NE(image_to_free, displayed_image_);
  available_images_.pop_back();
  for (auto iter = images_.begin(); iter != images_.end(); ++iter) {
    if (iter->get() == image_to_free) {
      images_.erase(iter);
      break;
    }
  }
}

bool SkiaOutputDeviceBufferQueue::OverlayDataComparator::operator()(
    const OverlayData& lhs,
    const OverlayData& rhs) const {
  return lhs.mailbox() < rhs.mailbox();
}

bool SkiaOutputDeviceBufferQueue::OverlayDataComparator::operator()(
    const OverlayData& lhs,
    const gpu::Mailbox& rhs) const {
  return lhs.mailbox() < rhs;
}
bool SkiaOutputDeviceBufferQueue::OverlayDataComparator::operator()(
    const gpu::Mailbox& lhs,
    const OverlayData& rhs) const {
  return lhs < rhs.mailbox();
}

}  // namespace viz
