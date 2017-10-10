// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/RecordingImageBufferSurface.h"

#include <memory>

#include "platform/Histogram.h"
#include "platform/graphics/CanvasHeuristicParameters.h"
#include "platform/graphics/CanvasMetrics.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

RecordingImageBufferSurface::RecordingImageBufferSurface(
    const IntSize& size,
    AllowFallback allow_fallback,
    OpacityMode opacity_mode,
    const CanvasColorParams& color_params)
    : ImageBufferSurface(size, opacity_mode, color_params),
      allow_fallback_(allow_fallback),
      image_buffer_(0),
      current_frame_pixel_count_(0),
      previous_frame_pixel_count_(0),
      frame_was_cleared_(true),
      did_record_draw_commands_in_current_frame_(false),
      current_frame_has_expensive_op_(false),
      previous_frame_has_expensive_op_(false) {
  InitializeCurrentFrame();
}

RecordingImageBufferSurface::~RecordingImageBufferSurface() {}

void RecordingImageBufferSurface::InitializeCurrentFrame() {
  current_frame_ = WTF::WrapUnique(new PaintRecorder);
  PaintCanvas* canvas =
      current_frame_->beginRecording(Size().Width(), Size().Height());
  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  canvas->save();

  if (image_buffer_) {
    image_buffer_->ResetCanvas(canvas);
  }
  did_record_draw_commands_in_current_frame_ = false;
  current_frame_has_expensive_op_ = false;
  current_frame_pixel_count_ = 0;
}

void RecordingImageBufferSurface::SetImageBuffer(ImageBuffer* image_buffer) {
  image_buffer_ = image_buffer;
  if (current_frame_ && image_buffer_) {
    image_buffer_->ResetCanvas(current_frame_->getRecordingCanvas());
  }
  if (fallback_surface_) {
    DCHECK(fallback_surface_->IsValid());
    fallback_surface_->SetImageBuffer(image_buffer);
  }
}

bool RecordingImageBufferSurface::WritePixels(const SkImageInfo& orig_info,
                                              const void* pixels,
                                              size_t row_bytes,
                                              int x,
                                              int y) {
  if (!fallback_surface_) {
    IntRect write_rect(x, y, orig_info.width(), orig_info.height());
    if (write_rect.Contains(IntRect(IntPoint(), Size())))
      WillOverwriteCanvas();
    FallBackToRasterCanvas(kFallbackReasonWritePixels);
    if (!fallback_surface_->IsValid())
      return false;
  }
  return fallback_surface_->WritePixels(orig_info, pixels, row_bytes, x, y);
}

void RecordingImageBufferSurface::FallBackToRasterCanvas(
    FallbackReason reason) {
  DCHECK(allow_fallback_ == kAllowFallback);
  CHECK(reason != kFallbackReasonUnknown);

  if (fallback_surface_) {
    DCHECK(!current_frame_);
    return;
  }

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, canvas_fallback_histogram,
      ("Canvas.DisplayListFallbackReason", kFallbackReasonCount));
  canvas_fallback_histogram.Count(reason);

  fallback_surface_ = WTF::WrapUnique(new UnacceleratedImageBufferSurface(
      Size(), GetOpacityMode(), kInitializeImagePixels, ColorParams()));
  // If the fallback surface fails to be created, then early out.
  if (!fallback_surface_->IsValid())
    return;

  fallback_surface_->SetImageBuffer(image_buffer_);

  if (previous_frame_) {
    fallback_surface_->Canvas()->drawPicture(previous_frame_);
    previous_frame_.reset();
  }

  if (current_frame_) {
    sk_sp<PaintRecord> record = current_frame_->finishRecordingAsPicture();
    if (record)
      fallback_surface_->Canvas()->drawPicture(record);
    current_frame_.reset();
  }

  if (image_buffer_) {
    image_buffer_->ResetCanvas(fallback_surface_->Canvas());
  }

  CanvasMetrics::CountCanvasContextUsage(
      CanvasMetrics::kDisplayList2DCanvasFallbackToRaster);
}

static RecordingImageBufferSurface::FallbackReason
SnapshotReasonToFallbackReason(SnapshotReason reason) {
  switch (reason) {
    case kSnapshotReasonUnknown:
      return RecordingImageBufferSurface::kFallbackReasonUnknown;
    case kSnapshotReasonGetImageData:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForGetImageData;
    case kSnapshotReasonPaint:
      return RecordingImageBufferSurface::kFallbackReasonSnapshotForPaint;
    case kSnapshotReasonToDataURL:
      return RecordingImageBufferSurface::kFallbackReasonSnapshotForToDataURL;
    case kSnapshotReasonToBlob:
      return RecordingImageBufferSurface::kFallbackReasonSnapshotForToBlob;
    case kSnapshotReasonCanvasListenerCapture:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForCanvasListenerCapture;
    case kSnapshotReasonDrawImage:
      return RecordingImageBufferSurface::kFallbackReasonSnapshotForDrawImage;
    case kSnapshotReasonCreatePattern:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForCreatePattern;
    case kSnapshotReasonTransferToImageBitmap:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForTransferToImageBitmap;
    case kSnapshotReasonUnitTests:
      return RecordingImageBufferSurface::kFallbackReasonSnapshotForUnitTests;
    case kSnapshotReasonGetCopiedImage:
      return RecordingImageBufferSurface::kFallbackReasonSnapshotGetCopiedImage;
    case kSnapshotReasonWebGLDrawImageIntoBuffer:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotWebGLDrawImageIntoBuffer;
    case kSnapshotReasonWebGLTexImage2D:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForWebGLTexImage2D;
    case kSnapshotReasonWebGLTexSubImage2D:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForWebGLTexSubImage2D;
    case kSnapshotReasonWebGLTexImage3D:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForWebGLTexImage3D;
    case kSnapshotReasonWebGLTexSubImage3D:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForWebGLTexSubImage3D;
    case kSnapshotReasonCopyToClipboard:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForCopyToClipboard;
    case kSnapshotReasonCreateImageBitmap:
      return RecordingImageBufferSurface::
          kFallbackReasonSnapshotForCreateImageBitmap;
  }
  NOTREACHED();
  return RecordingImageBufferSurface::kFallbackReasonUnknown;
}

RefPtr<StaticBitmapImage> RecordingImageBufferSurface::NewImageSnapshot(
    AccelerationHint hint,
    SnapshotReason reason) {
  if (!fallback_surface_)
    FallBackToRasterCanvas(SnapshotReasonToFallbackReason(reason));
  if (!fallback_surface_->IsValid())
    return nullptr;
  return fallback_surface_->NewImageSnapshot(hint, reason);
}

PaintCanvas* RecordingImageBufferSurface::Canvas() {
  if (fallback_surface_) {
    DCHECK(fallback_surface_->IsValid());
    return fallback_surface_->Canvas();
  }

  DCHECK(current_frame_->getRecordingCanvas());
  return current_frame_->getRecordingCanvas();
}

static RecordingImageBufferSurface::FallbackReason
DisableDeferralReasonToFallbackReason(DisableDeferralReason reason) {
  switch (reason) {
    case kDisableDeferralReasonUnknown:
      return RecordingImageBufferSurface::kFallbackReasonUnknown;
    case kDisableDeferralReasonExpensiveOverdrawHeuristic:
      return RecordingImageBufferSurface::
          kFallbackReasonExpensiveOverdrawHeuristic;
    case kDisableDeferralReasonUsingTextureBackedPattern:
      return RecordingImageBufferSurface::kFallbackReasonTextureBackedPattern;
    case kDisableDeferralReasonDrawImageOfVideo:
      return RecordingImageBufferSurface::kFallbackReasonDrawImageOfVideo;
    case kDisableDeferralReasonDrawImageOfAnimated2dCanvas:
      return RecordingImageBufferSurface::
          kFallbackReasonDrawImageOfAnimated2dCanvas;
    case kDisableDeferralReasonSubPixelTextAntiAliasingSupport:
      return RecordingImageBufferSurface::
          kFallbackReasonSubPixelTextAntiAliasingSupport;
    case kDisableDeferralDrawImageWithTextureBackedSourceImage:
      return RecordingImageBufferSurface::
          kFallbackReasonDrawImageWithTextureBackedSourceImage;
    // The LowEndDevice reason should only be used on Canvas2DLayerBridge.
    case kDisableDeferralReasonLowEndDevice:
    case kDisableDeferralReasonCount:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return RecordingImageBufferSurface::kFallbackReasonUnknown;
}

void RecordingImageBufferSurface::DisableDeferral(
    DisableDeferralReason reason) {
  if (!fallback_surface_)
    FallBackToRasterCanvas(DisableDeferralReasonToFallbackReason(reason));
}

sk_sp<PaintRecord> RecordingImageBufferSurface::GetRecord() {
  if (fallback_surface_)
    return nullptr;

  FallbackReason fallback_reason = kFallbackReasonUnknown;
  bool can_use_record = FinalizeFrameInternal(&fallback_reason);

  if (can_use_record) {
    return previous_frame_;
  }

  if (!fallback_surface_)
    FallBackToRasterCanvas(fallback_reason);
  return nullptr;
}

void RecordingImageBufferSurface::FinalizeFrame() {
  if (fallback_surface_) {
    fallback_surface_->FinalizeFrame();
    return;
  }

  FallbackReason fallback_reason = kFallbackReasonUnknown;
  if (!FinalizeFrameInternal(&fallback_reason))
    FallBackToRasterCanvas(fallback_reason);
}

void RecordingImageBufferSurface::DoPaintInvalidation(
    const FloatRect& dirty_rect) {
  if (fallback_surface_) {
    fallback_surface_->DoPaintInvalidation(dirty_rect);
  }
}

static RecordingImageBufferSurface::FallbackReason FlushReasonToFallbackReason(
    FlushReason reason) {
  switch (reason) {
    case kFlushReasonUnknown:
      return RecordingImageBufferSurface::kFallbackReasonUnknown;
    case kFlushReasonInitialClear:
      return RecordingImageBufferSurface::kFallbackReasonFlushInitialClear;
    case kFlushReasonDrawImageOfWebGL:
      return RecordingImageBufferSurface::
          kFallbackReasonFlushForDrawImageOfWebGL;
  }
  NOTREACHED();
  return RecordingImageBufferSurface::kFallbackReasonUnknown;
}

void RecordingImageBufferSurface::Flush(FlushReason reason) {
  if (!fallback_surface_)
    FallBackToRasterCanvas(FlushReasonToFallbackReason(reason));
  fallback_surface_->Flush(reason);
}

void RecordingImageBufferSurface::WillOverwriteCanvas() {
  frame_was_cleared_ = true;
  previous_frame_.reset();
  previous_frame_has_expensive_op_ = false;
  previous_frame_pixel_count_ = 0;
  if (did_record_draw_commands_in_current_frame_) {
    // Discard previous draw commands
    current_frame_->finishRecordingAsPicture();
    InitializeCurrentFrame();
  }
}

void RecordingImageBufferSurface::DidDraw(const FloatRect& rect) {
  did_record_draw_commands_in_current_frame_ = true;
  IntRect pixel_bounds = EnclosingIntRect(rect);
  CheckedNumeric<int> pixel_count = pixel_bounds.Width();
  pixel_count *= pixel_bounds.Height();
  pixel_count += current_frame_pixel_count_;
  current_frame_pixel_count_ =
      pixel_count.ValueOrDefault(std::numeric_limits<int>::max());
}

bool RecordingImageBufferSurface::FinalizeFrameInternal(
    FallbackReason* fallback_reason) {
  CHECK(!fallback_surface_);
  CHECK(current_frame_);
  DCHECK(current_frame_->getRecordingCanvas());
  DCHECK(fallback_reason);
  DCHECK(*fallback_reason == kFallbackReasonUnknown);
  if (!did_record_draw_commands_in_current_frame_) {
    if (!previous_frame_) {
      // Create an initial blank frame
      previous_frame_ = current_frame_->finishRecordingAsPicture();
      InitializeCurrentFrame();
    }
    CHECK(current_frame_);
    return true;
  }

  if (!frame_was_cleared_) {
    *fallback_reason = kFallbackReasonCanvasNotClearedBetweenFrames;
    return false;
  }

  if (allow_fallback_ == kAllowFallback &&
      current_frame_->getRecordingCanvas()->getSaveCount() - 1 >
          CanvasHeuristicParameters::kExpensiveRecordingStackDepth) {
    // (getSaveCount() decremented to account  for the intial recording canvas
    // save frame.)
    *fallback_reason = kFallbackReasonRunawayStateStack;
    return false;
  }

  previous_frame_ = current_frame_->finishRecordingAsPicture();
  previous_frame_has_expensive_op_ = current_frame_has_expensive_op_;
  previous_frame_pixel_count_ = current_frame_pixel_count_;
  InitializeCurrentFrame();

  frame_was_cleared_ = false;
  return true;
}

void RecordingImageBufferSurface::Draw(GraphicsContext& context,
                                       const FloatRect& dest_rect,
                                       const FloatRect& src_rect,
                                       SkBlendMode op) {
  if (fallback_surface_) {
    fallback_surface_->Draw(context, dest_rect, src_rect, op);
    return;
  }

  sk_sp<PaintRecord> record = GetRecord();
  if (record) {
    context.CompositeRecord(std::move(record), dest_rect, src_rect, op);
  } else {
    ImageBufferSurface::Draw(context, dest_rect, src_rect, op);
  }
}

bool RecordingImageBufferSurface::IsExpensiveToPaint() {
  if (fallback_surface_)
    return fallback_surface_->IsExpensiveToPaint();

  CheckedNumeric<int> overdraw_limit_checked = Size().Width();
  overdraw_limit_checked *= Size().Height();
  overdraw_limit_checked *=
      CanvasHeuristicParameters::kExpensiveOverdrawThreshold;
  int overdraw_limit =
      overdraw_limit_checked.ValueOrDefault(std::numeric_limits<int>::max());

  if (did_record_draw_commands_in_current_frame_) {
    if (current_frame_has_expensive_op_)
      return true;

    if (current_frame_pixel_count_ >= overdraw_limit)
      return true;

    if (frame_was_cleared_)
      return false;  // early exit because previous frame is overdrawn
  }

  if (previous_frame_) {
    if (previous_frame_has_expensive_op_)
      return true;

    if (previous_frame_pixel_count_ >= overdraw_limit)
      return true;
  }

  return false;
}

// Fallback passthroughs

bool RecordingImageBufferSurface::Restore() {
  if (fallback_surface_)
    return fallback_surface_->Restore();
  return ImageBufferSurface::Restore();
}

WebLayer* RecordingImageBufferSurface::Layer() const {
  if (fallback_surface_)
    return fallback_surface_->Layer();
  return ImageBufferSurface::Layer();
}

bool RecordingImageBufferSurface::IsAccelerated() const {
  if (fallback_surface_)
    return fallback_surface_->IsAccelerated();
  return ImageBufferSurface::IsAccelerated();
}

void RecordingImageBufferSurface::SetIsHidden(bool hidden) {
  if (fallback_surface_)
    fallback_surface_->SetIsHidden(hidden);
  else
    ImageBufferSurface::SetIsHidden(hidden);
}

}  // namespace blink
