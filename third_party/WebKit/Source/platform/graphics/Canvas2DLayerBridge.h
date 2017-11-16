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

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "cc/layers/texture_layer_client.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/WeakPtr.h"
#include "public/platform/WebExternalTextureLayer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"

struct SkImageInfo;

namespace blink {

class Canvas2DLayerBridgeTest;
class CanvasResourceProvider;
class ImageBuffer;
class SharedContextRateLimiter;

#if defined(OS_MACOSX)
// Canvas hibernation is currently disabled on MacOS X due to a bug that causes
// content loss. TODO: Find a better fix for crbug.com/588434
#define CANVAS2D_HIBERNATION_ENABLED 0
#else
#define CANVAS2D_HIBERNATION_ENABLED 1
#endif

// TODO: Fix background rendering and remove this workaround. crbug.com/600386
#define CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU 0

class PLATFORM_EXPORT Canvas2DLayerBridge : public cc::TextureLayerClient,
                                            public ImageBufferSurface {
  WTF_MAKE_NONCOPYABLE(Canvas2DLayerBridge);

 public:
  enum AccelerationMode {
    kDisableAcceleration,
    kEnableAcceleration,
    kForceAccelerationForTesting,
  };

  Canvas2DLayerBridge(const IntSize&,
                      int msaa_sample_count,
                      AccelerationMode,
                      const CanvasColorParams&,
                      bool is_unit_test = false);

  ~Canvas2DLayerBridge() override;

  // cc::TextureLayerClient implementation.
  bool PrepareTextureMailbox(viz::TextureMailbox* out_mailbox,
                             std::unique_ptr<viz::SingleReleaseCallback>*
                                 out_release_callback) override;

  // ImageBufferSurface implementation
  void FinalizeFrame() override;
  void DoPaintInvalidation(const FloatRect& dirty_rect) override;
  void WillOverwriteCanvas() override;
  PaintCanvas* Canvas() override;
  void DisableDeferral(DisableDeferralReason) override;
  bool IsValid() const override;
  bool Restore() override;
  WebLayer* Layer() override;
  bool IsAccelerated() const override;
  void SetFilterQuality(SkFilterQuality) override;
  void SetIsHidden(bool) override;
  void SetImageBuffer(ImageBuffer*) override;
  void DidDraw(const FloatRect&) override;
  bool WritePixels(const SkImageInfo&,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;
  void DontUseIdleSchedulingForTesting() {
    dont_use_idle_scheduling_for_testing_ = true;
  }

  void BeginDestruction();
  void Hibernate();
  bool IsHibernating() const { return hibernation_image_; }
  const CanvasColorParams& ColorParams() const { return color_params_; }

  bool HasRecordedDrawCommands() { return have_recorded_draw_commands_; }

  scoped_refptr<StaticBitmapImage> NewImageSnapshot(AccelerationHint,
                                                    SnapshotReason);

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
  void ResetResourceProvider();
  bool IsHidden() { return is_hidden_; }
  bool CheckResourceProviderValid();

  void StartRecording();
  void SkipQueuedDrawCommands();
  void FlushRecording();
  void ReportResourceProviderCreationFailure();

  CanvasResourceProvider* GetOrCreateResourceProvider(
      AccelerationHint = kPreferAcceleration);
  bool ShouldAccelerate(AccelerationHint) const;

  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  std::unique_ptr<PaintRecorder> recorder_;
  sk_sp<SkImage> hibernation_image_;
  std::unique_ptr<WebExternalTextureLayer> layer_;
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
  bool resource_provider_creation_failed_at_least_once_ = false;
  bool hibernation_scheduled_ = false;
  bool dont_use_idle_scheduling_for_testing_ = false;
  bool context_lost_ = false;

  friend class Canvas2DLayerBridgeTest;
  friend class CanvasRenderingContext2DTest;
  friend class HTMLCanvasPainterTestForSPv2;

  AccelerationMode acceleration_mode_;
  CanvasColorParams color_params_;
  CheckedNumeric<int> recording_pixel_count_;
};

}  // namespace blink

#endif
