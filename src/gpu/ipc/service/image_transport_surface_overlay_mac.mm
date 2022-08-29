// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface_overlay_mac.h"

#include <memory>
#include <sstream>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/accelerated_widget_mac/ca_layer_tree_coordinator.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/ca_renderer_layer_params.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/scoped_cgl.h"

namespace gpu {

namespace {

// Control use of AVFoundation to draw video content.
base::Feature kAVFoundationOverlays{"avfoundation-overlays",
                                    base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace

ImageTransportSurfaceOverlayMac::ImageTransportSurfaceOverlayMac(
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate)
    : delegate_(delegate),
      use_remote_layer_api_(ui::RemoteLayerAPISupported()),
      scale_factor_(1),
      gl_renderer_id_(0),
      weak_ptr_factory_(this) {
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);

  static bool av_disabled_at_command_line =
      !base::FeatureList::IsEnabled(kAVFoundationOverlays);

  bool allow_av_sample_buffer_display_layer =
      !av_disabled_at_command_line &&
      !delegate_->GetFeatureInfo()
           ->workarounds()
           .disable_av_sample_buffer_display_layer;

  ca_layer_tree_coordinator_ = std::make_unique<ui::CALayerTreeCoordinator>(
      use_remote_layer_api_, allow_av_sample_buffer_display_layer);

  // Create the CAContext to send this to the GPU process, and the layer for
  // the context.
  if (use_remote_layer_api_) {
    CGSConnectionID connection_id = CGSMainConnectionID();
    ca_context_.reset([[CAContext contextWithCGSConnection:connection_id
                                                   options:@{}] retain]);
    [ca_context_ setLayer:ca_layer_tree_coordinator_->GetCALayerForDisplay()];
  }
}

ImageTransportSurfaceOverlayMac::~ImageTransportSurfaceOverlayMac() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  Destroy();
}

bool ImageTransportSurfaceOverlayMac::Initialize(gl::GLSurfaceFormat format) {
  return true;
}

void ImageTransportSurfaceOverlayMac::PrepareToDestroy(bool have_context) {}

void ImageTransportSurfaceOverlayMac::Destroy() {
  ca_layer_tree_coordinator_.reset();
}

bool ImageTransportSurfaceOverlayMac::IsOffscreen() {
  return false;
}

void ImageTransportSurfaceOverlayMac::ApplyBackpressure() {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::ApplyBackpressure");
  // Create the fence for the current frame before waiting on the previous
  // frame's fence (to maximize CPU and GPU execution overlap).
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  uint64_t this_frame_fence = current_context->BackpressureFenceCreate();
  current_context->BackpressureFenceWait(previous_frame_fence_);
  previous_frame_fence_ = this_frame_fence;
}

void ImageTransportSurfaceOverlayMac::BufferPresented(
    gl::GLSurface::PresentationCallback callback,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(feedback);
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffersInternal(
    gl::GLSurface::SwapCompletionCallback completion_callback,
    gl::GLSurface::PresentationCallback presentation_callback) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::SwapBuffersInternal");

  constexpr base::TimeDelta kHistogramMinTime = base::Microseconds(5);
  constexpr base::TimeDelta kHistogramMaxTime = base::Milliseconds(16);
  constexpr int kHistogramTimeBuckets = 50;

  // Do a GL fence for flush to apply back-pressure before drawing.
  {
    base::TimeTicks start_time = base::TimeTicks::Now();
    ApplyBackpressure();

    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Gpu.Mac.BackpressureUs", base::TimeTicks::Now() - start_time,
        kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  }

  // Update the CALayer tree in the GPU process.
  base::TimeTicks before_transaction_time = base::TimeTicks::Now();
  {
    TRACE_EVENT0("gpu", "CommitPendingTreesToCA");
    ca_layer_tree_coordinator_->CommitPendingTreesToCA();

    base::TimeDelta transaction_time =
        base::TimeTicks::Now() - before_transaction_time;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.IOSurface.CATransactionTimeUs", transaction_time,
        kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  }

  // Populate the CA layer parameters to send to the browser.
  gfx::CALayerParams params;
  {
    TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers", TRACE_EVENT_SCOPE_THREAD,
                         "GLImpl", static_cast<int>(gl::GetGLImplementation()),
                         "width", pixel_size_.width());
    if (use_remote_layer_api_) {
      params.ca_context_id = [ca_context_ contextId];
    } else {
      IOSurfaceRef io_surface =
          ca_layer_tree_coordinator_->GetIOSurfaceForDisplay();
      if (io_surface) {
        params.io_surface_mach_port.reset(IOSurfaceCreateMachPort(io_surface));
      }
    }
    params.pixel_size = pixel_size_;
    params.scale_factor = scale_factor_;
    params.is_empty = false;
  }

  // Send the swap parameters to the browser.
  if (completion_callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback),
                       gfx::SwapCompletionResult(
                           gfx::SwapResult::SWAP_ACK,
                           std::make_unique<gfx::CALayerParams>(params))));
  }
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::Hertz(60),
                                     /*flags=*/0);
  feedback.ca_layer_error_code = ca_layer_error_code_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageTransportSurfaceOverlayMac::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(presentation_callback), feedback));
  return gfx::SwapResult::SWAP_ACK;
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::SwapBuffers(
    gl::GLSurface::PresentationCallback callback) {
  return SwapBuffersInternal(base::DoNothing(), std::move(callback));
}

void ImageTransportSurfaceOverlayMac::SwapBuffersAsync(
    gl::GLSurface::SwapCompletionCallback completion_callback,
    gl::GLSurface::PresentationCallback presentation_callback) {
  SwapBuffersInternal(std::move(completion_callback),
                      std::move(presentation_callback));
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    gl::GLSurface::PresentationCallback callback) {
  return SwapBuffersInternal(base::DoNothing(), std::move(callback));
}

void ImageTransportSurfaceOverlayMac::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    gl::GLSurface::SwapCompletionCallback completion_callback,
    gl::GLSurface::PresentationCallback presentation_callback) {
  SwapBuffersInternal(std::move(completion_callback),
                      std::move(presentation_callback));
}

gfx::SwapResult ImageTransportSurfaceOverlayMac::CommitOverlayPlanes(
    gl::GLSurface::PresentationCallback callback) {
  return SwapBuffersInternal(base::DoNothing(), std::move(callback));
}

void ImageTransportSurfaceOverlayMac::CommitOverlayPlanesAsync(
    gl::GLSurface::SwapCompletionCallback completion_callback,
    gl::GLSurface::PresentationCallback presentation_callback) {
  SwapBuffersInternal(std::move(completion_callback),
                      std::move(presentation_callback));
}

bool ImageTransportSurfaceOverlayMac::SupportsPostSubBuffer() {
  return true;
}

bool ImageTransportSurfaceOverlayMac::SupportsCommitOverlayPlanes() {
  return true;
}

bool ImageTransportSurfaceOverlayMac::SupportsAsyncSwap() {
  return true;
}

gfx::Size ImageTransportSurfaceOverlayMac::GetSize() {
  return gfx::Size();
}

void* ImageTransportSurfaceOverlayMac::GetHandle() {
  return nullptr;
}

gl::GLSurfaceFormat ImageTransportSurfaceOverlayMac::GetFormat() {
  return gl::GLSurfaceFormat();
}

bool ImageTransportSurfaceOverlayMac::OnMakeCurrent(gl::GLContext* context) {
  // Ensure that the context is on the appropriate GL renderer. The GL renderer
  // will generally only change when the GPU changes.
  if (gl_renderer_id_ && context)
    context->share_group()->SetRendererID(gl_renderer_id_);
  return true;
}

bool ImageTransportSurfaceOverlayMac::ScheduleOverlayPlane(
    gl::GLImage* image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  if (overlay_plane_data.plane_transform != gfx::OVERLAY_TRANSFORM_NONE) {
    DLOG(ERROR) << "Invalid overlay plane transform.";
    return false;
  }
  if (overlay_plane_data.z_order) {
    DLOG(ERROR) << "Invalid non-zero Z order.";
    return false;
  }
  gl::GLImageIOSurface* io_surface_image =
      gl::GLImageIOSurface::FromGLImage(image);
  if (!io_surface_image) {
    DLOG(ERROR) << "Not an IOSurface image.";
    return false;
  }
  // TODO(1290313): the display_bounds might not need to be rounded to the
  // nearest rect as this eventually gets made into a CALayer. CALayers work in
  // floats.
  const ui::CARendererLayerParams overlay_as_calayer_params(
      false,          // is_clipped
      gfx::Rect(),    // clip_rect
      gfx::RRectF(),  // rounded_corner_bounds
      0,              // sorting_context_id
      gfx::Transform(), image,
      overlay_plane_data.crop_rect,                           // contents_rect
      gfx::ToNearestRect(overlay_plane_data.display_bounds),  // rect
      SK_ColorTRANSPARENT,               // background_color
      0,                                 // edge_aa_mask
      1.f,                               // opacity
      GL_LINEAR,                         // filter
      gfx::ProtectedVideoType::kClear);  // protected_video_type
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(overlay_as_calayer_params);
}

bool ImageTransportSurfaceOverlayMac::ScheduleCALayer(
    const ui::CARendererLayerParams& params) {
  if (params.image) {
    gl::GLImageIOSurface* io_surface_image =
        gl::GLImageIOSurface::FromGLImage(params.image);
    if (!io_surface_image) {
      DLOG(ERROR) << "Cannot schedule CALayer with non-IOSurface GLImage";
      return false;
    }
  }
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(params);
}

bool ImageTransportSurfaceOverlayMac::IsSurfaceless() const {
  return true;
}

gfx::SurfaceOrigin ImageTransportSurfaceOverlayMac::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

bool ImageTransportSurfaceOverlayMac::Resize(const gfx::Size& pixel_size,
                                             float scale_factor,
                                             const gfx::ColorSpace& color_space,
                                             bool has_alpha) {
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
  ca_layer_tree_coordinator_->Resize(pixel_size, scale_factor);
  return true;
}

void ImageTransportSurfaceOverlayMac::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
  // Create a new context, and use the GL renderer ID that the new context gets.
  scoped_refptr<ui::IOSurfaceContext> context_on_new_gpu =
      ui::IOSurfaceContext::Get(ui::IOSurfaceContext::kCALayerContext);
  if (!context_on_new_gpu)
    return;
  GLint context_renderer_id = -1;
  if (CGLGetParameter(context_on_new_gpu->cgl_context(),
                      kCGLCPCurrentRendererID,
                      &context_renderer_id) != kCGLNoError) {
    LOG(ERROR) << "Failed to create test context after GPU switch";
    return;
  }
  gl_renderer_id_ = context_renderer_id & kCGLRendererIDMatchingMask;

  // Delay releasing the reference to the new GL context. The reason for this
  // is to avoid creating-then-destroying the context for every image transport
  // surface that is observing the GPU switch.
  base::ThreadTaskRunnerHandle::Get()->ReleaseSoon(
      FROM_HERE, std::move(context_on_new_gpu));
}

void ImageTransportSurfaceOverlayMac::SetCALayerErrorCode(
    gfx::CALayerResult ca_layer_error_code) {
  ca_layer_error_code_ = ca_layer_error_code;
}

#if defined(USE_EGL)

ImageTransportSurfaceOverlayMacEGL::ImageTransportSurfaceOverlayMacEGL(
    gl::GLDisplayEGL* display,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate)
    : gl::GLSurfaceEGL(display),
      delegate_(delegate),
      use_remote_layer_api_(ui::RemoteLayerAPISupported()),
      scale_factor_(1),
      gl_renderer_id_(0),
      weak_ptr_factory_(this) {
  ui::GpuSwitchingManager::GetInstance()->AddObserver(this);

  static bool av_disabled_at_command_line =
      !base::FeatureList::IsEnabled(kAVFoundationOverlays);

  bool allow_av_sample_buffer_display_layer =
      !av_disabled_at_command_line &&
      !delegate_->GetFeatureInfo()
           ->workarounds()
           .disable_av_sample_buffer_display_layer;

  ca_layer_tree_coordinator_ = std::make_unique<ui::CALayerTreeCoordinator>(
      use_remote_layer_api_, allow_av_sample_buffer_display_layer);

  // Create the CAContext to send this to the GPU process, and the layer for
  // the context.
  if (use_remote_layer_api_) {
    CGSConnectionID connection_id = CGSMainConnectionID();
    ca_context_.reset([[CAContext contextWithCGSConnection:connection_id
                                                   options:@{}] retain]);
    [ca_context_ setLayer:ca_layer_tree_coordinator_->GetCALayerForDisplay()];
  }
}

ImageTransportSurfaceOverlayMacEGL::~ImageTransportSurfaceOverlayMacEGL() {
  ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  Destroy();
}

bool ImageTransportSurfaceOverlayMacEGL::Initialize(
    gl::GLSurfaceFormat format) {
  return true;
}

void ImageTransportSurfaceOverlayMacEGL::PrepareToDestroy(bool have_context) {}

void ImageTransportSurfaceOverlayMacEGL::Destroy() {
  ca_layer_tree_coordinator_.reset();
}

bool ImageTransportSurfaceOverlayMacEGL::IsOffscreen() {
  return false;
}

void ImageTransportSurfaceOverlayMacEGL::ApplyBackpressure() {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::ApplyBackpressure");
  // Create the fence for the current frame before waiting on the previous
  // frame's fence (to maximize CPU and GPU execution overlap).
  gl::GLContext* current_context = gl::GLContext::GetCurrent();
  uint64_t this_frame_fence = current_context->BackpressureFenceCreate();
  current_context->BackpressureFenceWait(previous_frame_fence_);
  previous_frame_fence_ = this_frame_fence;
}

void ImageTransportSurfaceOverlayMacEGL::BufferPresented(
    gl::GLSurface::PresentationCallback callback,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(!callback.is_null());
  std::move(callback).Run(feedback);
}

gfx::SwapResult ImageTransportSurfaceOverlayMacEGL::SwapBuffersInternal(
    gl::GLSurface::SwapCompletionCallback completion_callback,
    gl::GLSurface::PresentationCallback presentation_callback) {
  TRACE_EVENT0("gpu", "ImageTransportSurfaceOverlayMac::SwapBuffersInternal");

  constexpr base::TimeDelta kHistogramMinTime = base::Microseconds(5);
  constexpr base::TimeDelta kHistogramMaxTime = base::Milliseconds(16);
  constexpr int kHistogramTimeBuckets = 50;

  // Do a GL fence for flush to apply back-pressure before drawing.
  {
    base::TimeTicks start_time = base::TimeTicks::Now();
    ApplyBackpressure();

    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Gpu.Mac.BackpressureUs", base::TimeTicks::Now() - start_time,
        kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  }

  // Update the CALayer tree in the GPU process.
  base::TimeTicks before_transaction_time = base::TimeTicks::Now();
  {
    TRACE_EVENT0("gpu", "CommitPendingTreesToCA");
    ca_layer_tree_coordinator_->CommitPendingTreesToCA();

    base::TimeDelta transaction_time =
        base::TimeTicks::Now() - before_transaction_time;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "GPU.IOSurface.CATransactionTimeUs", transaction_time,
        kHistogramMinTime, kHistogramMaxTime, kHistogramTimeBuckets);
  }

  // Populate the CA layer parameters to send to the browser.
  gfx::CALayerParams params;
  {
    TRACE_EVENT_INSTANT2("test_gpu", "SwapBuffers", TRACE_EVENT_SCOPE_THREAD,
                         "GLImpl", static_cast<int>(gl::GetGLImplementation()),
                         "width", pixel_size_.width());
    if (use_remote_layer_api_) {
      params.ca_context_id = [ca_context_ contextId];
    } else {
      IOSurfaceRef io_surface =
          ca_layer_tree_coordinator_->GetIOSurfaceForDisplay();
      if (io_surface) {
        params.io_surface_mach_port.reset(IOSurfaceCreateMachPort(io_surface));
      }
    }
    params.pixel_size = pixel_size_;
    params.scale_factor = scale_factor_;
    params.is_empty = false;
  }

  // Send the swap parameters to the browser.
  if (completion_callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback),
                       gfx::SwapCompletionResult(
                           gfx::SwapResult::SWAP_ACK,
                           std::make_unique<gfx::CALayerParams>(params))));
  }
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::Hertz(60),
                                     /*flags=*/0);
  feedback.ca_layer_error_code = ca_layer_error_code_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageTransportSurfaceOverlayMacEGL::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(presentation_callback), feedback));
  return gfx::SwapResult::SWAP_ACK;
}

gfx::SwapResult ImageTransportSurfaceOverlayMacEGL::SwapBuffers(
    gl::GLSurface::PresentationCallback callback) {
  return SwapBuffersInternal(base::DoNothing(), std::move(callback));
}

void ImageTransportSurfaceOverlayMacEGL::SwapBuffersAsync(
    gl::GLSurface::SwapCompletionCallback completion_callback,
    gl::GLSurface::PresentationCallback presentation_callback) {
  SwapBuffersInternal(std::move(completion_callback),
                      std::move(presentation_callback));
}

gfx::SwapResult ImageTransportSurfaceOverlayMacEGL::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    gl::GLSurface::PresentationCallback callback) {
  return SwapBuffersInternal(base::DoNothing(), std::move(callback));
}

void ImageTransportSurfaceOverlayMacEGL::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    gl::GLSurface::SwapCompletionCallback completion_callback,
    gl::GLSurface::PresentationCallback presentation_callback) {
  SwapBuffersInternal(std::move(completion_callback),
                      std::move(presentation_callback));
}

gfx::SwapResult ImageTransportSurfaceOverlayMacEGL::CommitOverlayPlanes(
    gl::GLSurface::PresentationCallback callback) {
  return SwapBuffersInternal(base::DoNothing(), std::move(callback));
}

void ImageTransportSurfaceOverlayMacEGL::CommitOverlayPlanesAsync(
    gl::GLSurface::SwapCompletionCallback completion_callback,
    gl::GLSurface::PresentationCallback presentation_callback) {
  SwapBuffersInternal(std::move(completion_callback),
                      std::move(presentation_callback));
}

bool ImageTransportSurfaceOverlayMacEGL::SupportsPostSubBuffer() {
  return true;
}

bool ImageTransportSurfaceOverlayMacEGL::SupportsCommitOverlayPlanes() {
  return true;
}

bool ImageTransportSurfaceOverlayMacEGL::SupportsAsyncSwap() {
  return true;
}

gfx::Size ImageTransportSurfaceOverlayMacEGL::GetSize() {
  return gfx::Size();
}

void* ImageTransportSurfaceOverlayMacEGL::GetHandle() {
  return nullptr;
}

gl::GLSurfaceFormat ImageTransportSurfaceOverlayMacEGL::GetFormat() {
  return gl::GLSurfaceFormat();
}

bool ImageTransportSurfaceOverlayMacEGL::OnMakeCurrent(gl::GLContext* context) {
  // Ensure that the context is on the appropriate GL renderer. The GL renderer
  // will generally only change when the GPU changes.
  if (gl_renderer_id_ && context)
    context->share_group()->SetRendererID(gl_renderer_id_);
  return true;
}

bool ImageTransportSurfaceOverlayMacEGL::ScheduleOverlayPlane(
    gl::GLImage* image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  if (overlay_plane_data.plane_transform != gfx::OVERLAY_TRANSFORM_NONE) {
    DLOG(ERROR) << "Invalid overlay plane transform.";
    return false;
  }
  if (overlay_plane_data.z_order) {
    DLOG(ERROR) << "Invalid non-zero Z order.";
    return false;
  }
  gl::GLImageIOSurface* io_surface_image =
      gl::GLImageIOSurface::FromGLImage(image);
  if (!io_surface_image) {
    DLOG(ERROR) << "Not an IOSurface image.";
    return false;
  }
  // TODO(1290313): the display_bounds might not need to be rounded to the
  // nearest rect as this eventually gets made into a CALayer. CALayers work in
  // floats.
  const ui::CARendererLayerParams overlay_as_calayer_params(
      false,          // is_clipped
      gfx::Rect(),    // clip_rect
      gfx::RRectF(),  // rounded_corner_bounds
      0,              // sorting_context_id
      gfx::Transform(), image,
      overlay_plane_data.crop_rect,                           // contents_rect
      gfx::ToNearestRect(overlay_plane_data.display_bounds),  // rect
      SK_ColorTRANSPARENT,               // background_color
      0,                                 // edge_aa_mask
      1.f,                               // opacity
      GL_LINEAR,                         // filter
      gfx::ProtectedVideoType::kClear);  // protected_video_type
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(overlay_as_calayer_params);
}

bool ImageTransportSurfaceOverlayMacEGL::ScheduleCALayer(
    const ui::CARendererLayerParams& params) {
  if (params.image) {
    gl::GLImageIOSurface* io_surface_image =
        gl::GLImageIOSurface::FromGLImage(params.image);
    if (!io_surface_image) {
      DLOG(ERROR) << "Cannot schedule CALayer with non-IOSurface GLImage";
      return false;
    }
  }
  return ca_layer_tree_coordinator_->GetPendingCARendererLayerTree()
      ->ScheduleCALayer(params);
}

bool ImageTransportSurfaceOverlayMacEGL::IsSurfaceless() const {
  return true;
}

gfx::SurfaceOrigin ImageTransportSurfaceOverlayMacEGL::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

bool ImageTransportSurfaceOverlayMacEGL::Resize(
    const gfx::Size& pixel_size,
    float scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha) {
  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;
  ca_layer_tree_coordinator_->Resize(pixel_size, scale_factor);
  return true;
}

void ImageTransportSurfaceOverlayMacEGL::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
  // Create a new context, and use the GL renderer ID that the new context gets.
  scoped_refptr<ui::IOSurfaceContext> context_on_new_gpu =
      ui::IOSurfaceContext::Get(ui::IOSurfaceContext::kCALayerContext);
  if (!context_on_new_gpu)
    return;
  GLint context_renderer_id = -1;
  if (CGLGetParameter(context_on_new_gpu->cgl_context(),
                      kCGLCPCurrentRendererID,
                      &context_renderer_id) != kCGLNoError) {
    LOG(ERROR) << "Failed to create test context after GPU switch";
    return;
  }
  gl_renderer_id_ = context_renderer_id & kCGLRendererIDMatchingMask;

  // Delay releasing the reference to the new GL context. The reason for this
  // is to avoid creating-then-destroying the context for every image transport
  // surface that is observing the GPU switch.
  base::ThreadTaskRunnerHandle::Get()->ReleaseSoon(
      FROM_HERE, std::move(context_on_new_gpu));
}

void ImageTransportSurfaceOverlayMacEGL::SetCALayerErrorCode(
    gfx::CALayerResult ca_layer_error_code) {
  ca_layer_error_code_ = ca_layer_error_code;
}

#endif  // USE_EGL

}  // namespace gpu
