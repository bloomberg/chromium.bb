// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_surface_control.h"

#include <utility>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/build_info.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_utils.h"

namespace gl {
namespace {

constexpr char kRootSurfaceName[] = "ChromeNativeWindowSurface";
constexpr char kChildSurfaceName[] = "ChromeChildSurface";

gfx::Size GetBufferSize(const AHardwareBuffer* buffer) {
  AHardwareBuffer_Desc desc;
  base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer, &desc);
  return gfx::Size(desc.width, desc.height);
}

std::string BuildSurfaceName(const char* suffix) {
  return base::StrCat(
      {base::android::BuildInfo::GetInstance()->package_name(), "/", suffix});
}

}  // namespace

GLSurfaceEGLSurfaceControl::GLSurfaceEGLSurfaceControl(
    ANativeWindow* window,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : root_surface_name_(BuildSurfaceName(kRootSurfaceName)),
      child_surface_name_(BuildSurfaceName(kChildSurfaceName)),
      window_rect_(0,
                   0,
                   ANativeWindow_getWidth(window),
                   ANativeWindow_getHeight(window)),
      root_surface_(
          new SurfaceControl::Surface(window, root_surface_name_.c_str())),
      gpu_task_runner_(std::move(task_runner)) {}

GLSurfaceEGLSurfaceControl::~GLSurfaceEGLSurfaceControl() {
  Destroy();
}

int GLSurfaceEGLSurfaceControl::GetBufferCount() const {
  // Triple buffering to match framework's BufferQueue.
  return 3;
}

bool GLSurfaceEGLSurfaceControl::Initialize(GLSurfaceFormat format) {
  if (!root_surface_->surface())
    return false;

  format_ = format;

  // Surfaceless is always disabled on Android so we create a 1x1 pbuffer
  // surface.
  if (!offscreen_surface_) {
    EGLDisplay display = GetDisplay();
    if (!display) {
      LOG(ERROR) << "Trying to create surface with invalid display.";
      return false;
    }

    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE,
    };
    offscreen_surface_ =
        eglCreatePbufferSurface(display, GetConfig(), pbuffer_attribs);
    if (!offscreen_surface_) {
      LOG(ERROR) << "eglCreatePbufferSurface failed with error "
                 << ui::GetLastEGLErrorString();
      return false;
    }
  }

  return true;
}

void GLSurfaceEGLSurfaceControl::PrepareToDestroy(bool have_context) {
  // Drop all transaction callbacks since its not possible to make the context
  // current after this point.
  weak_factory_.InvalidateWeakPtrs();
}

void GLSurfaceEGLSurfaceControl::Destroy() {
  pending_transaction_.reset();
  surface_list_.clear();
  root_surface_.reset();

  if (offscreen_surface_) {
    if (!eglDestroySurface(GetDisplay(), offscreen_surface_)) {
      LOG(ERROR) << "eglDestroySurface failed with error "
                 << ui::GetLastEGLErrorString();
    }
    offscreen_surface_ = nullptr;
  }
}

bool GLSurfaceEGLSurfaceControl::Resize(const gfx::Size& size,
                                        float scale_factor,
                                        const gfx::ColorSpace& color_space,
                                        bool has_alpha) {
  // TODO(khushalsagar): Update GLSurfaceFormat using the |color_space| above?
  // We don't do this for the NativeViewGLSurfaceEGL as well yet.
  window_rect_ = gfx::Rect(size);
  return true;
}

bool GLSurfaceEGLSurfaceControl::IsOffscreen() {
  return false;
}

gfx::SwapResult GLSurfaceEGLSurfaceControl::SwapBuffers(
    PresentationCallback callback) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::SwapResult GLSurfaceEGLSurfaceControl::CommitOverlayPlanes(
    PresentationCallback callback) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::SwapResult GLSurfaceEGLSurfaceControl::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurfaceEGLSurfaceControl::SwapBuffersAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  CommitPendingTransaction(window_rect_, std::move(completion_callback),
                           std::move(presentation_callback));
}

void GLSurfaceEGLSurfaceControl::CommitOverlayPlanesAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  CommitPendingTransaction(window_rect_, std::move(completion_callback),
                           std::move(presentation_callback));
}

void GLSurfaceEGLSurfaceControl::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  CommitPendingTransaction(gfx::Rect(x, y, width, height),
                           std::move(completion_callback),
                           std::move(presentation_callback));
}

void GLSurfaceEGLSurfaceControl::CommitPendingTransaction(
    const gfx::Rect& damage_rect,
    SwapCompletionCallback completion_callback,
    PresentationCallback present_callback) {
  // The transaction is initialized on the first ScheduleOverlayPlane call. If
  // we don't have a transaction at this point, it means the scheduling the
  // overlay plane failed. Simply report a swap failure to lose the context and
  // recreate the surface.
  if (!pending_transaction_ || surface_lost_) {
    LOG(ERROR) << "CommitPendingTransaction failed because surface is lost";

    surface_lost_ = true;
    std::move(completion_callback).Run(gfx::SwapResult::SWAP_FAILED, nullptr);
    std::move(present_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  // This is to workaround an Android bug where not specifying a damage region
  // is assumed to mean nothing is damaged. See crbug.com/993977.
  for (size_t i = 0; i < pending_surfaces_count_; ++i) {
    const auto& surface_state = surface_list_[i];
    if (!surface_state.hardware_buffer)
      continue;

    pending_transaction_->SetDamageRect(
        *surface_state.surface,
        gfx::Rect(GetBufferSize(surface_state.hardware_buffer)));
  }

  // Surfaces which are present in the current frame but not in the next frame
  // need to be explicitly updated in order to get a release fence for them in
  // the next transaction.
  DCHECK_LE(pending_surfaces_count_, surface_list_.size());
  for (size_t i = pending_surfaces_count_; i < surface_list_.size(); ++i) {
    const auto& surface_state = surface_list_[i];
    pending_transaction_->SetBuffer(*surface_state.surface, nullptr,
                                    base::ScopedFD());
    pending_transaction_->SetVisibility(*surface_state.surface, false);
  }

  // TODO(khushalsagar): Consider using the SetDamageRect API for partial
  // invalidations. Note that the damage rect set should be in the space in
  // which the content is rendered (including the pre-transform). See
  // crbug.com/988857 for details.

  // Release resources for the current frame once the next frame is acked.
  ResourceRefs resources_to_release;
  resources_to_release.swap(current_frame_resources_);
  current_frame_resources_.clear();

  // Track resources to be owned by the framework after this transaction.
  current_frame_resources_.swap(pending_frame_resources_);
  pending_frame_resources_.clear();

  SurfaceControl::Transaction::OnCompleteCb callback = base::BindOnce(
      &GLSurfaceEGLSurfaceControl::OnTransactionAckOnGpuThread,
      weak_factory_.GetWeakPtr(), std::move(completion_callback),
      std::move(present_callback), std::move(resources_to_release));
  pending_transaction_->SetOnCompleteCb(std::move(callback), gpu_task_runner_);

  // Cache only those surfaces which were used in this transaction. The surfaces
  // removed here are persisted in |resources_to_release| so we can release
  // them after receiving read fences from the framework.
  surface_list_.resize(pending_surfaces_count_);
  pending_surfaces_count_ = 0u;
  frame_rate_update_pending_ = false;

  if (transaction_ack_pending_) {
    pending_transaction_queue_.push(std::move(pending_transaction_).value());
  } else {
    transaction_ack_pending_ = true;
    pending_transaction_->Apply();
  }

  pending_transaction_.reset();
}

gfx::Size GLSurfaceEGLSurfaceControl::GetSize() {
  return gfx::Size(0, 0);
}

bool GLSurfaceEGLSurfaceControl::OnMakeCurrent(GLContext* context) {
  context_ = context;
  return true;
}

bool GLSurfaceEGLSurfaceControl::ScheduleOverlayPlane(
    int z_order,
    gfx::OverlayTransform transform,
    GLImage* image,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  if (surface_lost_) {
    LOG(ERROR) << "ScheduleOverlayPlane failed because surface is lost";
    return false;
  }

  const auto& image_color_space = GetNearestSupportedImageColorSpace(image);
  if (!SurfaceControl::SupportsColorSpace(image_color_space)) {
    LOG(ERROR) << "Not supported color space used with overlay : "
               << image_color_space.ToString();
  }

  if (!pending_transaction_)
    pending_transaction_.emplace();

  bool uninitialized = false;
  if (pending_surfaces_count_ == surface_list_.size()) {
    uninitialized = true;
    surface_list_.emplace_back(*root_surface_, child_surface_name_);
  }
  pending_surfaces_count_++;
  auto& surface_state = surface_list_.at(pending_surfaces_count_ - 1);

  if (uninitialized || surface_state.z_order != z_order) {
    surface_state.z_order = z_order;
    pending_transaction_->SetZOrder(*surface_state.surface, z_order);
  }

  AHardwareBuffer* hardware_buffer = nullptr;
  base::ScopedFD fence_fd;
  auto scoped_hardware_buffer = image->GetAHardwareBuffer();
  if (scoped_hardware_buffer) {
    hardware_buffer = scoped_hardware_buffer->buffer();
    fence_fd = scoped_hardware_buffer->TakeFence();

    auto* a_surface = surface_state.surface->surface();
    DCHECK_EQ(pending_frame_resources_.count(a_surface), 0u);

    auto& resource_ref = pending_frame_resources_[a_surface];
    resource_ref.surface = surface_state.surface;
    resource_ref.scoped_buffer = std::move(scoped_hardware_buffer);
  }

  surface_state.buffer_updated_in_pending_transaction =
      uninitialized || surface_state.hardware_buffer != hardware_buffer;
  if (surface_state.buffer_updated_in_pending_transaction) {
    surface_state.hardware_buffer = hardware_buffer;

    if (gpu_fence && surface_state.hardware_buffer) {
      auto fence_handle =
          gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle());
      DCHECK(!fence_handle.is_null());
      fence_fd = MergeFDs(std::move(fence_fd),
                          base::ScopedFD(fence_handle.native_fd.fd));
    }

    pending_transaction_->SetBuffer(*surface_state.surface,
                                    surface_state.hardware_buffer,
                                    std::move(fence_fd));
  }

  if (hardware_buffer) {
    gfx::Rect dst = bounds_rect;

    // Get the crop rectangle from the image which is the actual region of valid
    // pixels. This region could be smaller than the buffer dimensions. Hence
    // scale the |crop_rect| according to the valid pixel area rather than
    // buffer dimensions. crbug.com/1027766 for more details.
    gfx::Rect valid_pixel_rect = image->GetCropRect();
    gfx::Size buffer_size = GetBufferSize(hardware_buffer);

    // If the image doesn't provide a |valid_pixel_rect|, assume the entire
    // buffer is valid.
    if (valid_pixel_rect.IsEmpty()) {
      valid_pixel_rect.set_size(buffer_size);
    } else {
      // Clamp the |valid_pixel_rect| to the buffer dimensions to make sure for
      // some reason it does not overflows.
      valid_pixel_rect.Intersect(gfx::Rect(buffer_size));
    }
    gfx::RectF scaled_rect = gfx::RectF(
        crop_rect.x() * valid_pixel_rect.width() + valid_pixel_rect.x(),
        crop_rect.y() * valid_pixel_rect.height() + valid_pixel_rect.y(),
        crop_rect.width() * valid_pixel_rect.width(),
        crop_rect.height() * valid_pixel_rect.height());
    gfx::Rect src = gfx::ToEnclosedRect(scaled_rect);

    if (uninitialized || surface_state.src != src || surface_state.dst != dst ||
        surface_state.transform != transform) {
      surface_state.src = src;
      surface_state.dst = dst;
      surface_state.transform = transform;
      pending_transaction_->SetGeometry(*surface_state.surface, src, dst,
                                        transform);
    }
  }

  bool opaque = !enable_blend;
  if (uninitialized || surface_state.opaque != opaque) {
    surface_state.opaque = opaque;
    pending_transaction_->SetOpaque(*surface_state.surface, opaque);
  }

  if (uninitialized || surface_state.color_space != image_color_space) {
    surface_state.color_space = image_color_space;
    pending_transaction_->SetColorSpace(*surface_state.surface,
                                        image_color_space);
  }

  if (frame_rate_update_pending_)
    pending_transaction_->SetFrameRate(*surface_state.surface, frame_rate_);

  return true;
}

bool GLSurfaceEGLSurfaceControl::IsSurfaceless() const {
  return true;
}

void* GLSurfaceEGLSurfaceControl::GetHandle() {
  return offscreen_surface_;
}

bool GLSurfaceEGLSurfaceControl::SupportsPostSubBuffer() {
  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsAsyncSwap() {
  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsPlaneGpuFences() const {
  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsCommitOverlayPlanes() {
  return true;
}

void GLSurfaceEGLSurfaceControl::OnTransactionAckOnGpuThread(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    ResourceRefs released_resources,
    SurfaceControl::TransactionStats transaction_stats) {
  TRACE_EVENT0("gpu",
               "GLSurfaceEGLSurfaceControl::OnTransactionAckOnGpuThread");

  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(transaction_ack_pending_);

  transaction_ack_pending_ = false;

  const bool has_context = context_->MakeCurrent(this);
  for (auto& surface_stat : transaction_stats.surface_stats) {
    auto it = released_resources.find(surface_stat.surface);

    // The transaction ack includes data for all surfaces updated in this
    // transaction. So the following condition can occur if a new surface was
    // added in this transaction with a buffer. It'll be included in the ack
    // with no fence, since its not being released and so shouldn't be in
    // |released_resources| either.
    if (it == released_resources.end()) {
      DCHECK(!surface_stat.fence.is_valid());
      continue;
    }

    if (surface_stat.fence.is_valid()) {
      it->second.scoped_buffer->SetReadFence(std::move(surface_stat.fence),
                                             has_context);
    }
  }

  // Note that we may not see |surface_stats| for every resource above. This is
  // because we take a ref on every buffer used in a frame, even if it is not
  // updated in that frame. Since the transaction ack only includes surfaces
  // which were updated in that transaction, the surfaces with no buffer updates
  // won't be present in the ack.
  released_resources.clear();

  // The presentation feedback callback must run after swap completion.
  std::move(completion_callback).Run(gfx::SwapResult::SWAP_ACK, nullptr);

  PendingPresentationCallback pending_cb;
  pending_cb.latch_time = transaction_stats.latch_time;
  pending_cb.present_fence = std::move(transaction_stats.present_fence);
  pending_cb.callback = std::move(presentation_callback);
  pending_presentation_callback_queue_.push(std::move(pending_cb));

  CheckPendingPresentationCallbacks();

  if (!pending_transaction_queue_.empty()) {
    transaction_ack_pending_ = true;
    pending_transaction_queue_.front().Apply();
    pending_transaction_queue_.pop();
  }
}

void GLSurfaceEGLSurfaceControl::CheckPendingPresentationCallbacks() {
  TRACE_EVENT0("gpu",
               "GLSurfaceEGLSurfaceControl::CheckPendingPresentationCallbacks");
  check_pending_presentation_callback_queue_task_.Cancel();

  while (!pending_presentation_callback_queue_.empty()) {
    auto& pending_cb = pending_presentation_callback_queue_.front();

    base::TimeTicks signal_time;
    auto status =
        pending_cb.present_fence.is_valid()
            ? GLFenceAndroidNativeFenceSync::GetStatusChangeTimeForFence(
                  pending_cb.present_fence.get(), &signal_time)
            : GLFenceAndroidNativeFenceSync::kInvalid;
    if (status == GLFenceAndroidNativeFenceSync::kNotSignaled)
      break;

    auto flags = gfx::PresentationFeedback::kHWCompletion |
                 gfx::PresentationFeedback::kVSync;
    if (status == GLFenceAndroidNativeFenceSync::kInvalid) {
      signal_time = pending_cb.latch_time;
      flags = 0u;
    }

    TRACE_EVENT_INSTANT0(
        "gpu",
        "GLSurfaceEGLSurfaceControl::CheckPendingPresentationCallbacks - "
        "presentation_feedback",
        TRACE_EVENT_SCOPE_THREAD);
    gfx::PresentationFeedback feedback(signal_time, base::TimeDelta(), flags);
    std::move(pending_cb.callback).Run(feedback);
    pending_presentation_callback_queue_.pop();
  }

  // If there are unsignaled fences and we don't have any pending transactions,
  // schedule a task to poll the fences again. If there is a pending transaction
  // already, then we'll poll when that transaction is acked.
  if (!pending_presentation_callback_queue_.empty() &&
      pending_transaction_queue_.empty()) {
    check_pending_presentation_callback_queue_task_.Reset(base::BindOnce(
        &GLSurfaceEGLSurfaceControl::CheckPendingPresentationCallbacks,
        weak_factory_.GetWeakPtr()));
    gpu_task_runner_->PostDelayedTask(
        FROM_HERE, check_pending_presentation_callback_queue_task_.callback(),
        base::TimeDelta::FromSeconds(1) / 60);
  }
}

void GLSurfaceEGLSurfaceControl::SetDisplayTransform(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;
}

gfx::SurfaceOrigin GLSurfaceEGLSurfaceControl::GetOrigin() const {
  // GLSurfaceEGLSurfaceControl's y-axis is flipped compare to GL - (0,0) is at
  // top left corner.
  return gfx::SurfaceOrigin::kTopLeft;
}

void GLSurfaceEGLSurfaceControl::SetFrameRate(float frame_rate) {
  if (frame_rate_ == frame_rate)
    return;

  frame_rate_ = frame_rate;
  frame_rate_update_pending_ = true;
}

gfx::Rect GLSurfaceEGLSurfaceControl::ApplyDisplayInverse(
    const gfx::Rect& input) const {
  gfx::Transform display_inverse = gfx::OverlayTransformToTransform(
      gfx::InvertOverlayTransform(display_transform_),
      gfx::SizeF(window_rect_.size()));
  return cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
      display_inverse, input);
}

const gfx::ColorSpace&
GLSurfaceEGLSurfaceControl::GetNearestSupportedImageColorSpace(
    GLImage* image) const {
  static constexpr gfx::ColorSpace kSRGB = gfx::ColorSpace::CreateSRGB();
  static constexpr gfx::ColorSpace kP3 = gfx::ColorSpace::CreateDisplayP3D65();

  switch (format_.GetColorSpace()) {
    case GLSurfaceFormat::COLOR_SPACE_UNSPECIFIED:
    case GLSurfaceFormat::COLOR_SPACE_SRGB:
      return kSRGB;
    case GLSurfaceFormat::COLOR_SPACE_DISPLAY_P3:
      return image->color_space() == kP3 ? kP3 : kSRGB;
  }

  NOTREACHED();
  return kSRGB;
}

GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState(
    const SurfaceControl::Surface& parent,
    const std::string& name)
    : surface(new SurfaceControl::Surface(parent, name.c_str())) {}

GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState() = default;
GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState(SurfaceState&& other) =
    default;
GLSurfaceEGLSurfaceControl::SurfaceState&
GLSurfaceEGLSurfaceControl::SurfaceState::operator=(SurfaceState&& other) =
    default;

GLSurfaceEGLSurfaceControl::SurfaceState::~SurfaceState() = default;

GLSurfaceEGLSurfaceControl::ResourceRef::ResourceRef() = default;
GLSurfaceEGLSurfaceControl::ResourceRef::~ResourceRef() = default;
GLSurfaceEGLSurfaceControl::ResourceRef::ResourceRef(ResourceRef&& other) =
    default;
GLSurfaceEGLSurfaceControl::ResourceRef&
GLSurfaceEGLSurfaceControl::ResourceRef::operator=(ResourceRef&& other) =
    default;

GLSurfaceEGLSurfaceControl::PendingPresentationCallback::
    PendingPresentationCallback() = default;
GLSurfaceEGLSurfaceControl::PendingPresentationCallback::
    ~PendingPresentationCallback() = default;
GLSurfaceEGLSurfaceControl::PendingPresentationCallback::
    PendingPresentationCallback(PendingPresentationCallback&& other) = default;
GLSurfaceEGLSurfaceControl::PendingPresentationCallback&
GLSurfaceEGLSurfaceControl::PendingPresentationCallback::operator=(
    PendingPresentationCallback&& other) = default;

}  // namespace gl
