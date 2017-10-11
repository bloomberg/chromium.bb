// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RecordingImageBufferSurface_h
#define RecordingImageBufferSurface_h

#include <memory>
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebThread.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class ImageBuffer;
class UnacceleratedImageBufferSurface;

class PLATFORM_EXPORT RecordingImageBufferSurface : public ImageBufferSurface {
  WTF_MAKE_NONCOPYABLE(RecordingImageBufferSurface);
  USING_FAST_MALLOC(RecordingImageBufferSurface);

 public:
  enum AllowFallback : bool { kDisallowFallback, kAllowFallback };

  // If |AllowFallback| is kDisallowFallback the buffer surface should only be
  // used for one frame and should not be used for any operations which need a
  // raster surface, (i.e. WritePixels()).
  // Only GetRecord() should be used to access the resulting frame.
  RecordingImageBufferSurface(const IntSize&,
                              AllowFallback,
                              const CanvasColorParams& = CanvasColorParams());
  ~RecordingImageBufferSurface() override;

  // Implementation of ImageBufferSurface interfaces
  PaintCanvas* Canvas() override;
  void DisableDeferral(DisableDeferralReason) override;
  sk_sp<PaintRecord> GetRecord() override;
  void Flush(FlushReason) override;
  void DidDraw(const FloatRect&) override;
  bool IsValid() const override { return true; }
  bool IsRecording() const override { return !fallback_surface_; }
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;
  void WillOverwriteCanvas() override;
  void FinalizeFrame() override;
  void DoPaintInvalidation(const FloatRect&) override;
  void SetImageBuffer(ImageBuffer*) override;
  RefPtr<StaticBitmapImage> NewImageSnapshot(AccelerationHint,
                                             SnapshotReason) override;
  void Draw(GraphicsContext&,
            const FloatRect& dest_rect,
            const FloatRect& src_rect,
            SkBlendMode) override;
  bool IsExpensiveToPaint() override;
  void SetHasExpensiveOp() override { current_frame_has_expensive_op_ = true; }

  // Passthroughs to fallback surface
  bool Restore() override;
  WebLayer* Layer() const override;
  bool IsAccelerated() const override;
  void SetIsHidden(bool) override;

  // This enum is used in a UMA histogram.
  enum FallbackReason {
    kFallbackReasonUnknown =
        0,  // This value should never appear in production histograms
    kFallbackReasonCanvasNotClearedBetweenFrames = 1,
    kFallbackReasonRunawayStateStack = 2,
    kFallbackReasonWritePixels = 3,
    kFallbackReasonFlushInitialClear = 4,
    kFallbackReasonFlushForDrawImageOfWebGL = 5,
    kFallbackReasonSnapshotForGetImageData = 6,
    kFallbackReasonSnapshotForPaint = 8,
    kFallbackReasonSnapshotForToDataURL = 9,
    kFallbackReasonSnapshotForToBlob = 10,
    kFallbackReasonSnapshotForCanvasListenerCapture = 11,
    kFallbackReasonSnapshotForDrawImage = 12,
    kFallbackReasonSnapshotForCreatePattern = 13,
    kFallbackReasonExpensiveOverdrawHeuristic = 14,
    kFallbackReasonTextureBackedPattern = 15,
    kFallbackReasonDrawImageOfVideo = 16,
    kFallbackReasonDrawImageOfAnimated2dCanvas = 17,
    kFallbackReasonSubPixelTextAntiAliasingSupport = 18,
    kFallbackReasonDrawImageWithTextureBackedSourceImage = 19,
    kFallbackReasonSnapshotForTransferToImageBitmap = 20,
    kFallbackReasonSnapshotForUnitTests =
        21,  // This value should never appear in production histograms
    kFallbackReasonSnapshotGetCopiedImage = 22,
    kFallbackReasonSnapshotWebGLDrawImageIntoBuffer = 23,
    kFallbackReasonSnapshotForWebGLTexImage2D = 24,
    kFallbackReasonSnapshotForWebGLTexSubImage2D = 25,
    kFallbackReasonSnapshotForWebGLTexImage3D = 26,
    kFallbackReasonSnapshotForWebGLTexSubImage3D = 27,
    kFallbackReasonSnapshotForCopyToClipboard = 28,
    kFallbackReasonSnapshotForCreateImageBitmap = 29,
    kFallbackReasonCount,
  };

 private:
  void FallBackToRasterCanvas(FallbackReason);
  void InitializeCurrentFrame();
  bool FinalizeFrameInternal(FallbackReason*);
  int ApproximateOpCount();

  const AllowFallback allow_fallback_;
  std::unique_ptr<PaintRecorder> current_frame_;
  sk_sp<PaintRecord> previous_frame_;
  std::unique_ptr<UnacceleratedImageBufferSurface> fallback_surface_;
  ImageBuffer* image_buffer_;
  int initial_save_count_;
  int current_frame_pixel_count_;
  int previous_frame_pixel_count_;
  bool frame_was_cleared_;
  bool did_record_draw_commands_in_current_frame_;
  bool current_frame_has_expensive_op_;
  bool previous_frame_has_expensive_op_;
};

}  // namespace blink

#endif
