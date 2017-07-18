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

#ifndef Canvas2DLayerBridge_h
#define Canvas2DLayerBridge_h

#include "build/build_config.h"
#include "cc/layers/texture_layer_client.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/WeakPtr.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"

#include <memory>

class SkImage;
struct SkImageInfo;

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class Canvas2DLayerBridgeTest;
class ImageBuffer;
class WebGraphicsContext3DProvider;
class SharedContextRateLimiter;

#if defined(OS_MACOSX)
// Canvas hibernation is currently disabled on MacOS X due to a bug that causes
// content loss. TODO: Find a better fix for crbug.com/588434
#define CANVAS2D_HIBERNATION_ENABLED 0

// IOSurfaces are a primitive only present on OS X.
#define USE_IOSURFACE_FOR_2D_CANVAS 1
#else
#define CANVAS2D_HIBERNATION_ENABLED 1
#define USE_IOSURFACE_FOR_2D_CANVAS 0
#endif

// TODO: Fix background rendering and remove this workaround. crbug.com/600386
#define CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU 0

class PLATFORM_EXPORT Canvas2DLayerBridge
    : public NON_EXPORTED_BASE(cc::TextureLayerClient),
      public RefCounted<Canvas2DLayerBridge> {
  WTF_MAKE_NONCOPYABLE(Canvas2DLayerBridge);

 public:
  enum AccelerationMode {
    kDisableAcceleration,
    kEnableAcceleration,
    kForceAccelerationForTesting,
  };

  Canvas2DLayerBridge(std::unique_ptr<WebGraphicsContext3DProvider>,
                      const IntSize&,
                      int msaa_sample_count,
                      OpacityMode,
                      AccelerationMode,
                      const CanvasColorParams&);

  ~Canvas2DLayerBridge() override;

  // cc::TextureLayerClient implementation.
  bool PrepareTextureMailbox(viz::TextureMailbox* out_mailbox,
                             std::unique_ptr<cc::SingleReleaseCallback>*
                                 out_release_callback) override;

  // ImageBufferSurface implementation
  void FinalizeFrame();
  void DoPaintInvalidation(const FloatRect& dirty_rect);
  void WillWritePixels();
  void WillOverwriteAllPixels();
  void WillOverwriteCanvas();
  PaintCanvas* Canvas();
  void DisableDeferral(DisableDeferralReason);
  bool CheckSurfaceValid();
  bool RestoreSurface();
  WebLayer* Layer() const;
  bool IsAccelerated() const;
  void SetFilterQuality(SkFilterQuality);
  void SetIsHidden(bool);
  void SetImageBuffer(ImageBuffer*);
  void DidDraw(const FloatRect&);
  bool WritePixels(const SkImageInfo&,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y);
  void Flush();
  void FlushGpu();
  OpacityMode GetOpacityMode() { return opacity_mode_; }
  void DontUseIdleSchedulingForTesting() {
    dont_use_idle_scheduling_for_testing_ = true;
  }

  void BeginDestruction();
  void Hibernate();
  bool IsHibernating() const { return hibernation_image_.get(); }
  const CanvasColorParams& color_params() const { return color_params_; }

  bool HasRecordedDrawCommands() { return have_recorded_draw_commands_; }

  sk_sp<SkImage> NewImageSnapshot(AccelerationHint, SnapshotReason);

  // The values of the enum entries must not change because they are used for
  // usage metrics histograms. New values can be added to the end.
  enum HibernationEvent {
    kHibernationScheduled = 0,
    kHibernationAbortedDueToDestructionWhileHibernatePending = 1,
    kHibernationAbortedDueToPendingDestruction = 2,
    kHibernationAbortedDueToVisibilityChange = 3,
    kHibernationAbortedDueGpuContextLoss = 4,
    kHibernationAbortedDueToSwitchToUnacceleratedRendering = 5,
    kHibernationAbortedDueToAllocationFailure = 6,
    kHibernationEndedNormally = 7,
    kHibernationEndedWithSwitchToBackgroundRendering = 8,
    kHibernationEndedWithFallbackToSW = 9,
    kHibernationEndedWithTeardown = 10,
    kHibernationAbortedBecauseNoSurface = 11,

    kHibernationEventCount = 12,
  };

  class PLATFORM_EXPORT Logger {
   public:
    virtual void ReportHibernationEvent(HibernationEvent);
    virtual void DidStartHibernating() {}
    virtual ~Logger() {}
  };

  void SetLoggerForTesting(std::unique_ptr<Logger>);

 private:
  void ResetSurface();

  // Callback for mailboxes given to the compositor from PrepareTextureMailbox.
  void MailboxReleased(const gpu::Mailbox&,
                       const gpu::SyncToken&,
                       bool lost_resource);
  bool IsHidden() { return is_hidden_; }

#if USE_IOSURFACE_FOR_2D_CANVAS
  // All information associated with a CHROMIUM image.
  struct ImageInfo;
#endif  // USE_IOSURFACE_FOR_2D_CANVAS

  struct MailboxInfo {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    gpu::Mailbox mailbox_;
    sk_sp<SkImage> image_;
    RefPtr<Canvas2DLayerBridge> parent_layer_bridge_;

#if USE_IOSURFACE_FOR_2D_CANVAS
    // If this mailbox wraps an IOSurface-backed texture, the ids of the
    // CHROMIUM image and the texture.
    RefPtr<ImageInfo> image_info_;
#endif  // USE_IOSURFACE_FOR_2D_CANVAS

    MailboxInfo(const MailboxInfo&);
    MailboxInfo();
  };

  gpu::gles2::GLES2Interface* ContextGL();
  void StartRecording();
  void SkipQueuedDrawCommands();
  void FlushRecordingOnly();
  void ReportSurfaceCreationFailure();

  SkSurface* GetOrCreateSurface(AccelerationHint = kPreferAcceleration);
  bool ShouldAccelerate(AccelerationHint) const;

  // Returns the GL filter associated with |m_filterQuality|.
  GLenum GetGLFilter();

#if USE_IOSURFACE_FOR_2D_CANVAS
  // Creates an IOSurface-backed texture. Copies |image| into the texture.
  // Prepares a mailbox from the texture. The caller must have created a new
  // MailboxInfo, and prepended it to |m_mailboxs|. Returns whether the
  // mailbox was successfully prepared. |mailbox| is an out parameter only
  // populated on success.
  bool PrepareIOSurfaceMailboxFromImage(SkImage*, viz::TextureMailbox*);

  // Creates an IOSurface-backed texture. Returns an ImageInfo, which is empty
  // on failure. The caller takes ownership of both the texture and the image.
  RefPtr<ImageInfo> CreateIOSurfaceBackedTexture();

  // Releases all resources associated with a CHROMIUM image.
  void DeleteCHROMIUMImage(RefPtr<ImageInfo>);

  // Releases all resources in the CHROMIUM image cache.
  void ClearCHROMIUMImageCache();
#endif  // USE_IOSURFACE_FOR_2D_CANVAS

  // Prepends a new MailboxInfo object to |m_mailboxes|.
  void CreateMailboxInfo();

  // Returns whether the mailbox was successfully prepared from the SkImage.
  // The mailbox is an out parameter only populated on success.
  bool PrepareMailboxFromImage(sk_sp<SkImage>, viz::TextureMailbox*);

  // Resets Skia's texture bindings. This method should be called after
  // changing texture bindings.
  void ResetSkiaTextureBinding();

  std::unique_ptr<PaintRecorder> recorder_;
  sk_sp<SkSurface> surface_;
  std::unique_ptr<PaintCanvas> surface_paint_canvas_;
  sk_sp<SkImage> hibernation_image_;
  int initial_surface_save_count_;
  std::unique_ptr<WebExternalTextureLayer> layer_;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider_;
  std::unique_ptr<SharedContextRateLimiter> rate_limiter_;
  std::unique_ptr<Logger> logger_;
  WeakPtrFactory<Canvas2DLayerBridge> weak_ptr_factory_;
  ImageBuffer* image_buffer_;
  int msaa_sample_count_;
  int frames_since_last_commit_ = 0;
  size_t bytes_allocated_;
  bool have_recorded_draw_commands_;
  bool destruction_in_progress_;
  SkFilterQuality filter_quality_;
  bool is_hidden_;
  bool is_deferral_enabled_;
  bool software_rendering_while_hidden_;
  bool surface_creation_failed_at_least_once_ = false;
  bool hibernation_scheduled_ = false;
  bool dont_use_idle_scheduling_for_testing_ = false;
  bool did_draw_since_last_flush_ = false;
  bool did_draw_since_last_gpu_flush_ = false;

  friend class Canvas2DLayerBridgeTest;
  friend class CanvasRenderingContext2DTest;
  friend class HTMLCanvasPainterTestForSPv2;

  uint32_t last_image_id_;

  enum {
    // We should normally not have more that two active mailboxes at a time,
    // but sometimes we may have three due to the async nature of mailbox
    // handling.
    kMaxActiveMailboxes = 3,
  };

  Deque<MailboxInfo, kMaxActiveMailboxes> mailboxes_;
  GLenum last_filter_;
  AccelerationMode acceleration_mode_;
  OpacityMode opacity_mode_;
  const IntSize size_;
  CanvasColorParams color_params_;
  int recording_pixel_count_;

#if USE_IOSURFACE_FOR_2D_CANVAS
  // Each element in this vector represents an IOSurface backed texture that
  // is ready to be reused.
  // Elements in this vector can safely be purged in low memory conditions.
  Vector<RefPtr<ImageInfo>> image_info_cache_;
#endif  // USE_IOSURFACE_FOR_2D_CANVAS
};

}  // namespace blink

#endif
