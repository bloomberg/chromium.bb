/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/Canvas2DLayerBridge.h"

#include <memory>
#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/CanvasHeuristicParameters.h"
#include "platform/graphics/CanvasMetrics.h"
#include "platform/graphics/CanvasResourceProvider.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/graphics/gpu/SharedContextRateLimiter.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebTraceLocation.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace {
enum {
  InvalidMailboxIndex = -1,
  MaxCanvasAnimationBacklog = 2,  // Make sure the the GPU is never more than
                                  // two animation frames behind.
};
}  // namespace

namespace blink {

Canvas2DLayerBridge::Canvas2DLayerBridge(const IntSize& size,
                                         int msaa_sample_count,
                                         AccelerationMode acceleration_mode,
                                         const CanvasColorParams& color_params,
                                         bool is_unit_test)
    : ImageBufferSurface(size, color_params),
      logger_(WTF::WrapUnique(new Logger)),
      weak_ptr_factory_(this),
      image_buffer_(nullptr),
      msaa_sample_count_(msaa_sample_count),
      bytes_allocated_(0),
      have_recorded_draw_commands_(false),
      destruction_in_progress_(false),
      filter_quality_(kLow_SkFilterQuality),
      is_hidden_(false),
      is_deferral_enabled_(true),
      software_rendering_while_hidden_(false),
      acceleration_mode_(acceleration_mode),
      color_params_(color_params) {
  // Used by browser tests to detect the use of a Canvas2DLayerBridge.
  TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);

  StartRecording();
  Clear();
}

Canvas2DLayerBridge::~Canvas2DLayerBridge() {
  BeginDestruction();
  DCHECK(destruction_in_progress_);
  layer_.reset();
}

void Canvas2DLayerBridge::StartRecording() {
  DCHECK(is_deferral_enabled_);
  recorder_ = WTF::WrapUnique(new PaintRecorder);
  PaintCanvas* canvas =
      recorder_->beginRecording(Size().Width(), Size().Height());
  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  canvas->save();

  if (image_buffer_) {
    image_buffer_->ResetCanvas(canvas);
  }
  recording_pixel_count_ = 0;
}

void Canvas2DLayerBridge::SetLoggerForTesting(std::unique_ptr<Logger> logger) {
  logger_ = std::move(logger);
}

void Canvas2DLayerBridge::ResetResourceProvider() {
  resource_provider_.reset();
}

bool Canvas2DLayerBridge::ShouldAccelerate(AccelerationHint hint) const {
  bool accelerate;
  if (software_rendering_while_hidden_) {
    accelerate = false;
  } else if (acceleration_mode_ == kForceAccelerationForTesting) {
    accelerate = true;
  } else if (acceleration_mode_ == kDisableAcceleration) {
    accelerate = false;
  } else {
    accelerate = hint == kPreferAcceleration ||
                 hint == kPreferAccelerationAfterVisibilityChange;
  }

  WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (accelerate && (!context_provider_wrapper ||
                     context_provider_wrapper->ContextProvider()
                             ->ContextGL()
                             ->GetGraphicsResetStatusKHR() != GL_NO_ERROR))
    accelerate = false;
  return accelerate;
}

bool Canvas2DLayerBridge::IsAccelerated() const {
  if (acceleration_mode_ == kDisableAcceleration)
    return false;
  if (IsHibernating())
    return false;
  if (software_rendering_while_hidden_)
    return false;
  if (resource_provider_)
    return resource_provider_->IsAccelerated();

  // Whether or not to accelerate is not yet resolved. Determine whether
  // immediate presentation of the canvas would result in the canvas being
  // accelerated. Presentation is assumed to be a 'PreferAcceleration'
  // operation.
  return ShouldAccelerate(kPreferAcceleration);
}

static void HibernateWrapper(WeakPtr<Canvas2DLayerBridge> bridge,
                             double /*idleDeadline*/) {
  if (bridge) {
    bridge->Hibernate();
  } else {
    Canvas2DLayerBridge::Logger local_logger;
    local_logger.ReportHibernationEvent(
        Canvas2DLayerBridge::
            kHibernationAbortedDueToDestructionWhileHibernatePending);
  }
}

static void HibernateWrapperForTesting(WeakPtr<Canvas2DLayerBridge> bridge) {
  HibernateWrapper(bridge, 0);
}

void Canvas2DLayerBridge::Hibernate() {
  DCHECK(!IsHibernating());
  DCHECK(hibernation_scheduled_);

  hibernation_scheduled_ = false;

  if (destruction_in_progress_) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueToPendingDestruction);
    return;
  }

  if (!resource_provider_) {
    logger_->ReportHibernationEvent(kHibernationAbortedBecauseNoSurface);
    return;
  }

  if (!IsHidden()) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueToVisibilityChange);
    return;
  }

  if (!IsValid()) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueGpuContextLoss);
    return;
  }

  if (!IsAccelerated()) {
    logger_->ReportHibernationEvent(
        kHibernationAbortedDueToSwitchToUnacceleratedRendering);
    return;
  }

  TRACE_EVENT0("blink", "Canvas2DLayerBridge::hibernate");
  sk_sp<SkSurface> temp_hibernation_surface =
      SkSurface::MakeRasterN32Premul(Size().Width(), Size().Height());
  if (!temp_hibernation_surface) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueToAllocationFailure);
    return;
  }
  // No HibernationEvent reported on success. This is on purppose to avoid
  // non-complementary stats. Each HibernationScheduled event is paired with
  // exactly one failure or exit event.
  FlushRecording();
  // The following checks that the flush succeeded, which should always be the
  // case because flushRecording should only fail it it fails to allocate
  // a surface, and we have an early exit at the top of this function for when
  // 'this' does not already have a surface.
  DCHECK(!have_recorded_draw_commands_);
  SkPaint copy_paint;
  copy_paint.setBlendMode(SkBlendMode::kSrc);
  scoped_refptr<StaticBitmapImage> snapshot = resource_provider_->Snapshot();
  temp_hibernation_surface->getCanvas()->drawImage(
      snapshot->PaintImageForCurrentFrame().GetSkImage(), 0, 0, &copy_paint);
  hibernation_image_ = temp_hibernation_surface->makeImageSnapshot();
  ResetResourceProvider();
  layer_->ClearTexture();

  // shouldBeDirectComposited() may have changed.
  if (image_buffer_)
    image_buffer_->SetNeedsCompositingUpdate();
  logger_->DidStartHibernating();
}

void Canvas2DLayerBridge::ReportResourceProviderCreationFailure() {
  if (!resource_provider_creation_failed_at_least_once_) {
    // Only count the failure once per instance so that the histogram may
    // reflect the proportion of Canvas2DLayerBridge instances with surface
    // allocation failures.
    CanvasMetrics::CountCanvasContextUsage(
        CanvasMetrics::kGPUAccelerated2DCanvasSurfaceCreationFailed);
    resource_provider_creation_failed_at_least_once_ = true;
  }
}

CanvasResourceProvider* Canvas2DLayerBridge::GetOrCreateResourceProvider(
    AccelerationHint hint) {
  if (context_lost_) {
    DCHECK(!resource_provider_);
    return nullptr;
  }

  if (resource_provider_)
    return resource_provider_.get();

  if (layer_ && !IsHibernating() && hint == kPreferAcceleration &&
      acceleration_mode_ != kDisableAcceleration) {
    return nullptr;  // re-creation will happen through restore()
  }

  bool want_acceleration = ShouldAccelerate(hint);
  if (CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU && IsHidden() &&
      want_acceleration) {
    want_acceleration = false;
    software_rendering_while_hidden_ = true;
  }

  CanvasResourceProvider::ResourceUsage usage =
      want_acceleration
          ? CanvasResourceProvider::kAcceleratedCompositedResourceUsage
          : CanvasResourceProvider::kSoftwareCompositedResourceUsage;

  resource_provider_ = CanvasResourceProvider::Create(
      Size(), msaa_sample_count_, color_params_, usage,
      SharedGpuContext::ContextProviderWrapper());

  if (resource_provider_) {
    // Always save an initial frame, to support resetting the top level matrix
    // and clip.
    resource_provider_->Canvas()->save();
    resource_provider_->SetFilterQuality(filter_quality_);
    resource_provider_->SetResourceRecyclingEnabled(!IsHidden());
  } else {
    ReportResourceProviderCreationFailure();
  }

  if (resource_provider_ && resource_provider_->IsAccelerated() && !layer_) {
    layer_ =
        Platform::Current()->CompositorSupport()->CreateExternalTextureLayer(
            this);
    layer_->SetOpaque(ColorParams().GetOpacityMode() == kOpaque);
    layer_->SetBlendBackgroundColor(ColorParams().GetOpacityMode() != kOpaque);
    GraphicsLayer::RegisterContentsLayer(layer_->Layer());
    layer_->SetNearestNeighbor(filter_quality_ == kNone_SkFilterQuality);
  }

  if (resource_provider_ && IsHibernating()) {
    if (resource_provider_->IsAccelerated()) {
      logger_->ReportHibernationEvent(kHibernationEndedNormally);
    } else {
      if (IsHidden()) {
        logger_->ReportHibernationEvent(
            kHibernationEndedWithSwitchToBackgroundRendering);
      } else {
        logger_->ReportHibernationEvent(kHibernationEndedWithFallbackToSW);
      }
    }

    cc::PaintFlags copy_paint;
    copy_paint.setBlendMode(SkBlendMode::kSrc);
    PaintImageBuilder builder = PaintImageBuilder::WithDefault();
    builder.set_image(hibernation_image_);
    builder.set_id(PaintImage::GetNextId());
    resource_provider_->Canvas()->drawImage(builder.TakePaintImage(), 0, 0,
                                            &copy_paint);
    hibernation_image_.reset();

    if (image_buffer_) {
      image_buffer_->UpdateGPUMemoryUsage();

      if (!is_deferral_enabled_)
        image_buffer_->ResetCanvas(resource_provider_->Canvas());

      // shouldBeDirectComposited() may have changed.
      image_buffer_->SetNeedsCompositingUpdate();
    }
  }

  return resource_provider_.get();
}

PaintCanvas* Canvas2DLayerBridge::Canvas() {
  if (!is_deferral_enabled_) {
    GetOrCreateResourceProvider();
    return resource_provider_ ? resource_provider_->Canvas() : nullptr;
  }
  return recorder_->getRecordingCanvas();
}

void Canvas2DLayerBridge::DisableDeferral(DisableDeferralReason reason) {
  // Disabling deferral is permanent: once triggered by disableDeferral()
  // we stay in immediate mode indefinitely. This is a performance heuristic
  // that significantly helps a number of use cases. The rationale is that if
  // immediate rendering was needed once, it is likely to be needed at least
  // once per frame, which eliminates the possibility for inter-frame
  // overdraw optimization. Furthermore, in cases where immediate mode is
  // required multiple times per frame, the repeated flushing of deferred
  // commands would cause significant overhead, so it is better to just stop
  // trying to defer altogether.
  if (!is_deferral_enabled_)
    return;

  DEFINE_STATIC_LOCAL(EnumerationHistogram, gpu_disabled_histogram,
                      ("Canvas.GPUAccelerated2DCanvasDisableDeferralReason",
                       kDisableDeferralReasonCount));
  gpu_disabled_histogram.Count(reason);
  CanvasMetrics::CountCanvasContextUsage(
      CanvasMetrics::kGPUAccelerated2DCanvasDeferralDisabled);
  FlushRecording();
  // Because we will be discarding the recorder, if the flush failed
  // content will be lost -> force m_haveRecordedDrawCommands to false
  have_recorded_draw_commands_ = false;

  is_deferral_enabled_ = false;
  recorder_.reset();
  // install the current matrix/clip stack onto the immediate canvas
  GetOrCreateResourceProvider();
  if (image_buffer_ && resource_provider_)
    image_buffer_->ResetCanvas(resource_provider_->Canvas());
}

void Canvas2DLayerBridge::SetImageBuffer(ImageBuffer* image_buffer) {
  image_buffer_ = image_buffer;
  if (image_buffer_ && is_deferral_enabled_) {
    image_buffer_->ResetCanvas(recorder_->getRecordingCanvas());
  }
}

void Canvas2DLayerBridge::BeginDestruction() {
  if (destruction_in_progress_)
    return;
  if (IsHibernating())
    logger_->ReportHibernationEvent(kHibernationEndedWithTeardown);
  hibernation_image_.reset();
  recorder_.reset();
  image_buffer_ = nullptr;
  destruction_in_progress_ = true;
  SetIsHidden(true);
  ResetResourceProvider();

  if (layer_ && acceleration_mode_ != kDisableAcceleration) {
    GraphicsLayer::UnregisterContentsLayer(layer_->Layer());
    layer_->ClearTexture();
    // Orphaning the layer is required to trigger the recration of a new layer
    // in the case where destruction is caused by a canvas resize. Test:
    // virtual/gpu/fast/canvas/canvas-resize-after-paint-without-layout.html
    layer_->Layer()->RemoveFromParent();
  }

  DCHECK(!bytes_allocated_);
}

void Canvas2DLayerBridge::SetFilterQuality(SkFilterQuality filter_quality) {
  DCHECK(!destruction_in_progress_);
  filter_quality_ = filter_quality;
  if (resource_provider_)
    resource_provider_->SetFilterQuality(filter_quality);
  if (layer_)
    layer_->SetNearestNeighbor(filter_quality == kNone_SkFilterQuality);
}

void Canvas2DLayerBridge::SetIsHidden(bool hidden) {
  bool new_hidden_value = hidden || destruction_in_progress_;
  if (is_hidden_ == new_hidden_value)
    return;

  is_hidden_ = new_hidden_value;
  if (resource_provider_)
    resource_provider_->SetResourceRecyclingEnabled(!IsHidden());

  if (CANVAS2D_HIBERNATION_ENABLED && resource_provider_ && IsHidden() &&
      !destruction_in_progress_ && !hibernation_scheduled_) {
    if (layer_)
      layer_->ClearTexture();
    logger_->ReportHibernationEvent(kHibernationScheduled);
    hibernation_scheduled_ = true;
    if (dont_use_idle_scheduling_for_testing_) {
      Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
          BLINK_FROM_HERE, WTF::Bind(&HibernateWrapperForTesting,
                                     weak_ptr_factory_.CreateWeakPtr()));
    } else {
      Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
          BLINK_FROM_HERE,
          WTF::Bind(&HibernateWrapper, weak_ptr_factory_.CreateWeakPtr()));
    }
  }
  if (!IsHidden() && software_rendering_while_hidden_) {
    FlushRecording();
    cc::PaintFlags copy_paint;
    copy_paint.setBlendMode(SkBlendMode::kSrc);

    std::unique_ptr<CanvasResourceProvider> old_resource_provider =
        std::move(resource_provider_);
    ResetResourceProvider();

    software_rendering_while_hidden_ = false;
    GetOrCreateResourceProvider(kPreferAccelerationAfterVisibilityChange);

    if (resource_provider_) {
      if (old_resource_provider) {
        cc::PaintImage snapshot =
            old_resource_provider->Snapshot()->PaintImageForCurrentFrame();
        resource_provider_->Canvas()->drawImage(snapshot, 0, 0, &copy_paint);
      }
      if (image_buffer_ && !is_deferral_enabled_) {
        image_buffer_->ResetCanvas(resource_provider_->Canvas());
      }
    } else {
      // New resource provider could not be created. Stay with old one.
      resource_provider_ = std::move(old_resource_provider);
    }
  }
  if (!IsHidden() && IsHibernating()) {
    GetOrCreateResourceProvider();  // Rude awakening
  }
}

bool Canvas2DLayerBridge::WritePixels(const SkImageInfo& orig_info,
                                      const void* pixels,
                                      size_t row_bytes,
                                      int x,
                                      int y) {
  if (!GetOrCreateResourceProvider())
    return false;
  if (x <= 0 && y <= 0 && x + orig_info.width() >= Size().Width() &&
      y + orig_info.height() >= Size().Height()) {
    SkipQueuedDrawCommands();
  } else {
    FlushRecording();
  }

  SkImageInfo tmp_info = SkImageInfo::Make(
      orig_info.width(), orig_info.height(), ColorParams().GetSkColorType(),
      kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());
  sk_sp<SkSurface> tmp_surface = SkSurface::MakeRaster(tmp_info, nullptr);
  tmp_surface->getCanvas()->writePixels(orig_info, pixels, row_bytes, 0, 0);

  PaintImageBuilder builder = PaintImageBuilder::WithDefault();
  builder.set_image(tmp_surface->makeImageSnapshot());
  builder.set_id(PaintImage::GetNextId());

  PaintCanvas* canvas = GetOrCreateResourceProvider()->Canvas();
  if (!canvas)
    return false;

  // Ignore clip and matrix
  canvas->restoreToCount(0);

  PaintFlags copy_paint;
  copy_paint.setBlendMode(SkBlendMode::kSrc);

  canvas->drawImage(builder.TakePaintImage(), x, y, &copy_paint);

  canvas->save();  // intial save
  if (image_buffer_ && !is_deferral_enabled_) {
    image_buffer_->ResetCanvas(canvas);
  }

  // We did not make a copy of the pixel data, so it needs to be consumed
  // immediately
  DidDraw(FloatRect(x, y, orig_info.width(), orig_info.height()));
  GetOrCreateResourceProvider()->FlushSkia();

  return true;
}

void Canvas2DLayerBridge::SkipQueuedDrawCommands() {
  if (have_recorded_draw_commands_) {
    recorder_->finishRecordingAsPicture();
    StartRecording();
    have_recorded_draw_commands_ = false;
  }

  if (is_deferral_enabled_) {
    if (rate_limiter_)
      rate_limiter_->Reset();
  }
}

void Canvas2DLayerBridge::FlushRecording() {
  DCHECK(!destruction_in_progress_);

  if (have_recorded_draw_commands_ && GetOrCreateResourceProvider()) {
    TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushRecording");

    PaintCanvas* canvas = GetOrCreateResourceProvider()->Canvas();
    {
      sk_sp<PaintRecord> recording = recorder_->finishRecordingAsPicture();
      canvas->drawPicture(recording);
    }
    if (is_deferral_enabled_)
      StartRecording();
    have_recorded_draw_commands_ = false;
  }
}

bool Canvas2DLayerBridge::IsValid() const {
  return const_cast<Canvas2DLayerBridge*>(this)->CheckResourceProviderValid();
}

bool Canvas2DLayerBridge::CheckResourceProviderValid() {
  DCHECK(!destruction_in_progress_);
  if (destruction_in_progress_)
    return false;
  if (IsHibernating())
    return true;
  if (!layer_ || acceleration_mode_ == kDisableAcceleration)
    return true;
  if (context_lost_)
    return false;
  if (resource_provider_ && resource_provider_->IsAccelerated() &&
      resource_provider_->IsGpuContextLost()) {
    context_lost_ = true;
    ResetResourceProvider();
    if (image_buffer_)
      image_buffer_->NotifySurfaceInvalid();
    CanvasMetrics::CountCanvasContextUsage(
        CanvasMetrics::kAccelerated2DCanvasGPUContextLost);
    return false;
  }
  if (!GetOrCreateResourceProvider())
    return false;
  return resource_provider_.get();
}

bool Canvas2DLayerBridge::Restore() {
  DCHECK(!destruction_in_progress_);
  DCHECK(context_lost_);
  if (destruction_in_progress_ || !IsAccelerated())
    return false;
  DCHECK(!resource_provider_);

  gpu::gles2::GLES2Interface* shared_gl = nullptr;
  layer_->ClearTexture();
  WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  if (context_provider_wrapper)
    shared_gl = context_provider_wrapper->ContextProvider()->ContextGL();

  if (shared_gl && shared_gl->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    std::unique_ptr<CanvasResourceProvider> resource_provider =
        CanvasResourceProvider::Create(
            Size(), msaa_sample_count_, color_params_,
            CanvasResourceProvider::kAcceleratedCompositedResourceUsage,
            std::move(context_provider_wrapper));

    if (!resource_provider)
      ReportResourceProviderCreationFailure();

    // The current paradigm does not support switching from accelerated to
    // non-accelerated, which would be tricky due to changes to the layer tree,
    // which can only happen at specific times during the document lifecycle.
    // Therefore, we can only accept the restored surface if it is accelerated.
    if (resource_provider && resource_provider->IsAccelerated()) {
      resource_provider_ = std::move(resource_provider);
      // FIXME: draw sad canvas picture into new buffer crbug.com/243842
    }
    context_lost_ = false;
  }
  if (image_buffer_)
    image_buffer_->UpdateGPUMemoryUsage();

  return resource_provider_.get();
}

bool Canvas2DLayerBridge::PrepareTextureMailbox(
    viz::TextureMailbox* out_mailbox,
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  if (destruction_in_progress_) {
    // It can be hit in the following sequence.
    // 1. Canvas draws something.
    // 2. The compositor begins the frame.
    // 3. Javascript makes a context be lost.
    // 4. Here.
    return false;
  }

  DCHECK(layer_);  // This explodes if FinalizeFrame() was not called.

  frames_since_last_commit_ = 0;
  if (rate_limiter_) {
    rate_limiter_->Reset();
  }

  // if hibernating but not hidden, we want to wake up from
  // hibernation
  if ((IsHibernating() || software_rendering_while_hidden_) && IsHidden())
    return false;

  if (!IsValid())
    return false;

  // If the context is lost, we don't know if we should be producing GPU or
  // software frames, until we get a new context, since the compositor will
  // be trying to get a new context and may change modes.
  if (!GetOrCreateResourceProvider())
    return false;

  if (!resource_provider_->IsAccelerated()) {
  }

  FlushRecording();
  if (resource_provider_->PrepareTextureMailbox(out_mailbox,
                                                out_release_callback)) {
    out_mailbox->set_color_space(color_params_.GetSamplerGfxColorSpace());
    return true;
  }
  return false;
}

WebLayer* Canvas2DLayerBridge::Layer() {
  DCHECK(!destruction_in_progress_);
  // Trigger lazy layer creation
  GetOrCreateResourceProvider(kPreferAcceleration);
  return layer_ ? layer_->Layer() : nullptr;
}

void Canvas2DLayerBridge::DidDraw(const FloatRect& rect) {
  if (is_deferral_enabled_) {
    have_recorded_draw_commands_ = true;
    IntRect pixel_bounds = EnclosingIntRect(rect);
    CheckedNumeric<int> pixel_bounds_size = pixel_bounds.Width();
    pixel_bounds_size *= pixel_bounds.Height();
    recording_pixel_count_ += pixel_bounds_size;
    if (!recording_pixel_count_.IsValid()) {
      DisableDeferral(kDisableDeferralReasonExpensiveOverdrawHeuristic);
      return;
    }
    CheckedNumeric<int> threshold_size = Size().Width();
    threshold_size *= Size().Height();
    threshold_size *= CanvasHeuristicParameters::kExpensiveOverdrawThreshold;
    if (!threshold_size.IsValid()) {
      DisableDeferral(kDisableDeferralReasonExpensiveOverdrawHeuristic);
      return;
    }
    if (recording_pixel_count_.ValueOrDie() >= threshold_size.ValueOrDie()) {
      DisableDeferral(kDisableDeferralReasonExpensiveOverdrawHeuristic);
    }
  }
}

void Canvas2DLayerBridge::FinalizeFrame() {
  TRACE_EVENT0("blink", "Canvas2DLayerBridge::FinalizeFrame");
  DCHECK(!destruction_in_progress_);

  // Make sure surface is ready for painting: fix the rendering mode now
  // because it will be too late during the paint invalidation phase.
  if (!GetOrCreateResourceProvider(kPreferAcceleration))
    return;

  ++frames_since_last_commit_;

  if (frames_since_last_commit_ >= 2) {
    GetOrCreateResourceProvider()->FlushSkia();
    if (IsAccelerated()) {
      if (!rate_limiter_) {
        rate_limiter_ =
            SharedContextRateLimiter::Create(MaxCanvasAnimationBacklog);
      }
    }
  }

  if (rate_limiter_) {
    rate_limiter_->Tick();
  }
}

void Canvas2DLayerBridge::DoPaintInvalidation(const FloatRect& dirty_rect) {
  DCHECK(!destruction_in_progress_);
  if (layer_ && acceleration_mode_ != kDisableAcceleration)
    layer_->Layer()->InvalidateRect(EnclosingIntRect(dirty_rect));
}

scoped_refptr<StaticBitmapImage> Canvas2DLayerBridge::NewImageSnapshot(
    AccelerationHint hint,
    SnapshotReason) {
  if (IsHibernating())
    return StaticBitmapImage::Create(hibernation_image_);
  if (!IsValid())
    return nullptr;
  if (!GetOrCreateResourceProvider(hint))
    return nullptr;
  FlushRecording();
  return GetOrCreateResourceProvider()->Snapshot();
}

void Canvas2DLayerBridge::WillOverwriteCanvas() {
  SkipQueuedDrawCommands();
}

void Canvas2DLayerBridge::Logger::ReportHibernationEvent(
    HibernationEvent event) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, hibernation_histogram,
                      ("Canvas.HibernationEvents", kHibernationEventCount));
  hibernation_histogram.Count(event);
}

}  // namespace blink
