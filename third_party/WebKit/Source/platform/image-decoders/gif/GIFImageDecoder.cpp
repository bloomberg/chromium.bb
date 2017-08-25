/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/image-decoders/gif/GIFImageDecoder.h"

#include <limits>
#include "platform/image-decoders/SegmentStream.h"
#include "platform/wtf/NotFound.h"
#include "platform/wtf/PtrUtil.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {

GIFImageDecoder::GIFImageDecoder(AlphaOption alpha_option,
                                 const ColorBehavior& color_behavior,
                                 size_t max_decoded_bytes)
    : ImageDecoder(alpha_option, color_behavior, max_decoded_bytes),
      codec_(),
      segment_stream_(nullptr) {}

GIFImageDecoder::~GIFImageDecoder() = default;

void GIFImageDecoder::OnSetData(SegmentReader* data) {
  if (!data) {
    if (segment_stream_)
      segment_stream_->SetReader(nullptr);
    return;
  }

  std::unique_ptr<SegmentStream> segment_stream;
  if (!segment_stream_) {
    segment_stream = base::MakeUnique<SegmentStream>();
    segment_stream_ = segment_stream.get();
  }

  segment_stream_->SetReader(std::move(data));

  if (!codec_) {
    SkCodec::Result codec_creation_result;
    codec_ = SkCodec::MakeFromStream(std::move(segment_stream),
                                     &codec_creation_result, nullptr);
    switch (codec_creation_result) {
      case SkCodec::kSuccess: {
        // SkCodec::MakeFromStream will read enough of the image to get the
        // image size.
        SkImageInfo image_info = codec_->getInfo();
        SetSize(image_info.width(), image_info.height());
        return;
      }
      case SkCodec::kIncompleteInput:
        // |segment_stream_|'s ownership is passed into MakeFromStream.
        // It is deleted if MakeFromStream fails.
        // If MakeFromStream fails, we set |segment_stream_| to null so
        // we aren't pointing to reclaimed memory.
        segment_stream_ = nullptr;
        return;
      default:
        SetFailed();
        return;
    }
  }
}

int GIFImageDecoder::RepetitionCount() const {
  if (!codec_ || segment_stream_->IsCleared())
    return repetition_count_;

  DCHECK(!Failed());

  // This value can arrive at any point in the image data stream.  Most GIFs
  // in the wild declare it near the beginning of the file, so it usually is
  // set by the time we've decoded the size, but (depending on the GIF and the
  // packets sent back by the webserver) not always.
  //
  // SkCodec will parse forward in the file if the repetition count has not
  // been seen yet.
  int repetition_count = codec_->getRepetitionCount();

  switch (repetition_count) {
    case 0: {
      // SkCodec returns 0 for both still images and animated images which
      // only play once.
      if (IsAllDataReceived() && codec_->getFrameCount() == 1) {
        repetition_count_ = kAnimationNone;
        break;
      }

      repetition_count_ = kAnimationLoopOnce;
      break;
    }
    case SkCodec::kRepetitionCountInfinite:
      repetition_count_ = kAnimationLoopInfinite;
      break;
    default:
      repetition_count_ = repetition_count;
      break;
  }

  return repetition_count_;
}

bool GIFImageDecoder::FrameIsReceivedAtIndex(size_t index) const {
  SkCodec::FrameInfo frame_info;
  if (!codec_ || !codec_->getFrameInfo(index, &frame_info))
    return false;
  return frame_info.fFullyReceived;
}

float GIFImageDecoder::FrameDurationAtIndex(size_t index) const {
  if (index < frame_buffer_cache_.size())
    return frame_buffer_cache_[index].Duration();
  return 0;
}

bool GIFImageDecoder::SetFailed() {
  segment_stream_ = nullptr;
  codec_.reset();
  return ImageDecoder::SetFailed();
}

size_t GIFImageDecoder::ClearCacheExceptFrame(size_t index) {
  // SkCodec attempts to report the earliest possible required frame, but it is
  // possible that frame has been evicted, while a later frame (which could also
  // be used as the required frame) is still cached. Try to preserve a frame
  // that is still cached.
  if (frame_buffer_cache_.size() <= 1)
    return 0;

  size_t index2 = kNotFound;
  if (index < frame_buffer_cache_.size()) {
    const ImageFrame& frame = frame_buffer_cache_[index];
    if (frame.RequiredPreviousFrameIndex() != kNotFound &&
        (!FrameStatusSufficientForSuccessors(index) ||
         frame.GetDisposalMethod() == ImageFrame::kDisposeOverwritePrevious)) {
      index2 = GetViableReferenceFrameIndex(index);
    }
  }

  return ClearCacheExceptTwoFrames(index, index2);
}

size_t GIFImageDecoder::DecodeFrameCount() {
  if (!codec_ || segment_stream_->IsCleared())
    return frame_buffer_cache_.size();

  return codec_->getFrameCount();
}

void GIFImageDecoder::InitializeNewFrame(size_t index) {
  DCHECK(codec_);

  ImageFrame& frame = frame_buffer_cache_[index];
  // SkCodec does not inform us if only a portion of the image was updated
  // in the current frame. Because of this, rather than correctly filling in
  // the frame rect, we set the frame rect to be the image's full size.
  // The original frame rect is not used, anyway.
  IntSize full_image_size = Size();
  frame.SetOriginalFrameRect(IntRect(IntPoint(), full_image_size));

  SkCodec::FrameInfo frame_info;
  bool frame_info_received = codec_->getFrameInfo(index, &frame_info);
  DCHECK(frame_info_received);
  frame.SetDuration(frame_info.fDuration);
  size_t required_previous_frame_index;
  if (frame_info.fRequiredFrame == SkCodec::kNone) {
    required_previous_frame_index = kNotFound;
  } else {
    required_previous_frame_index =
        static_cast<size_t>(frame_info.fRequiredFrame);
  }
  frame.SetRequiredPreviousFrameIndex(required_previous_frame_index);

  ImageFrame::DisposalMethod disposal_method = ImageFrame::kDisposeNotSpecified;
  switch (frame_info.fDisposalMethod) {
    case SkCodecAnimation::DisposalMethod::kKeep:
      disposal_method = ImageFrame::kDisposeKeep;
      break;
    case SkCodecAnimation::DisposalMethod::kRestoreBGColor:
      disposal_method = ImageFrame::kDisposeOverwriteBgcolor;
      break;
    case SkCodecAnimation::DisposalMethod::kRestorePrevious:
      disposal_method = ImageFrame::kDisposeOverwritePrevious;
      break;
  }
  frame.SetDisposalMethod(disposal_method);
}

void GIFImageDecoder::Decode(size_t index) {
  if (!codec_ || segment_stream_->IsCleared())
    return;

  DCHECK(!Failed());

  DCHECK_LT(index, frame_buffer_cache_.size());

  UpdateAggressivePurging(index);
  SkImageInfo image_info = codec_->getInfo()
                               .makeColorType(kN32_SkColorType)
                               .makeColorSpace(ColorSpaceForSkImages());

  SkCodec::Options options;
  options.fFrameIndex = index;
  options.fPriorFrame = SkCodec::kNone;
  options.fZeroInitialized = SkCodec::kNo_ZeroInitialized;

  ImageFrame& frame = frame_buffer_cache_[index];
  if (frame.GetStatus() == ImageFrame::kFrameEmpty) {
    size_t required_previous_frame_index = frame.RequiredPreviousFrameIndex();
    if (required_previous_frame_index == kNotFound) {
      frame.AllocatePixelData(Size().Width(), Size().Height(),
                              ColorSpaceForSkImages());
      frame.ZeroFillPixelData();
    } else {
      size_t previous_frame_index = GetViableReferenceFrameIndex(index);
      if (previous_frame_index == kNotFound) {
        previous_frame_index = required_previous_frame_index;
        Decode(previous_frame_index);
        if (Failed()) {
          return;
        }
      }

      // We try to reuse |previous_frame| as starting state to avoid copying.
      // If CanReusePreviousFrameBuffer returns false, we must copy the data
      // since |previous_frame| is necessary to decode this or later frames.
      // In that case copy the data instead.
      ImageFrame& previous_frame = frame_buffer_cache_[previous_frame_index];
      if ((!CanReusePreviousFrameBuffer(index) ||
           !frame.TakeBitmapDataIfWritable(&previous_frame)) &&
          !frame.CopyBitmapData(previous_frame)) {
        SetFailed();
        return;
      }
      options.fPriorFrame = previous_frame_index;
    }
  }

  if (frame.GetStatus() == ImageFrame::kFrameAllocated) {
    SkCodec::Result start_incremental_decode_result =
        codec_->startIncrementalDecode(image_info, frame.Bitmap().getPixels(),
                                       frame.Bitmap().rowBytes(), &options);
    switch (start_incremental_decode_result) {
      case SkCodec::kSuccess:
        break;
      case SkCodec::kIncompleteInput:
        return;
      default:
        SetFailed();
        return;
    }
    frame.SetStatus(ImageFrame::kFramePartial);
  }

  SkCodec::Result incremental_decode_result = codec_->incrementalDecode();
  switch (incremental_decode_result) {
    case SkCodec::kSuccess: {
      SkCodec::FrameInfo frame_info;
      bool frame_info_received = codec_->getFrameInfo(index, &frame_info);
      DCHECK(frame_info_received);
      frame.SetHasAlpha(frame_info.fAlpha != SkEncodedInfo::kOpaque_Alpha);
      frame.SetPixelsChanged(true);
      frame.SetStatus(ImageFrame::kFrameComplete);
      PostDecodeProcessing(index);
      break;
    }
    case SkCodec::kIncompleteInput:
      frame.SetPixelsChanged(true);
      if (FrameIsReceivedAtIndex(index) || IsAllDataReceived()) {
        SetFailed();
      }
      break;
    default:
      SetFailed();
      break;
  }
}

bool GIFImageDecoder::CanReusePreviousFrameBuffer(size_t index) const {
  DCHECK_LT(index, frame_buffer_cache_.size());
  return frame_buffer_cache_[index].GetDisposalMethod() !=
         ImageFrame::kDisposeOverwritePrevious;
}

size_t GIFImageDecoder::GetViableReferenceFrameIndex(
    size_t dependent_index) const {
  DCHECK_LT(dependent_index, frame_buffer_cache_.size());

  size_t required_previous_frame_index =
      frame_buffer_cache_[dependent_index].RequiredPreviousFrameIndex();

  // Any frame in the range [|required_previous_frame_index|, |dependent_index|)
  // which has a disposal method other than kRestorePrevious can be provided as
  // the prior frame to SkCodec.
  //
  // SkCodec sets SkCodec::FrameInfo::fRequiredFrame to the earliest frame which
  // can be used. This might come up when several frames update the same
  // subregion. If that same subregion is about to be overwritten, it doesn't
  // matter which frame in that chain is provided.
  DCHECK_NE(required_previous_frame_index, kNotFound);
  // Loop backwards because the frames most likely to be in cache are the most
  // recent.
  for (size_t i = dependent_index - 1; i != required_previous_frame_index;
       i--) {
    const ImageFrame& frame = frame_buffer_cache_[i];

    if (frame.GetDisposalMethod() == ImageFrame::kDisposeOverwritePrevious)
      continue;

    if (frame.GetStatus() == ImageFrame::kFrameComplete) {
      return i;
    }
  }

  return kNotFound;
}

}  // namespace blink
