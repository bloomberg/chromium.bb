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
#include "components/viz/common/quads/texture_mailbox.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/CanvasHeuristicParameters.h"
#include "platform/graphics/CanvasMetrics.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/WebGraphicsContext3DProviderWrapper.h"
#include "platform/graphics/gpu/SharedContextRateLimiter.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/WebTraceLocation.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace {
enum {
  InvalidMailboxIndex = -1,
  MaxCanvasAnimationBacklog = 2,  // Make sure the the GPU is never more than
                                  // two animation frames behind.
};

static void ReleaseMailboxImageResource(
    RefPtr<blink::StaticBitmapImage>&& image,
    bool lost_resource) {
  if (lost_resource)
    image->Abandon();
  // Image going out of scope takes care of resource clean-up in
  // AccelStaticBitmapImage and MailboxTextureHolder destructors.
}

// Resets Skia's texture bindings. This method should be called after
// changing texture bindings.
static void ResetSkiaTextureBinding(
    WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  if (!context_provider_wrapper)
    return;
  GrContext* gr_context =
      context_provider_wrapper->ContextProvider()->GetGrContext();
  if (gr_context)
    gr_context->resetContext(kTextureBinding_GrGLBackendState);
}

// Releases all resources associated with a CHROMIUM image.
static void DeleteCHROMIUMImage(
    WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    const GLuint& image_id,
    const GLuint& texture_id) {
  if (!context_provider_wrapper)
    return;
  gpu::gles2::GLES2Interface* gl =
      context_provider_wrapper->ContextProvider()->ContextGL();

  if (gl) {
    GLenum target = GC3D_TEXTURE_RECTANGLE_ARB;
    gl->BindTexture(target, texture_id);
    gl->ReleaseTexImage2DCHROMIUM(target, image_id);
    gl->DestroyImageCHROMIUM(image_id);
    gl->DeleteTextures(1, &texture_id);
    gl->BindTexture(target, 0);
    gpu_memory_buffer.reset();

    ResetSkiaTextureBinding(context_provider_wrapper);
  }
}

}  // namespace

namespace blink {

struct Canvas2DLayerBridge::ImageInfo : public RefCounted<ImageInfo> {
  ImageInfo(std::unique_ptr<gfx::GpuMemoryBuffer>,
            GLuint image_id,
            GLuint texture_id);
  ~ImageInfo();

  // The backing buffer.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;

  // The id of the CHROMIUM image.
  const GLuint image_id_;

  // The id of the texture bound to the CHROMIUM image.
  const GLuint texture_id_;
};

static sk_sp<SkSurface> CreateSkSurface(GrContext* gr,
                                        const IntSize& size,
                                        int msaa_sample_count,
                                        const CanvasColorParams& color_params,
                                        bool* surface_is_accelerated) {
  if (gr)
    gr->resetContext();

  // If we need color correction for all color spaces, we set the proper color
  // space when creating the surface. If color correct rendering is only toward
  // SRGB, we leave the surface with no color space. The painting canvas will
  // get wrapped with a proper SkColorSpaceXformCanvas in GetOrCreateSurface().
  sk_sp<SkColorSpace> color_space = nullptr;
  if (RuntimeEnabledFeatures::ColorCanvasExtensionsEnabled())
    color_space = color_params.GetSkColorSpaceForSkSurfaces();
  SkImageInfo info = SkImageInfo::Make(
      size.Width(), size.Height(), color_params.GetSkColorType(),
      color_params.GetSkAlphaType(), color_space);
  SkSurfaceProps disable_lcd_props(0, kUnknown_SkPixelGeometry);
  sk_sp<SkSurface> surface;

  if (gr) {
    *surface_is_accelerated = true;
    surface = SkSurface::MakeRenderTarget(gr, SkBudgeted::kNo, info,
                                          msaa_sample_count,
                                          color_params.GetSkSurfaceProps());
  }

  if (!surface) {
    *surface_is_accelerated = false;
    surface = SkSurface::MakeRaster(info, color_params.GetSkSurfaceProps());
  }

  if (surface) {
    if (color_params.GetOpacityMode() == kOpaque) {
      surface->getCanvas()->clear(SK_ColorBLACK);
    } else {
      surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    }
  }
  return surface;
}

Canvas2DLayerBridge::Canvas2DLayerBridge(const IntSize& size,
                                         int msaa_sample_count,
                                         AccelerationMode acceleration_mode,
                                         const CanvasColorParams& color_params,
                                         bool is_unit_test)
    : ImageBufferSurface(size, color_params),
      logger_(WTF::WrapUnique(new Logger)),
      weak_ptr_factory_(this),
      image_buffer_(0),
      msaa_sample_count_(msaa_sample_count),
      bytes_allocated_(0),
      have_recorded_draw_commands_(false),
      destruction_in_progress_(false),
      filter_quality_(kLow_SkFilterQuality),
      is_hidden_(false),
      is_deferral_enabled_(true),
      software_rendering_while_hidden_(false),
      last_image_id_(0),
      last_filter_(GL_LINEAR),
      acceleration_mode_(acceleration_mode),
      size_(size),
      color_params_(color_params) {
  if (acceleration_mode != kDisableAcceleration) {
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
    DCHECK(context_provider_wrapper_);
    DCHECK(
        !context_provider_wrapper_->ContextProvider()->IsSoftwareRendering());
  }
  // Used by browser tests to detect the use of a Canvas2DLayerBridge.
  TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation",
                       TRACE_EVENT_SCOPE_GLOBAL);
  StartRecording();
  if (!is_unit_test)
    Init();
}

Canvas2DLayerBridge::~Canvas2DLayerBridge() {
  BeginDestruction();
  DCHECK(destruction_in_progress_);
  if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled())
    ClearCHROMIUMImageCache();
  layer_.reset();
}

void Canvas2DLayerBridge::Init() {
  Clear();
  if (CheckSurfaceValid())
    FlushInternal();
}

void Canvas2DLayerBridge::StartRecording() {
  DCHECK(is_deferral_enabled_);
  recorder_ = WTF::WrapUnique(new PaintRecorder);
  PaintCanvas* canvas =
      recorder_->beginRecording(size_.Width(), size_.Height());
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

void Canvas2DLayerBridge::ResetSurface() {
  surface_paint_canvas_.reset();
  surface_.reset();
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

  if (accelerate && (!context_provider_wrapper_ ||
                     context_provider_wrapper_->ContextProvider()
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
  if (layer_) {
    // We don't check |m_surface|, so this returns true if context was lost
    // (|m_surface| is null) with restoration pending.
    return true;
  }
  if (surface_)  // && !m_layer is implied
    return false;

  // Whether or not to accelerate is not yet resolved. Determine whether
  // immediate presentation of the canvas would result in the canvas being
  // accelerated. Presentation is assumed to be a 'PreferAcceleration'
  // operation.
  return ShouldAccelerate(kPreferAcceleration);
}

GLenum Canvas2DLayerBridge::GetGLFilter() {
  return filter_quality_ == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
}

bool Canvas2DLayerBridge::PrepareGpuMemoryBufferMailboxFromImage(
    SkImage* image,
    MailboxInfo* info,
    viz::TextureMailbox* out_mailbox) {
  // Need to flush skia's internal queue, because the texture is about to be
  // accessed directly.
  GrContext* gr_context =
      context_provider_wrapper_->ContextProvider()->GetGrContext();
  gr_context->flush();

  RefPtr<ImageInfo> image_info = CreateGpuMemoryBufferBackedTexture();
  if (!image_info)
    return false;

  gpu::gles2::GLES2Interface* gl = ContextGL();
  if (!gl)
    return false;

  GLuint image_texture =
      skia::GrBackendObjectToGrGLTextureInfo(image->getTextureHandle(true))
          ->fID;
  GLenum texture_target = GC3D_TEXTURE_RECTANGLE_ARB;
  gl->CopySubTextureCHROMIUM(
      image_texture, 0, texture_target, image_info->texture_id_, 0, 0, 0, 0, 0,
      size_.Width(), size_.Height(), GL_FALSE, GL_FALSE, GL_FALSE);

  gpu::Mailbox mailbox;
  gl->GenMailboxCHROMIUM(mailbox.name);
  gl->ProduceTextureDirectCHROMIUM(image_info->texture_id_, texture_target,
                                   mailbox.name);

  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->Flush();
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  info->image_info_ = image_info;
  bool is_overlay_candidate = true;

  *out_mailbox = viz::TextureMailbox(mailbox, sync_token, texture_target,
                                     gfx::Size(size_), is_overlay_candidate);
  out_mailbox->set_color_space(color_params_.GetSamplerGfxColorSpace());
  image_info->gpu_memory_buffer_->SetColorSpaceForScanout(
      color_params_.GetStorageGfxColorSpace());

  gl->BindTexture(GC3D_TEXTURE_RECTANGLE_ARB, 0);

  // Because we are changing the texture binding without going through skia,
  // we must dirty the context.
  ResetSkiaTextureBinding(context_provider_wrapper_);

  return true;
}

RefPtr<Canvas2DLayerBridge::ImageInfo>
Canvas2DLayerBridge::CreateGpuMemoryBufferBackedTexture() {
  if (!image_info_cache_.IsEmpty()) {
    RefPtr<Canvas2DLayerBridge::ImageInfo> info = image_info_cache_.back();
    image_info_cache_.pop_back();
    return info;
  }

  gpu::gles2::GLES2Interface* gl = ContextGL();
  if (!gl)
    return nullptr;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      Platform::Current()->GetGpuMemoryBufferManager();
  if (!gpu_memory_buffer_manager)
    return nullptr;

  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
          gfx::Size(size_), gfx::BufferFormat::RGBA_8888,
          gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle);
  if (!gpu_memory_buffer)
    return nullptr;

  GLuint image_id =
      gl->CreateImageCHROMIUM(gpu_memory_buffer->AsClientBuffer(),
                              size_.Width(), size_.Height(), GL_RGBA);
  if (!image_id)
    return nullptr;

  GLuint texture_id;
  gl->GenTextures(1, &texture_id);
  GLenum target = GC3D_TEXTURE_RECTANGLE_ARB;
  gl->BindTexture(target, texture_id);
  gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GetGLFilter());
  gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GetGLFilter());
  gl->TexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->BindTexImage2DCHROMIUM(target, image_id);

  ResetSkiaTextureBinding(context_provider_wrapper_);

  return WTF::AdoptRef(new Canvas2DLayerBridge::ImageInfo(
      std::move(gpu_memory_buffer), image_id, texture_id));
}

void Canvas2DLayerBridge::ClearCHROMIUMImageCache() {
  for (const auto& it : image_info_cache_) {
    DeleteCHROMIUMImage(context_provider_wrapper_,
                        std::move(it->gpu_memory_buffer_), it->image_id_,
                        it->texture_id_);
  }
  image_info_cache_.clear();
}

bool Canvas2DLayerBridge::PrepareMailboxFromImage(
    RefPtr<StaticBitmapImage>&& image,
    MailboxInfo* mailbox_info,
    viz::TextureMailbox* out_mailbox) {
  if (!context_provider_wrapper_)
    return false;

  GrContext* gr_context =
      context_provider_wrapper_->ContextProvider()->GetGrContext();
  if (!gr_context) {
    mailbox_info->image_ = std::move(image);
    // For testing, skip GL stuff when using a mock graphics context.
    return true;
  }

  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();

  if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
    if (PrepareGpuMemoryBufferMailboxFromImage(skia_image.get(), mailbox_info,
                                               out_mailbox))
      return true;
    // Note: if GpuMemoryBuffer-backed texture creation failed we fall back to
    // the non-GpuMemoryBuffer path.
  }

  mailbox_info->image_ = std::move(image);

  if (context_provider_wrapper_->ContextProvider()
          ->GetCapabilities()
          .disable_2d_canvas_copy_on_write) {
    surface_->notifyContentWillChange(SkSurface::kRetain_ContentChangeMode);
  }

  // Need to flush skia's internal queue, because the texture is about to be
  // accessed directly.
  gr_context->flush();

  // Because of texture sharing with the compositor, we must invalidate
  // the state cached in skia so that the deferred copy on write
  // in SkSurface_Gpu does not make any false assumptions.
  skia_image->getTexture()->textureParamsModified();

  gpu::gles2::GLES2Interface* gl = ContextGL();
  if (!gl)
    return false;

  GLuint texture_id =
      skia::GrBackendObjectToGrGLTextureInfo(skia_image->getTextureHandle(true))
          ->fID;
  gl->BindTexture(GL_TEXTURE_2D, texture_id);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GetGLFilter());
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetGLFilter());
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  mailbox_info->image_->EnsureMailbox(kUnverifiedSyncToken);

  *out_mailbox =
      viz::TextureMailbox(mailbox_info->image_->GetMailbox(),
                          mailbox_info->image_->GetSyncToken(), GL_TEXTURE_2D);

  if (IsHidden()) {
    // With hidden canvases, we release the SkImage immediately because
    // there is no need for animations to be double buffered. Deleteing
    // the SkImage will resulting skia's copy-on-write being skipped.
    mailbox_info->image_ = nullptr;
  }

  gl->BindTexture(GL_TEXTURE_2D, 0);
  // Because we are changing the texture binding without going through skia,
  // we must dirty the context.
  ResetSkiaTextureBinding(context_provider_wrapper_);
  return true;
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

  if (!surface_) {
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
      SkSurface::MakeRasterN32Premul(size_.Width(), size_.Height());
  if (!temp_hibernation_surface) {
    logger_->ReportHibernationEvent(kHibernationAbortedDueToAllocationFailure);
    return;
  }
  // No HibernationEvent reported on success. This is on purppose to avoid
  // non-complementary stats. Each HibernationScheduled event is paired with
  // exactly one failure or exit event.
  FlushRecordingOnly();
  // The following checks that the flush succeeded, which should always be the
  // case because flushRecordingOnly should only fail it it fails to allocate
  // a surface, and we have an early exit at the top of this function for when
  // 'this' does not already have a surface.
  DCHECK(!have_recorded_draw_commands_);
  SkPaint copy_paint;
  copy_paint.setBlendMode(SkBlendMode::kSrc);
  surface_->draw(temp_hibernation_surface->getCanvas(), 0, 0,
                 &copy_paint);  // GPU readback
  hibernation_image_ = temp_hibernation_surface->makeImageSnapshot();
  ResetSurface();
  layer_->ClearTexture();
  if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled())
    ClearCHROMIUMImageCache();
  // shouldBeDirectComposited() may have changed.
  if (image_buffer_)
    image_buffer_->SetNeedsCompositingUpdate();
  logger_->DidStartHibernating();
}

void Canvas2DLayerBridge::ReportSurfaceCreationFailure() {
  if (!surface_creation_failed_at_least_once_) {
    // Only count the failure once per instance so that the histogram may
    // reflect the proportion of Canvas2DLayerBridge instances with surface
    // allocation failures.
    CanvasMetrics::CountCanvasContextUsage(
        CanvasMetrics::kGPUAccelerated2DCanvasSurfaceCreationFailed);
    surface_creation_failed_at_least_once_ = true;
  }
}

SkSurface* Canvas2DLayerBridge::GetOrCreateSurface(AccelerationHint hint) {
  if (surface_)
    return surface_.get();

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

  GrContext* gr =
      want_acceleration && context_provider_wrapper_
          ? context_provider_wrapper_->ContextProvider()->GetGrContext()
          : nullptr;

  bool surface_is_accelerated;
  surface_ = CreateSkSurface(gr, size_, msaa_sample_count_, color_params_,
                             &surface_is_accelerated);
  if (!surface_)
    return nullptr;
  if (!color_params_.LinearPixelMath()) {
    surface_paint_canvas_ = WTF::WrapUnique(new SkiaPaintCanvas(
        surface_->getCanvas(), color_params_.GetSkColorSpace()));
  } else {
    surface_paint_canvas_ =
        WTF::WrapUnique(new SkiaPaintCanvas(surface_->getCanvas()));
  }

  if (surface_) {
    // Always save an initial frame, to support resetting the top level matrix
    // and clip.
    surface_paint_canvas_->save();
  } else {
    ReportSurfaceCreationFailure();
  }

  if (surface_ && surface_is_accelerated && !layer_) {
    layer_ =
        Platform::Current()->CompositorSupport()->CreateExternalTextureLayer(
            this);
    layer_->SetOpaque(ColorParams().GetOpacityMode() == kOpaque);
    layer_->SetBlendBackgroundColor(ColorParams().GetOpacityMode() != kOpaque);
    GraphicsLayer::RegisterContentsLayer(layer_->Layer());
    layer_->SetNearestNeighbor(filter_quality_ == kNone_SkFilterQuality);
  }

  if (surface_ && IsHibernating()) {
    if (surface_is_accelerated) {
      logger_->ReportHibernationEvent(kHibernationEndedNormally);
    } else {
      if (IsHidden()) {
        logger_->ReportHibernationEvent(
            kHibernationEndedWithSwitchToBackgroundRendering);
      } else {
        logger_->ReportHibernationEvent(kHibernationEndedWithFallbackToSW);
      }
    }

    SkPaint copy_paint;
    copy_paint.setBlendMode(SkBlendMode::kSrc);
    surface_->getCanvas()->drawImage(hibernation_image_.get(), 0, 0,
                                     &copy_paint);
    hibernation_image_.reset();

    if (image_buffer_) {
      image_buffer_->UpdateGPUMemoryUsage();

      if (!is_deferral_enabled_)
        image_buffer_->ResetCanvas(surface_paint_canvas_.get());

      // shouldBeDirectComposited() may have changed.
      image_buffer_->SetNeedsCompositingUpdate();
    }
  }

  return surface_.get();
}

PaintCanvas* Canvas2DLayerBridge::Canvas() {
  if (!is_deferral_enabled_) {
    GetOrCreateSurface();
    return surface_paint_canvas_.get();
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
  FlushRecordingOnly();
  // Because we will be discarding the recorder, if the flush failed
  // content will be lost -> force m_haveRecordedDrawCommands to false
  have_recorded_draw_commands_ = false;

  is_deferral_enabled_ = false;
  recorder_.reset();
  // install the current matrix/clip stack onto the immediate canvas
  GetOrCreateSurface();
  if (image_buffer_ && surface_paint_canvas_)
    image_buffer_->ResetCanvas(surface_paint_canvas_.get());
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
  ResetSurface();

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
  if (layer_)
    layer_->SetNearestNeighbor(filter_quality_ == kNone_SkFilterQuality);
}

void Canvas2DLayerBridge::SetIsHidden(bool hidden) {
  bool new_hidden_value = hidden || destruction_in_progress_;
  if (is_hidden_ == new_hidden_value)
    return;

  is_hidden_ = new_hidden_value;
  if (CANVAS2D_HIBERNATION_ENABLED && surface_ && IsHidden() &&
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
    FlushRecordingOnly();
    SkPaint copy_paint;
    copy_paint.setBlendMode(SkBlendMode::kSrc);

    sk_sp<SkSurface> old_surface = std::move(surface_);
    ResetSurface();

    software_rendering_while_hidden_ = false;
    SkSurface* new_surface =
        GetOrCreateSurface(kPreferAccelerationAfterVisibilityChange);
    if (new_surface) {
      if (old_surface)
        old_surface->draw(new_surface->getCanvas(), 0, 0, &copy_paint);
      if (image_buffer_ && !is_deferral_enabled_) {
        image_buffer_->ResetCanvas(surface_paint_canvas_.get());
      }
    }
  }
  if (!IsHidden() && IsHibernating()) {
    GetOrCreateSurface();  // Rude awakening
  }
}

bool Canvas2DLayerBridge::WritePixels(const SkImageInfo& orig_info,
                                      const void* pixels,
                                      size_t row_bytes,
                                      int x,
                                      int y) {
  if (!GetOrCreateSurface())
    return false;
  if (x <= 0 && y <= 0 && x + orig_info.width() >= size_.Width() &&
      y + orig_info.height() >= size_.Height()) {
    SkipQueuedDrawCommands();
  } else {
    FlushInternal();
  }
  DCHECK(!have_recorded_draw_commands_);
  // call write pixels on the surface, not the recording canvas.
  // No need to call beginDirectSurfaceAccessModeIfNeeded() because writePixels
  // ignores the matrix and clip state.
  return GetOrCreateSurface()->getCanvas()->writePixels(orig_info, pixels,
                                                        row_bytes, x, y);
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

void Canvas2DLayerBridge::FlushRecordingOnly() {
  DCHECK(!destruction_in_progress_);

  if (have_recorded_draw_commands_ && GetOrCreateSurface()) {
    TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushRecordingOnly");

    // For legacy canvases, transform all input colors and images to the target
    // space using a SkCreateColorSpaceXformCanvas. This ensures blending will
    // be done using target space pixel values.
    SkCanvas* canvas = GetOrCreateSurface()->getCanvas();
    std::unique_ptr<SkCanvas> color_transform_canvas;
    if (!color_params_.LinearPixelMath()) {
      color_transform_canvas = SkCreateColorSpaceXformCanvas(
          canvas, color_params_.GetSkColorSpace());
      canvas = color_transform_canvas.get();
    }

    recorder_->finishRecordingAsPicture()->Playback(canvas);
    if (is_deferral_enabled_)
      StartRecording();
    have_recorded_draw_commands_ = false;
  }
}

void Canvas2DLayerBridge::FlushInternal() {
  if (!did_draw_since_last_flush_)
    return;
  TRACE_EVENT0("cc", "Canvas2DLayerBridge::flush");
  if (!GetOrCreateSurface())
    return;
  FlushRecordingOnly();
  GetOrCreateSurface()->getCanvas()->flush();
  did_draw_since_last_flush_ = false;
}

void Canvas2DLayerBridge::Flush(FlushReason reason) {
  FlushInternal();
}

void Canvas2DLayerBridge::FlushGpuInternal() {
  FlushInternal();
  gpu::gles2::GLES2Interface* gl = ContextGL();
  if (IsAccelerated() && gl && did_draw_since_last_gpu_flush_) {
    TRACE_EVENT0("cc", "Canvas2DLayerBridge::flushGpu");
    gl->Flush();
    did_draw_since_last_gpu_flush_ = false;
  }
}

void Canvas2DLayerBridge::FlushGpu(FlushReason reason) {
  FlushGpuInternal();
}

gpu::gles2::GLES2Interface* Canvas2DLayerBridge::ContextGL() {
  // Check on m_layer is necessary because contextGL() may be called during
  // the destruction of m_layer
  if (layer_ && acceleration_mode_ != kDisableAcceleration &&
      !destruction_in_progress_) {
    // Call checkSurfaceValid to ensure the rate limiter is disabled if the
    // context is lost.
    if (!IsValid())
      return nullptr;
  }
  return context_provider_wrapper_
             ? context_provider_wrapper_->ContextProvider()->ContextGL()
             : nullptr;
}

bool Canvas2DLayerBridge::IsValid() const {
  return const_cast<Canvas2DLayerBridge*>(this)->CheckSurfaceValid();
}

bool Canvas2DLayerBridge::CheckSurfaceValid() {
  DCHECK(!destruction_in_progress_);
  if (destruction_in_progress_)
    return false;
  if (IsHibernating())
    return true;
  if (!layer_ || acceleration_mode_ == kDisableAcceleration)
    return true;
  if (!surface_)
    return false;
  if (!context_provider_wrapper_ ||
      context_provider_wrapper_->ContextProvider()
              ->ContextGL()
              ->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    ResetSurface();
    if (image_buffer_)
      image_buffer_->NotifySurfaceInvalid();
    CanvasMetrics::CountCanvasContextUsage(
        CanvasMetrics::kAccelerated2DCanvasGPUContextLost);
  }
  return surface_.get();
}

bool Canvas2DLayerBridge::Restore() {
  DCHECK(!destruction_in_progress_);
  if (destruction_in_progress_ || !IsAccelerated())
    return false;
  DCHECK(!surface_);

  gpu::gles2::GLES2Interface* shared_gl = nullptr;
  layer_->ClearTexture();
  context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  if (context_provider_wrapper_)
    shared_gl = context_provider_wrapper_->ContextProvider()->ContextGL();

  if (shared_gl && shared_gl->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    GrContext* gr_ctx =
        context_provider_wrapper_->ContextProvider()->GetGrContext();
    bool surface_is_accelerated;
    sk_sp<SkSurface> surface(CreateSkSurface(gr_ctx, size_, msaa_sample_count_,
                                             color_params_,
                                             &surface_is_accelerated));
    if (!surface_)
      ReportSurfaceCreationFailure();

    // The current paradigm does not support switching from accelerated to
    // non-accelerated, which would be tricky due to changes to the layer tree,
    // which can only happen at specific times during the document lifecycle.
    // Therefore, we can only accept the restored surface if it is accelerated.
    if (surface && surface_is_accelerated) {
      surface_ = std::move(surface);
      // FIXME: draw sad canvas picture into new buffer crbug.com/243842
    }
  }
  if (image_buffer_)
    image_buffer_->UpdateGPUMemoryUsage();

  return surface_.get();
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

  frames_since_last_commit_ = 0;
  if (rate_limiter_) {
    rate_limiter_->Reset();
  }

  // If the context is lost, we don't know if we should be producing GPU or
  // software frames, until we get a new context, since the compositor will
  // be trying to get a new context and may change modes.
  if (!context_provider_wrapper_ ||
      context_provider_wrapper_->ContextProvider()
              ->ContextGL()
              ->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    return false;

  DCHECK(IsAccelerated() || IsHibernating() ||
         software_rendering_while_hidden_);

  // if hibernating but not hidden, we want to wake up from
  // hibernation
  if ((IsHibernating() || software_rendering_while_hidden_) && IsHidden())
    return false;

  RefPtr<StaticBitmapImage> image =
      NewImageSnapshot(kPreferAcceleration, kSnapshotReasonUnknown);
  if (!image || !image->IsValid() || !image->IsTextureBacked())
    return false;

  {
    sk_sp<SkImage> skImage = image->PaintImageForCurrentFrame().GetSkImage();
    // Early exit if canvas was not drawn to since last prepareMailbox.
    GLenum filter = GetGLFilter();
    if (skImage->uniqueID() == last_image_id_ && filter == last_filter_)
      return false;
    last_image_id_ = skImage->uniqueID();
    last_filter_ = filter;
  }

  std::unique_ptr<MailboxInfo> info = WTF::WrapUnique(new MailboxInfo());
  if (!PrepareMailboxFromImage(std::move(image), info.get(), out_mailbox))
    return false;
  out_mailbox->set_nearest_neighbor(GetGLFilter() == GL_NEAREST);
  out_mailbox->set_color_space(color_params_.GetSamplerGfxColorSpace());

  auto func =
      WTF::Bind(&ReleaseFrameResources, weak_ptr_factory_.CreateWeakPtr(),
                context_provider_wrapper_, WTF::Passed(std::move(info)),
                out_mailbox->mailbox());
  *out_release_callback = viz::SingleReleaseCallback::Create(
      ConvertToBaseCallback(std::move(func)));
  return true;
}

// TODO(xidachen): Make this a static local function once we deprecate the
// MailboxInfo structure and pass all the resources in that structure as params
// to this function.
void Canvas2DLayerBridge::ReleaseFrameResources(
    WeakPtr<Canvas2DLayerBridge> layer_bridge,
    WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    std::unique_ptr<MailboxInfo> released_mailbox_info,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  if (sync_token.HasData() && context_provider_wrapper) {
    context_provider_wrapper->ContextProvider()
        ->ContextGL()
        ->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }
  bool context_or_layer_bridge_lost = true;
  if (layer_bridge) {
    DCHECK(layer_bridge->IsAccelerated() || layer_bridge->IsHibernating());
    context_or_layer_bridge_lost =
        !layer_bridge->IsHibernating() &&
        (!layer_bridge->surface_ || !layer_bridge->context_provider_wrapper_ ||
         layer_bridge->context_provider_wrapper_->ContextProvider()
                 ->ContextGL()
                 ->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  }

  if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
    RefPtr<ImageInfo>& info = released_mailbox_info->image_info_;
    if (info) {
      if (lost_resource || context_or_layer_bridge_lost) {
        DeleteCHROMIUMImage(context_provider_wrapper,
                            std::move(info->gpu_memory_buffer_),
                            info->image_id_, info->texture_id_);
      } else {
        layer_bridge->image_info_cache_.push_back(info);
      }
    }
  }

  // Invalidate texture state in case the compositor altered it since the
  // copy-on-write.
  if (released_mailbox_info->image_) {
    if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled())
      DCHECK(!released_mailbox_info->image_info_);
    bool layer_bridge_with_valid_context =
        layer_bridge && !context_or_layer_bridge_lost;
    if (layer_bridge_with_valid_context || !layer_bridge) {
      ReleaseMailboxImageResource(std::move(released_mailbox_info->image_),
                                  lost_resource);
    }
  }

  if (layer_bridge && layer_bridge->acceleration_mode_ == kDisableAcceleration)
    layer_bridge->layer_.reset();
}

WebLayer* Canvas2DLayerBridge::Layer() const {
  DCHECK(!destruction_in_progress_);
  DCHECK(layer_);
  return layer_->Layer();
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
    CheckedNumeric<int> threshold_size = size_.Width();
    threshold_size *= size_.Height();
    threshold_size *= CanvasHeuristicParameters::kExpensiveOverdrawThreshold;
    if (!threshold_size.IsValid()) {
      DisableDeferral(kDisableDeferralReasonExpensiveOverdrawHeuristic);
      return;
    }
    if (recording_pixel_count_.ValueOrDie() >= threshold_size.ValueOrDie()) {
      DisableDeferral(kDisableDeferralReasonExpensiveOverdrawHeuristic);
    }
  }
  did_draw_since_last_flush_ = true;
  did_draw_since_last_gpu_flush_ = true;
}

void Canvas2DLayerBridge::FinalizeFrame() {
  TRACE_EVENT0("blink", "Canvas2DLayerBridge::finalizeFrame");
  DCHECK(!destruction_in_progress_);

  // Make sure surface is ready for painting: fix the rendering mode now
  // because it will be too late during the paint invalidation phase.
  GetOrCreateSurface(kPreferAcceleration);

  ++frames_since_last_commit_;

  if (frames_since_last_commit_ >= 2) {
    if (IsAccelerated()) {
      FlushGpuInternal();
      if (!rate_limiter_) {
        rate_limiter_ =
            SharedContextRateLimiter::Create(MaxCanvasAnimationBacklog);
      }
    } else {
      FlushInternal();
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

RefPtr<StaticBitmapImage> Canvas2DLayerBridge::NewImageSnapshot(
    AccelerationHint hint,
    SnapshotReason) {
  if (IsHibernating())
    return StaticBitmapImage::Create(hibernation_image_);
  if (!IsValid())
    return nullptr;
  if (!GetOrCreateSurface(hint))
    return nullptr;
  FlushInternal();
  if (IsAccelerated()) {
    // A readback operation may alter the texture parameters, which may affect
    // the compositor's behavior. Therefore, we must trigger copy-on-write
    // even though we are not technically writing to the texture, only to its
    // parameters.
    GetOrCreateSurface()->notifyContentWillChange(
        SkSurface::kRetain_ContentChangeMode);
  }
  RefPtr<StaticBitmapImage> image = StaticBitmapImage::Create(
      surface_->makeImageSnapshot(), ContextProviderWrapper());
  if (image->IsTextureBacked()) {
    static_cast<AcceleratedStaticBitmapImage*>(image.get())
        ->RetainOriginalSkImageForCopyOnWrite();
  }
  return image;
}

void Canvas2DLayerBridge::WillOverwriteCanvas() {
  SkipQueuedDrawCommands();
}

Canvas2DLayerBridge::ImageInfo::ImageInfo(
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    GLuint image_id,
    GLuint texture_id)
    : gpu_memory_buffer_(std::move(gpu_memory_buffer)),
      image_id_(image_id),
      texture_id_(texture_id) {
  DCHECK(gpu_memory_buffer_);
  DCHECK(image_id_);
  DCHECK(texture_id_);
}

Canvas2DLayerBridge::ImageInfo::~ImageInfo() {}

Canvas2DLayerBridge::MailboxInfo::MailboxInfo() = default;
Canvas2DLayerBridge::MailboxInfo::MailboxInfo(const MailboxInfo& other) =
    default;

void Canvas2DLayerBridge::Logger::ReportHibernationEvent(
    HibernationEvent event) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, hibernation_histogram,
                      ("Canvas.HibernationEvents", kHibernationEventCount));
  hibernation_histogram.Count(event);
}

}  // namespace blink
