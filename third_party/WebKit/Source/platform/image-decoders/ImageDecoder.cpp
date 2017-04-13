/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "platform/image-decoders/ImageDecoder.h"

#include <memory>
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/BitmapImageMetrics.h"
#include "platform/image-decoders/FastSharedBufferReader.h"
#include "platform/image-decoders/bmp/BMPImageDecoder.h"
#include "platform/image-decoders/gif/GIFImageDecoder.h"
#include "platform/image-decoders/ico/ICOImageDecoder.h"
#include "platform/image-decoders/jpeg/JPEGImageDecoder.h"
#include "platform/image-decoders/png/PNGImageDecoder.h"
#include "platform/image-decoders/webp/WEBPImageDecoder.h"
#include "platform/instrumentation/PlatformInstrumentation.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

inline bool MatchesJPEGSignature(const char* contents) {
  return !memcmp(contents, "\xFF\xD8\xFF", 3);
}

inline bool MatchesPNGSignature(const char* contents) {
  return !memcmp(contents, "\x89PNG\r\n\x1A\n", 8);
}

inline bool MatchesGIFSignature(const char* contents) {
  return !memcmp(contents, "GIF87a", 6) || !memcmp(contents, "GIF89a", 6);
}

inline bool MatchesWebPSignature(const char* contents) {
  return !memcmp(contents, "RIFF", 4) && !memcmp(contents + 8, "WEBPVP", 6);
}

inline bool MatchesICOSignature(const char* contents) {
  return !memcmp(contents, "\x00\x00\x01\x00", 4);
}

inline bool MatchesCURSignature(const char* contents) {
  return !memcmp(contents, "\x00\x00\x02\x00", 4);
}

inline bool MatchesBMPSignature(const char* contents) {
  return !memcmp(contents, "BM", 2);
}

// This needs to be updated if we ever add a matches*Signature() which requires
// more characters.
static constexpr size_t kLongestSignatureLength = sizeof("RIFF????WEBPVP") - 1;

std::unique_ptr<ImageDecoder> ImageDecoder::Create(
    RefPtr<SegmentReader> data,
    bool data_complete,
    AlphaOption alpha_option,
    const ColorBehavior& color_behavior) {
  // We need at least kLongestSignatureLength bytes to run the signature
  // matcher.
  if (data->size() < kLongestSignatureLength)
    return nullptr;

  const size_t max_decoded_bytes =
      Platform::Current() ? Platform::Current()->MaxDecodedImageBytes()
                          : kNoDecodedImageByteLimit;

  // Access the first kLongestSignatureLength chars to sniff the signature.
  // (note: FastSharedBufferReader only makes a copy if the bytes are segmented)
  char buffer[kLongestSignatureLength];
  const FastSharedBufferReader fast_reader(data);
  const ImageDecoder::SniffResult sniff_result = DetermineImageType(
      fast_reader.GetConsecutiveData(0, kLongestSignatureLength, buffer),
      kLongestSignatureLength);

  std::unique_ptr<ImageDecoder> decoder;
  switch (sniff_result) {
    case SniffResult::JPEG:
      decoder.reset(new JPEGImageDecoder(alpha_option, color_behavior,
                                         max_decoded_bytes));
      break;
    case SniffResult::PNG:
      decoder.reset(
          new PNGImageDecoder(alpha_option, color_behavior, max_decoded_bytes));
      break;
    case SniffResult::GIF:
      decoder.reset(
          new GIFImageDecoder(alpha_option, color_behavior, max_decoded_bytes));
      break;
    case SniffResult::WEBP:
      decoder.reset(new WEBPImageDecoder(alpha_option, color_behavior,
                                         max_decoded_bytes));
      break;
    case SniffResult::ICO:
      decoder.reset(
          new ICOImageDecoder(alpha_option, color_behavior, max_decoded_bytes));
      break;
    case SniffResult::BMP:
      decoder.reset(
          new BMPImageDecoder(alpha_option, color_behavior, max_decoded_bytes));
      break;
    case SniffResult::kInvalid:
      break;
  }

  if (decoder)
    decoder->SetData(std::move(data), data_complete);

  return decoder;
}

bool ImageDecoder::HasSufficientDataToSniffImageType(const SharedBuffer& data) {
  return data.size() >= kLongestSignatureLength;
}

ImageDecoder::SniffResult ImageDecoder::DetermineImageType(const char* contents,
                                                           size_t length) {
  DCHECK_GE(length, kLongestSignatureLength);

  if (MatchesJPEGSignature(contents))
    return SniffResult::JPEG;
  if (MatchesPNGSignature(contents))
    return SniffResult::PNG;
  if (MatchesGIFSignature(contents))
    return SniffResult::GIF;
  if (MatchesWebPSignature(contents))
    return SniffResult::WEBP;
  if (MatchesICOSignature(contents) || MatchesCURSignature(contents))
    return SniffResult::ICO;
  if (MatchesBMPSignature(contents))
    return SniffResult::BMP;
  return SniffResult::kInvalid;
}

size_t ImageDecoder::FrameCount() {
  const size_t old_size = frame_buffer_cache_.size();
  const size_t new_size = DecodeFrameCount();
  if (old_size != new_size) {
    frame_buffer_cache_.Resize(new_size);
    for (size_t i = old_size; i < new_size; ++i) {
      frame_buffer_cache_[i].SetPremultiplyAlpha(premultiply_alpha_);
      InitializeNewFrame(i);
    }
  }
  return new_size;
}

ImageFrame* ImageDecoder::FrameBufferAtIndex(size_t index) {
  if (index >= FrameCount())
    return 0;

  ImageFrame* frame = &frame_buffer_cache_[index];
  if (frame->GetStatus() != ImageFrame::kFrameComplete) {
    PlatformInstrumentation::WillDecodeImage(FilenameExtension());
    Decode(index);
    PlatformInstrumentation::DidDecodeImage();
  }

  if (!has_histogrammed_color_space_) {
    BitmapImageMetrics::CountImageGammaAndGamut(embedded_color_space_.get());
    has_histogrammed_color_space_ = true;
  }

  frame->NotifyBitmapIfPixelsChanged();
  return frame;
}

bool ImageDecoder::FrameHasAlphaAtIndex(size_t index) const {
  return !FrameIsCompleteAtIndex(index) ||
         frame_buffer_cache_[index].HasAlpha();
}

bool ImageDecoder::FrameIsCompleteAtIndex(size_t index) const {
  return (index < frame_buffer_cache_.size()) &&
         (frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameComplete);
}

size_t ImageDecoder::FrameBytesAtIndex(size_t index) const {
  if (index >= frame_buffer_cache_.size() ||
      frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameEmpty)
    return 0;

  struct ImageSize {
    explicit ImageSize(IntSize size) {
      area = static_cast<uint64_t>(size.Width()) * size.Height();
    }

    uint64_t area;
  };

  return ImageSize(FrameSizeAtIndex(index)).area *
         sizeof(ImageFrame::PixelData);
}

size_t ImageDecoder::ClearCacheExceptFrame(size_t clear_except_frame) {
  // Don't clear if there are no frames or only one frame.
  if (frame_buffer_cache_.size() <= 1)
    return 0;

  // We expect that after this call, we'll be asked to decode frames after this
  // one. So we want to avoid clearing frames such that those requests would
  // force re-decoding from the beginning of the image. There are two cases in
  // which preserving |clearCacheExcept| frame is not enough to avoid that:
  //
  // 1. |clearExceptFrame| is not yet sufficiently decoded to decode subsequent
  //    frames. We need the previous frame to sufficiently decode this frame.
  // 2. The disposal method of |clearExceptFrame| is DisposeOverwritePrevious.
  //    In that case, we need to keep the required previous frame in the cache
  //    to prevent re-decoding that frame when |clearExceptFrame| is disposed.
  //
  // If either 1 or 2 is true, store the required previous frame in
  // |clearExceptFrame2| so it won't be cleared.
  size_t clear_except_frame2 = kNotFound;
  if (clear_except_frame < frame_buffer_cache_.size()) {
    const ImageFrame& frame = frame_buffer_cache_[clear_except_frame];
    if (!FrameStatusSufficientForSuccessors(clear_except_frame) ||
        frame.GetDisposalMethod() == ImageFrame::kDisposeOverwritePrevious)
      clear_except_frame2 = frame.RequiredPreviousFrameIndex();
  }

  // Now |clearExceptFrame2| indicates the frame that |clearExceptFrame|
  // depends on, as described above. But if decoding is skipping forward past
  // intermediate frames, this frame may be insufficiently decoded. So we need
  // to keep traversing back through the required previous frames until we find
  // the nearest ancestor that is sufficiently decoded. Preserving that will
  // minimize the amount of future decoding needed.
  while (clear_except_frame2 < frame_buffer_cache_.size() &&
         !FrameStatusSufficientForSuccessors(clear_except_frame2)) {
    clear_except_frame2 =
        frame_buffer_cache_[clear_except_frame2].RequiredPreviousFrameIndex();
  }

  return ClearCacheExceptTwoFrames(clear_except_frame, clear_except_frame2);
}

size_t ImageDecoder::ClearCacheExceptTwoFrames(size_t clear_except_frame1,
                                               size_t clear_except_frame2) {
  size_t frame_bytes_cleared = 0;
  for (size_t i = 0; i < frame_buffer_cache_.size(); ++i) {
    if (frame_buffer_cache_[i].GetStatus() != ImageFrame::kFrameEmpty &&
        i != clear_except_frame1 && i != clear_except_frame2) {
      frame_bytes_cleared += FrameBytesAtIndex(i);
      ClearFrameBuffer(i);
    }
  }
  return frame_bytes_cleared;
}

void ImageDecoder::ClearFrameBuffer(size_t frame_index) {
  frame_buffer_cache_[frame_index].ClearPixelData();
}

Vector<size_t> ImageDecoder::FindFramesToDecode(size_t index) const {
  DCHECK(index < frame_buffer_cache_.size());

  Vector<size_t> frames_to_decode;
  do {
    frames_to_decode.push_back(index);
    index = frame_buffer_cache_[index].RequiredPreviousFrameIndex();
  } while (index != kNotFound && frame_buffer_cache_[index].GetStatus() !=
                                     ImageFrame::kFrameComplete);
  return frames_to_decode;
}

bool ImageDecoder::PostDecodeProcessing(size_t index) {
  DCHECK(index < frame_buffer_cache_.size());

  if (frame_buffer_cache_[index].GetStatus() != ImageFrame::kFrameComplete)
    return false;

  if (purge_aggressively_)
    ClearCacheExceptFrame(index);

  return true;
}

void ImageDecoder::CorrectAlphaWhenFrameBufferSawNoAlpha(size_t index) {
  DCHECK(index < frame_buffer_cache_.size());
  ImageFrame& buffer = frame_buffer_cache_[index];

  // When this frame spans the entire image rect we can set hasAlpha to false,
  // since there are logically no transparent pixels outside of the frame rect.
  if (buffer.OriginalFrameRect().Contains(IntRect(IntPoint(), Size()))) {
    buffer.SetHasAlpha(false);
    buffer.SetRequiredPreviousFrameIndex(kNotFound);
  } else if (buffer.RequiredPreviousFrameIndex() != kNotFound) {
    // When the frame rect does not span the entire image rect, and it does
    // *not* have a required previous frame, the pixels outside of the frame
    // rect will be fully transparent, so we shoudn't set hasAlpha to false.
    //
    // It is a tricky case when the frame does have a required previous frame.
    // The frame does not have alpha only if everywhere outside its rect
    // doesn't have alpha.  To know whether this is true, we check the start
    // state of the frame -- if it doesn't have alpha, we're safe.
    //
    // We first check that the required previous frame does not have
    // DisposeOverWritePrevious as its disposal method - this should never
    // happen, since the required frame should in that case be the required
    // frame of this frame's required frame.
    //
    // If |prevBuffer| is DisposeNotSpecified or DisposeKeep, |buffer| has no
    // alpha if |prevBuffer| had no alpha. Since initFrameBuffer() already
    // copied the alpha state, there's nothing to do here.
    //
    // The only remaining case is a DisposeOverwriteBgcolor frame.  If
    // it had no alpha, and its rect is contained in the current frame's
    // rect, we know the current frame has no alpha.
    //
    // For DisposeNotSpecified, DisposeKeep and DisposeOverwriteBgcolor there
    // is one situation that is not taken into account - when |prevBuffer|
    // *does* have alpha, but only in the frame rect of |buffer|, we can still
    // say that this frame has no alpha. However, to determine this, we
    // potentially need to analyze all image pixels of |prevBuffer|, which is
    // too computationally expensive.
    const ImageFrame* prev_buffer =
        &frame_buffer_cache_[buffer.RequiredPreviousFrameIndex()];
    DCHECK(prev_buffer->GetDisposalMethod() !=
           ImageFrame::kDisposeOverwritePrevious);

    if ((prev_buffer->GetDisposalMethod() ==
         ImageFrame::kDisposeOverwriteBgcolor) &&
        !prev_buffer->HasAlpha() &&
        buffer.OriginalFrameRect().Contains(prev_buffer->OriginalFrameRect()))
      buffer.SetHasAlpha(false);
  }
}

bool ImageDecoder::InitFrameBuffer(size_t frame_index) {
  DCHECK(frame_index < frame_buffer_cache_.size());

  ImageFrame* const buffer = &frame_buffer_cache_[frame_index];

  // If the frame is already initialized, return true.
  if (buffer->GetStatus() != ImageFrame::kFrameEmpty)
    return true;

  size_t required_previous_frame_index = buffer->RequiredPreviousFrameIndex();
  if (required_previous_frame_index == kNotFound) {
    // This frame doesn't rely on any previous data.
    if (!buffer->AllocatePixelData(Size().Width(), Size().Height(),
                                   ColorSpaceForSkImages())) {
      return false;
    }
    buffer->ZeroFillPixelData();
  } else {
    ImageFrame* const prev_buffer =
        &frame_buffer_cache_[required_previous_frame_index];
    DCHECK(prev_buffer->GetStatus() == ImageFrame::kFrameComplete);

    // We try to reuse |prevBuffer| as starting state to avoid copying.
    // If canReusePreviousFrameBuffer returns false, we must copy the data since
    // |prevBuffer| is necessary to decode this or later frames. In that case,
    // copy the data instead.
    if ((!CanReusePreviousFrameBuffer(frame_index) ||
         !buffer->TakeBitmapDataIfWritable(prev_buffer)) &&
        !buffer->CopyBitmapData(*prev_buffer))
      return false;

    if (prev_buffer->GetDisposalMethod() ==
        ImageFrame::kDisposeOverwriteBgcolor) {
      // We want to clear the previous frame to transparent, without
      // affecting pixels in the image outside of the frame.
      const IntRect& prev_rect = prev_buffer->OriginalFrameRect();
      DCHECK(!prev_rect.Contains(IntRect(IntPoint(), Size())));
      buffer->ZeroFillFrameRect(prev_rect);
    }
  }

  OnInitFrameBuffer(frame_index);

  // Update our status to be partially complete.
  buffer->SetStatus(ImageFrame::kFramePartial);

  return true;
}

void ImageDecoder::UpdateAggressivePurging(size_t index) {
  if (purge_aggressively_)
    return;

  // We don't want to cache so much that we cause a memory issue.
  //
  // If we used a LRU cache we would fill it and then on next animation loop
  // we would need to decode all the frames again -- the LRU would give no
  // benefit and would consume more memory.
  // So instead, simply purge unused frames if caching all of the frames of
  // the image would use more memory than the image decoder is allowed
  // (m_maxDecodedBytes) or would overflow 32 bits..
  //
  // As we decode we will learn the total number of frames, and thus total
  // possible image memory used.

  const uint64_t frame_area = DecodedSize().Area();
  const uint64_t frame_memory_usage = frame_area * 4;  // 4 bytes per pixel
  if (frame_memory_usage / 4 != frame_area) {          // overflow occurred
    purge_aggressively_ = true;
    return;
  }

  const uint64_t total_memory_usage = frame_memory_usage * index;
  if (total_memory_usage / frame_memory_usage != index) {  // overflow occurred
    purge_aggressively_ = true;
    return;
  }

  if (total_memory_usage > max_decoded_bytes_) {
    purge_aggressively_ = true;
  }
}

size_t ImageDecoder::FindRequiredPreviousFrame(size_t frame_index,
                                               bool frame_rect_is_opaque) {
  DCHECK_LT(frame_index, frame_buffer_cache_.size());
  if (!frame_index) {
    // The first frame doesn't rely on any previous data.
    return kNotFound;
  }

  const ImageFrame* curr_buffer = &frame_buffer_cache_[frame_index];
  if ((frame_rect_is_opaque ||
       curr_buffer->GetAlphaBlendSource() == ImageFrame::kBlendAtopBgcolor) &&
      curr_buffer->OriginalFrameRect().Contains(IntRect(IntPoint(), Size())))
    return kNotFound;

  // The starting state for this frame depends on the previous frame's
  // disposal method.
  size_t prev_frame = frame_index - 1;
  const ImageFrame* prev_buffer = &frame_buffer_cache_[prev_frame];

  switch (prev_buffer->GetDisposalMethod()) {
    case ImageFrame::kDisposeNotSpecified:
    case ImageFrame::kDisposeKeep:
      // prevFrame will be used as the starting state for this frame.
      // FIXME: Be even smarter by checking the frame sizes and/or
      // alpha-containing regions.
      return prev_frame;
    case ImageFrame::kDisposeOverwritePrevious:
      // Frames that use the DisposeOverwritePrevious method are effectively
      // no-ops in terms of changing the starting state of a frame compared to
      // the starting state of the previous frame, so skip over them and
      // return the required previous frame of it.
      return prev_buffer->RequiredPreviousFrameIndex();
    case ImageFrame::kDisposeOverwriteBgcolor:
      // If the previous frame fills the whole image, then the current frame
      // can be decoded alone. Likewise, if the previous frame could be
      // decoded without reference to any prior frame, the starting state for
      // this frame is a blank frame, so it can again be decoded alone.
      // Otherwise, the previous frame contributes to this frame.
      return (prev_buffer->OriginalFrameRect().Contains(
                  IntRect(IntPoint(), Size())) ||
              (prev_buffer->RequiredPreviousFrameIndex() == kNotFound))
                 ? kNotFound
                 : prev_frame;
    default:
      NOTREACHED();
      return kNotFound;
  }
}

ImagePlanes::ImagePlanes() {
  for (int i = 0; i < 3; ++i) {
    planes_[i] = 0;
    row_bytes_[i] = 0;
  }
}

ImagePlanes::ImagePlanes(void* planes[3], const size_t row_bytes[3]) {
  for (int i = 0; i < 3; ++i) {
    planes_[i] = planes[i];
    row_bytes_[i] = row_bytes[i];
  }
}

void* ImagePlanes::Plane(int i) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, 3);
  return planes_[i];
}

size_t ImagePlanes::RowBytes(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, 3);
  return row_bytes_[i];
}

void ImageDecoder::SetEmbeddedColorProfile(const char* icc_data,
                                           unsigned icc_length) {
  sk_sp<SkColorSpace> color_space = SkColorSpace::MakeICC(icc_data, icc_length);
  if (!color_space)
    DLOG(ERROR) << "Failed to parse image ICC profile";
  SetEmbeddedColorSpace(std::move(color_space));
}

void ImageDecoder::SetEmbeddedColorSpace(sk_sp<SkColorSpace> color_space) {
  DCHECK(!IgnoresColorSpace());
  DCHECK(!has_histogrammed_color_space_);

  embedded_color_space_ = color_space;
  source_to_target_color_transform_needs_update_ = true;
}

SkColorSpaceXform* ImageDecoder::ColorTransform() {
  if (!source_to_target_color_transform_needs_update_)
    return source_to_target_color_transform_.get();
  source_to_target_color_transform_needs_update_ = false;
  source_to_target_color_transform_ = nullptr;

  if (!color_behavior_.IsTransformToTargetColorSpace())
    return nullptr;

  sk_sp<SkColorSpace> src_color_space = embedded_color_space_;
  if (!src_color_space) {
    if (RuntimeEnabledFeatures::colorCorrectRenderingEnabled())
      src_color_space = SkColorSpace::MakeSRGB();
    else
      return nullptr;
  }

  sk_sp<SkColorSpace> dst_color_space =
      color_behavior_.TargetColorSpace().ToSkColorSpace();

  if (SkColorSpace::Equals(src_color_space.get(), dst_color_space.get())) {
    return nullptr;
  }

  source_to_target_color_transform_ =
      SkColorSpaceXform::New(src_color_space.get(), dst_color_space.get());
  return source_to_target_color_transform_.get();
}

sk_sp<SkColorSpace> ImageDecoder::ColorSpaceForSkImages() const {
  if (!color_behavior_.IsTag())
    return nullptr;

  if (embedded_color_space_)
    return embedded_color_space_;
  return SkColorSpace::MakeSRGB();
}

}  // namespace blink
