/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "platform/graphics/DeferredImageDecoder.h"

#include <memory>
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/graphics/DecodingImageGenerator.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/ImageFrameGenerator.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-decoders/SegmentReader.h"
#include "platform/wtf/PtrUtil.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

struct DeferredFrameData {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(DeferredFrameData);

 public:
  DeferredFrameData()
      : orientation_(kDefaultImageOrientation),
        duration_(0),
        is_received_(false),
        frame_bytes_(0),
        unique_id_(DecodingImageGenerator::kNeedNewImageUniqueID) {}

  ImageOrientation orientation_;
  float duration_;
  bool is_received_;
  size_t frame_bytes_;
  uint32_t unique_id_;
};

std::unique_ptr<DeferredImageDecoder> DeferredImageDecoder::Create(
    RefPtr<SharedBuffer> data,
    bool data_complete,
    ImageDecoder::AlphaOption alpha_option,
    const ColorBehavior& color_behavior) {
  std::unique_ptr<ImageDecoder> actual_decoder =
      ImageDecoder::Create(data, data_complete, alpha_option, color_behavior);
  if (!actual_decoder)
    return nullptr;

  std::unique_ptr<DeferredImageDecoder> decoder(
      new DeferredImageDecoder(std::move(actual_decoder)));

  // Since we've just instantiated a fresh decoder, there's no need to reset its
  // data.
  decoder->SetDataInternal(std::move(data), data_complete, false);

  return decoder;
}

std::unique_ptr<DeferredImageDecoder> DeferredImageDecoder::CreateForTesting(
    std::unique_ptr<ImageDecoder> actual_decoder) {
  return WTF::WrapUnique(new DeferredImageDecoder(std::move(actual_decoder)));
}

DeferredImageDecoder::DeferredImageDecoder(
    std::unique_ptr<ImageDecoder> actual_decoder)
    : actual_decoder_(std::move(actual_decoder)),
      repetition_count_(kAnimationNone),
      all_data_received_(false),
      can_yuv_decode_(false),
      has_hot_spot_(false) {}

DeferredImageDecoder::~DeferredImageDecoder() {}

String DeferredImageDecoder::FilenameExtension() const {
  return actual_decoder_ ? actual_decoder_->FilenameExtension()
                         : filename_extension_;
}

sk_sp<SkImage> DeferredImageDecoder::CreateFrameAtIndex(size_t index) {
  if (frame_generator_ && frame_generator_->DecodeFailed())
    return nullptr;

  PrepareLazyDecodedFrames();

  if (index < frame_data_.size()) {
    DeferredFrameData* frame_data = &frame_data_[index];
    if (actual_decoder_)
      frame_data->frame_bytes_ = actual_decoder_->FrameBytesAtIndex(index);
    else
      frame_data->frame_bytes_ = size_.Area() * sizeof(ImageFrame::PixelData);
    // ImageFrameGenerator has the latest known alpha state. There will be a
    // performance boost if this frame is opaque.
    DCHECK(frame_generator_);
    return CreateFrameImageAtIndex(index, !frame_generator_->HasAlpha(index));
  }

  if (!actual_decoder_ || actual_decoder_->Failed())
    return nullptr;

  ImageFrame* frame = actual_decoder_->FrameBufferAtIndex(index);
  if (!frame || frame->GetStatus() == ImageFrame::kFrameEmpty)
    return nullptr;

  return (frame->GetStatus() == ImageFrame::kFrameComplete)
             ? frame->FinalizePixelsAndGetImage()
             : SkImage::MakeFromBitmap(frame->Bitmap());
}

PassRefPtr<SharedBuffer> DeferredImageDecoder::Data() {
  if (!rw_buffer_)
    return nullptr;
  sk_sp<SkROBuffer> ro_buffer(rw_buffer_->makeROBufferSnapshot());
  RefPtr<SharedBuffer> shared_buffer = SharedBuffer::Create();
  SkROBuffer::Iter it(ro_buffer.get());
  do {
    shared_buffer->Append(static_cast<const char*>(it.data()), it.size());
  } while (it.next());
  return shared_buffer;
}

void DeferredImageDecoder::SetData(PassRefPtr<SharedBuffer> data,
                                   bool all_data_received) {
  SetDataInternal(std::move(data), all_data_received, true);
}

void DeferredImageDecoder::SetDataInternal(RefPtr<SharedBuffer> data,
                                           bool all_data_received,
                                           bool push_data_to_decoder) {
  if (actual_decoder_) {
    all_data_received_ = all_data_received;
    if (push_data_to_decoder)
      actual_decoder_->SetData(data, all_data_received);
    PrepareLazyDecodedFrames();
  }

  if (frame_generator_) {
    if (!rw_buffer_)
      rw_buffer_ = WTF::WrapUnique(new SkRWBuffer(data->size()));

    const char* segment = 0;
    for (size_t length = data->GetSomeData(segment, rw_buffer_->size()); length;
         length = data->GetSomeData(segment, rw_buffer_->size())) {
      DCHECK_GE(data->size(), rw_buffer_->size() + length);
      const size_t remaining = data->size() - rw_buffer_->size() - length;
      rw_buffer_->append(segment, length, remaining);
    }
  }
}

bool DeferredImageDecoder::IsSizeAvailable() {
  // m_actualDecoder is 0 only if image decoding is deferred and that means
  // the image header decoded successfully and the size is available.
  return actual_decoder_ ? actual_decoder_->IsSizeAvailable() : true;
}

bool DeferredImageDecoder::HasEmbeddedColorSpace() const {
  return actual_decoder_ ? actual_decoder_->HasEmbeddedColorSpace()
                         : has_embedded_color_space_;
}

IntSize DeferredImageDecoder::Size() const {
  return actual_decoder_ ? actual_decoder_->Size() : size_;
}

IntSize DeferredImageDecoder::FrameSizeAtIndex(size_t index) const {
  // FIXME: LocalFrame size is assumed to be uniform. This might not be true for
  // future supported codecs.
  return actual_decoder_ ? actual_decoder_->FrameSizeAtIndex(index) : size_;
}

size_t DeferredImageDecoder::FrameCount() {
  return actual_decoder_ ? actual_decoder_->FrameCount() : frame_data_.size();
}

int DeferredImageDecoder::RepetitionCount() const {
  return actual_decoder_ ? actual_decoder_->RepetitionCount()
                         : repetition_count_;
}

size_t DeferredImageDecoder::ClearCacheExceptFrame(size_t clear_except_frame) {
  if (actual_decoder_)
    return actual_decoder_->ClearCacheExceptFrame(clear_except_frame);
  size_t frame_bytes_cleared = 0;
  for (size_t i = 0; i < frame_data_.size(); ++i) {
    if (i != clear_except_frame) {
      frame_bytes_cleared += frame_data_[i].frame_bytes_;
      frame_data_[i].frame_bytes_ = 0;
    }
  }
  return frame_bytes_cleared;
}

bool DeferredImageDecoder::FrameHasAlphaAtIndex(size_t index) const {
  if (actual_decoder_)
    return actual_decoder_->FrameHasAlphaAtIndex(index);
  if (!frame_generator_->IsMultiFrame())
    return frame_generator_->HasAlpha(index);
  return true;
}

bool DeferredImageDecoder::FrameIsReceivedAtIndex(size_t index) const {
  if (actual_decoder_)
    return actual_decoder_->FrameIsReceivedAtIndex(index);
  if (index < frame_data_.size())
    return frame_data_[index].is_received_;
  return false;
}

float DeferredImageDecoder::FrameDurationAtIndex(size_t index) const {
  if (actual_decoder_)
    return actual_decoder_->FrameDurationAtIndex(index);
  if (index < frame_data_.size())
    return frame_data_[index].duration_;
  return 0;
}

size_t DeferredImageDecoder::FrameBytesAtIndex(size_t index) const {
  if (actual_decoder_)
    return actual_decoder_->FrameBytesAtIndex(index);
  if (index < frame_data_.size())
    return frame_data_[index].frame_bytes_;
  return 0;
}

ImageOrientation DeferredImageDecoder::OrientationAtIndex(size_t index) const {
  if (actual_decoder_)
    return actual_decoder_->Orientation();
  if (index < frame_data_.size())
    return frame_data_[index].orientation_;
  return kDefaultImageOrientation;
}

void DeferredImageDecoder::ActivateLazyDecoding() {
  if (frame_generator_)
    return;

  size_ = actual_decoder_->Size();
  has_hot_spot_ = actual_decoder_->HotSpot(hot_spot_);
  filename_extension_ = actual_decoder_->FilenameExtension();
  // JPEG images support YUV decoding; other decoders do not. (WebP could in the
  // future.)
  can_yuv_decode_ = RuntimeEnabledFeatures::DecodeToYUVEnabled() &&
                    (filename_extension_ == "jpg");
  has_embedded_color_space_ = actual_decoder_->HasEmbeddedColorSpace();
  color_space_for_sk_images_ = actual_decoder_->ColorSpaceForSkImages();

  const bool is_single_frame =
      actual_decoder_->RepetitionCount() == kAnimationNone ||
      (all_data_received_ && actual_decoder_->FrameCount() == 1u);
  const SkISize decoded_size =
      SkISize::Make(actual_decoder_->DecodedSize().Width(),
                    actual_decoder_->DecodedSize().Height());
  frame_generator_ = ImageFrameGenerator::Create(
      decoded_size, !is_single_frame, actual_decoder_->GetColorBehavior());
}

void DeferredImageDecoder::PrepareLazyDecodedFrames() {
  if (!actual_decoder_ || !actual_decoder_->IsSizeAvailable())
    return;

  ActivateLazyDecoding();

  const size_t previous_size = frame_data_.size();
  frame_data_.resize(actual_decoder_->FrameCount());

  // We have encountered a broken image file. Simply bail.
  if (frame_data_.size() < previous_size)
    return;

  for (size_t i = previous_size; i < frame_data_.size(); ++i) {
    frame_data_[i].duration_ = actual_decoder_->FrameDurationAtIndex(i);
    frame_data_[i].orientation_ = actual_decoder_->Orientation();
    frame_data_[i].is_received_ = actual_decoder_->FrameIsReceivedAtIndex(i);
  }

  // The last lazy decoded frame created from previous call might be
  // incomplete so update its state.
  if (previous_size) {
    const size_t last_frame = previous_size - 1;
    frame_data_[last_frame].is_received_ =
        actual_decoder_->FrameIsReceivedAtIndex(last_frame);
  }

  if (all_data_received_) {
    repetition_count_ = actual_decoder_->RepetitionCount();
    actual_decoder_.reset();
    // Hold on to m_rwBuffer, which is still needed by createFrameAtIndex.
  }
}

sk_sp<SkImage> DeferredImageDecoder::CreateFrameImageAtIndex(
    size_t index,
    bool known_to_be_opaque) {
  const SkISize& decoded_size = frame_generator_->GetFullSize();
  DCHECK_GT(decoded_size.width(), 0);
  DCHECK_GT(decoded_size.height(), 0);

  sk_sp<SkROBuffer> ro_buffer(rw_buffer_->makeROBufferSnapshot());
  RefPtr<SegmentReader> segment_reader =
      SegmentReader::CreateFromSkROBuffer(std::move(ro_buffer));

  SkImageInfo info = SkImageInfo::MakeN32(
      decoded_size.width(), decoded_size.height(),
      known_to_be_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType,
      color_space_for_sk_images_);

  auto generator = WTF::MakeUnique<DecodingImageGenerator>(
      frame_generator_, info, std::move(segment_reader), all_data_received_,
      index, frame_data_[index].unique_id_);
  generator->SetCanYUVDecode(can_yuv_decode_);
  sk_sp<SkImage> image = SkImage::MakeFromGenerator(std::move(generator));
  if (!image)
    return nullptr;

  // We can consider decoded bitmap constant and reuse uniqueID only after all
  // data is received.  We reuse it also for multiframe images when image data
  // is partially received but the frame data is fully received.
  if (all_data_received_ || frame_data_[index].is_received_) {
    DCHECK(frame_data_[index].unique_id_ ==
               DecodingImageGenerator::kNeedNewImageUniqueID ||
           frame_data_[index].unique_id_ == image->uniqueID());
    frame_data_[index].unique_id_ = image->uniqueID();
  }

  return image;
}

bool DeferredImageDecoder::HotSpot(IntPoint& hot_spot) const {
  if (actual_decoder_)
    return actual_decoder_->HotSpot(hot_spot);
  if (has_hot_spot_)
    hot_spot = hot_spot_;
  return has_hot_spot_;
}

}  // namespace blink

namespace WTF {
template <>
struct VectorTraits<blink::DeferredFrameData>
    : public SimpleClassVectorTraits<blink::DeferredFrameData> {
  STATIC_ONLY(VectorTraits);
  static const bool kCanInitializeWithMemset =
      false;  // Not all DeferredFrameData members initialize to 0.
};
}
