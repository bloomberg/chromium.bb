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
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_context.h"
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
      gpu_task_runner_(std::move(task_runner)),
      weak_factory_(this) {}

GLSurfaceEGLSurfaceControl::~GLSurfaceEGLSurfaceControl() = default;

int GLSurfaceEGLSurfaceControl::GetBufferCount() const {
  // Triple buffering to match framework's BufferQueue.
  return 3;
}

bool GLSurfaceEGLSurfaceControl::Initialize(GLSurfaceFormat format) {
  format_ = format;
  return true;
}

void GLSurfaceEGLSurfaceControl::Destroy() {
  pending_transaction_.reset();
  surface_list_.clear();
  root_surface_.reset();
}

bool GLSurfaceEGLSurfaceControl::Resize(const gfx::Size& size,
                                        float scale_factor,
                                        ColorSpace color_space,
                                        bool has_alpha) {
  window_rect_ = gfx::Rect(0, 0, size.width(), size.height());
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
  DCHECK(pending_transaction_);

  // Mark the intersection of a surface's rect with the damage rect as the dirty
  // rect for that surface.
  DCHECK_LE(pending_surfaces_count_, surface_list_.size());
  for (size_t i = 0; i < pending_surfaces_count_; ++i) {
    const auto& surface_state = surface_list_[i];
    if (!surface_state.buffer_updated_in_pending_transaction)
      continue;

    gfx::Rect surface_damage_rect = surface_state.dst;
    surface_damage_rect.Intersect(damage_rect);
    pending_transaction_->SetDamageRect(*surface_state.surface,
                                        surface_damage_rect);
  }

  // Surfaces which are present in the current frame but not in the next frame
  // need to be explicitly updated in order to get a release fence for them in
  // the next transaction.
  for (size_t i = pending_surfaces_count_; i < surface_list_.size(); ++i) {
    pending_transaction_->SetBuffer(*surface_list_[i].surface, nullptr,
                                    base::ScopedFD());
  }

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
  if (!SurfaceControl::SupportsColorSpace(image->color_space())) {
    LOG(ERROR) << "Not supported color space used with overlay : "
               << image->color_space().ToString();
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

    gfx::Size buffer_size = GetBufferSize(hardware_buffer);
    gfx::RectF scaled_rect =
        gfx::RectF(crop_rect.x() * buffer_size.width(),
                   crop_rect.y() * buffer_size.height(),
                   crop_rect.width() * buffer_size.width(),
                   crop_rect.height() * buffer_size.height());
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

  if (uninitialized || surface_state.color_space != image->color_space()) {
    surface_state.color_space = image->color_space();
    pending_transaction_->SetColorSpace(*surface_state.surface,
                                        image->color_space());
  }

  return true;
}

bool GLSurfaceEGLSurfaceControl::IsSurfaceless() const {
  return true;
}

void* GLSurfaceEGLSurfaceControl::GetHandle() {
  return nullptr;
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

bool GLSurfaceEGLSurfaceControl::SupportsPresentationCallback() {
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
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(transaction_ack_pending_);

  transaction_ack_pending_ = false;

  // The presentation feedback callback must run after swap completion.
  std::move(completion_callback).Run(gfx::SwapResult::SWAP_ACK, nullptr);

  // TODO(khushalsagar): Maintain a queue of present fences so we poll to see if
  // they are signaled every frame, and get a signal timestamp to feed into this
  // feedback.
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::TimeDelta(),
                                     0 /* flags */);
  std::move(presentation_callback).Run(feedback);

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

  if (!pending_transaction_queue_.empty()) {
    transaction_ack_pending_ = true;
    pending_transaction_queue_.front().Apply();
    pending_transaction_queue_.pop();
  }
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

}  // namespace gl
