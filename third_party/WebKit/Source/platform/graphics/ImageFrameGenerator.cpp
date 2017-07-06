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

#include "platform/graphics/ImageFrameGenerator.h"

#include <memory>
#include "SkData.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/PtrUtil.h"
#include "third_party/skia/include/core/SkYUVSizeInfo.h"

namespace blink {

static void CopyPixels(void* dst_addr,
                       size_t dst_row_bytes,
                       const void* src_addr,
                       size_t src_row_bytes,
                       const SkImageInfo& info) {
  size_t row_bytes = info.bytesPerPixel() * info.width();
  for (int y = 0; y < info.height(); ++y) {
    memcpy(dst_addr, src_addr, row_bytes);
    src_addr = static_cast<const char*>(src_addr) + src_row_bytes;
    dst_addr = static_cast<char*>(dst_addr) + dst_row_bytes;
  }
}

static bool CompatibleInfo(const SkImageInfo& src, const SkImageInfo& dst) {
  if (src == dst)
    return true;

  // It is legal to write kOpaque_SkAlphaType pixels into a kPremul_SkAlphaType
  // buffer. This can happen when DeferredImageDecoder allocates an
  // kOpaque_SkAlphaType image generator based on cached frame info, while the
  // ImageFrame-allocated dest bitmap stays kPremul_SkAlphaType.
  if (src.alphaType() == kOpaque_SkAlphaType &&
      dst.alphaType() == kPremul_SkAlphaType) {
    const SkImageInfo& tmp = src.makeAlphaType(kPremul_SkAlphaType);
    return tmp == dst;
  }

  return false;
}

// Creates a SkPixelRef such that the memory for pixels is given by an external
// body. This is used to write directly to the memory given by Skia during
// decoding.
class ExternalMemoryAllocator final : public SkBitmap::Allocator {
  USING_FAST_MALLOC(ExternalMemoryAllocator);
  WTF_MAKE_NONCOPYABLE(ExternalMemoryAllocator);

 public:
  ExternalMemoryAllocator(const SkImageInfo& info,
                          void* pixels,
                          size_t row_bytes)
      : info_(info), pixels_(pixels), row_bytes_(row_bytes) {}

  bool allocPixelRef(SkBitmap* dst, SkColorTable* ctable) override {
    const SkImageInfo& info = dst->info();
    if (kUnknown_SkColorType == info.colorType())
      return false;

    if (!CompatibleInfo(info_, info) || row_bytes_ != dst->rowBytes())
      return false;

    return dst->installPixels(info, pixels_, row_bytes_);
  }

 private:
  SkImageInfo info_;
  void* pixels_;
  size_t row_bytes_;
};

static bool UpdateYUVComponentSizes(ImageDecoder* decoder,
                                    SkISize component_sizes[3],
                                    size_t component_width_bytes[3]) {
  if (!decoder->CanDecodeToYUV())
    return false;

  for (int yuv_index = 0; yuv_index < 3; ++yuv_index) {
    IntSize size = decoder->DecodedYUVSize(yuv_index);
    component_sizes[yuv_index].set(size.Width(), size.Height());
    component_width_bytes[yuv_index] = decoder->DecodedYUVWidthBytes(yuv_index);
  }

  return true;
}

ImageFrameGenerator::ImageFrameGenerator(const SkISize& full_size,
                                         bool is_multi_frame,
                                         const ColorBehavior& color_behavior)
    : full_size_(full_size),
      decoder_color_behavior_(color_behavior),
      is_multi_frame_(is_multi_frame),
      decode_failed_(false),
      yuv_decoding_failed_(false),
      frame_count_(0) {}

ImageFrameGenerator::~ImageFrameGenerator() {
  ImageDecodingStore::Instance().RemoveCacheIndexedByGenerator(this);
}

bool ImageFrameGenerator::DecodeAndScale(
    SegmentReader* data,
    bool all_data_received,
    size_t index,
    const SkImageInfo& info,
    void* pixels,
    size_t row_bytes,
    ImageDecoder::AlphaOption alpha_option) {
  if (decode_failed_)
    return false;

  TRACE_EVENT1("blink", "ImageFrameGenerator::decodeAndScale", "frame index",
               static_cast<int>(index));

  // This implementation does not support scaling so check the requested size.
  SkISize scaled_size = SkISize::Make(info.width(), info.height());
  DCHECK(full_size_ == scaled_size);

  // It is okay to allocate ref-counted ExternalMemoryAllocator on the stack,
  // because 1) it contains references to memory that will be invalid after
  // returning (i.e. a pointer to |pixels|) and therefore 2) should not live
  // longer than the call to the current method.
  ExternalMemoryAllocator external_allocator(info, pixels, row_bytes);
  SkBitmap bitmap =
      TryToResumeDecode(data, all_data_received, index, scaled_size,
                        &external_allocator, alpha_option);
  DCHECK(external_allocator.unique());  // Verify we have the only ref-count.

  if (bitmap.isNull())
    return false;

  // Check to see if the decoder has written directly to the pixel memory
  // provided. If not, make a copy.
  DCHECK_EQ(bitmap.width(), scaled_size.width());
  DCHECK_EQ(bitmap.height(), scaled_size.height());
  if (bitmap.getPixels() != pixels)
    CopyPixels(pixels, row_bytes, bitmap.getPixels(), bitmap.rowBytes(), info);
  return true;
}

bool ImageFrameGenerator::DecodeToYUV(SegmentReader* data,
                                      size_t index,
                                      const SkISize component_sizes[3],
                                      void* planes[3],
                                      const size_t row_bytes[3]) {
  // TODO (scroggo): The only interesting thing this uses from the
  // ImageFrameGenerator is m_decodeFailed. Move this into
  // DecodingImageGenerator, which is the only class that calls it.
  if (decode_failed_)
    return false;

  TRACE_EVENT1("blink", "ImageFrameGenerator::decodeToYUV", "frame index",
               static_cast<int>(index));

  if (!planes || !planes[0] || !planes[1] || !planes[2] || !row_bytes ||
      !row_bytes[0] || !row_bytes[1] || !row_bytes[2]) {
    return false;
  }

  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      data, true, ImageDecoder::kAlphaPremultiplied, decoder_color_behavior_);
  // getYUVComponentSizes was already called and was successful, so
  // ImageDecoder::create must succeed.
  DCHECK(decoder);

  std::unique_ptr<ImagePlanes> image_planes =
      WTF::MakeUnique<ImagePlanes>(planes, row_bytes);
  decoder->SetImagePlanes(std::move(image_planes));

  DCHECK(decoder->CanDecodeToYUV());

  if (decoder->DecodeToYUV()) {
    SetHasAlpha(0, false);  // YUV is always opaque
    return true;
  }

  DCHECK(decoder->Failed());
  yuv_decoding_failed_ = true;
  return false;
}

SkBitmap ImageFrameGenerator::TryToResumeDecode(
    SegmentReader* data,
    bool all_data_received,
    size_t index,
    const SkISize& scaled_size,
    SkBitmap::Allocator* allocator,
    ImageDecoder::AlphaOption alpha_option) {
  TRACE_EVENT1("blink", "ImageFrameGenerator::tryToResumeDecode", "frame index",
               static_cast<int>(index));

  ImageDecoder* decoder = 0;

  // Lock the mutex, so only one thread can use the decoder at once.
  MutexLocker lock(decode_mutex_);
  const bool resume_decoding = ImageDecodingStore::Instance().LockDecoder(
      this, full_size_, alpha_option, &decoder);
  DCHECK(!resume_decoding || decoder);

  SkBitmap full_size_image;
  bool complete = Decode(data, all_data_received, index, &decoder,
                         &full_size_image, allocator, alpha_option);

  if (!decoder)
    return SkBitmap();

  // If we are not resuming decoding that means the decoder is freshly
  // created and we have ownership. If we are resuming decoding then
  // the decoder is owned by ImageDecodingStore.
  std::unique_ptr<ImageDecoder> decoder_container;
  if (!resume_decoding)
    decoder_container = WTF::WrapUnique(decoder);

  if (full_size_image.isNull()) {
    // If decoding has failed, we can save work in the future by
    // ignoring further requests to decode the image.
    decode_failed_ = decoder->Failed();
    if (resume_decoding)
      ImageDecodingStore::Instance().UnlockDecoder(this, decoder);
    return SkBitmap();
  }

  bool remove_decoder = false;
  if (complete) {
    // Free as much memory as possible.  For single-frame images, we can
    // just delete the decoder entirely.  For multi-frame images, we keep
    // the decoder around in order to preserve decoded information such as
    // the required previous frame indexes, but if we've reached the last
    // frame we can at least delete all the cached frames.  (If we were to
    // do this before reaching the last frame, any subsequent requested
    // frames which relied on the current frame would trigger extra
    // re-decoding of all frames in the dependency chain.)
    if (!is_multi_frame_)
      remove_decoder = true;
    else if (index == frame_count_ - 1)
      decoder->ClearCacheExceptFrame(kNotFound);
  }

  if (resume_decoding) {
    if (remove_decoder)
      ImageDecodingStore::Instance().RemoveDecoder(this, decoder);
    else
      ImageDecodingStore::Instance().UnlockDecoder(this, decoder);
  } else if (!remove_decoder) {
    ImageDecodingStore::Instance().InsertDecoder(this,
                                                 std::move(decoder_container));
  }
  return full_size_image;
}

void ImageFrameGenerator::SetHasAlpha(size_t index, bool has_alpha) {
  MutexLocker lock(alpha_mutex_);
  if (index >= has_alpha_.size()) {
    const size_t old_size = has_alpha_.size();
    has_alpha_.resize(index + 1);
    for (size_t i = old_size; i < has_alpha_.size(); ++i)
      has_alpha_[i] = true;
  }
  has_alpha_[index] = has_alpha;
}

bool ImageFrameGenerator::Decode(SegmentReader* data,
                                 bool all_data_received,
                                 size_t index,
                                 ImageDecoder** decoder,
                                 SkBitmap* bitmap,
                                 SkBitmap::Allocator* allocator,
                                 ImageDecoder::AlphaOption alpha_option) {
#if DCHECK_IS_ON()
  DCHECK(decode_mutex_.Locked());
#endif
  TRACE_EVENT2("blink", "ImageFrameGenerator::decode", "width",
               full_size_.width(), "height", full_size_.height());

  // Try to create an ImageDecoder if we are not given one.
  DCHECK(decoder);
  bool new_decoder = false;
  bool should_call_set_data = true;
  if (!*decoder) {
    new_decoder = true;
    if (image_decoder_factory_)
      *decoder = image_decoder_factory_->Create().release();

    if (!*decoder) {
      *decoder = ImageDecoder::Create(data, all_data_received, alpha_option,
                                      decoder_color_behavior_)
                     .release();
      // The newly created decoder just grabbed the data.  No need to reset it.
      should_call_set_data = false;
    }

    if (!*decoder)
      return false;
  }

  if (should_call_set_data)
    (*decoder)->SetData(data, all_data_received);

  bool using_external_allocator = false;

  // For multi-frame image decoders, we need to know how many frames are
  // in that image in order to release the decoder when all frames are
  // decoded. frameCount() is reliable only if all data is received and set in
  // decoder, particularly with GIF.
  if (all_data_received) {
    frame_count_ = (*decoder)->FrameCount();
    // TODO (scroggo): If !m_isMultiFrame && newDecoder && allDataReceived, it
    // should always be the case that 1u == m_frameCount. But it looks like it
    // is currently possible for m_frameCount to be another value.
    if (!is_multi_frame_ && new_decoder && 1u == frame_count_) {
      // If we're using an external memory allocator that means we're decoding
      // directly into the output memory and we can save one memcpy.
      DCHECK(allocator);
      (*decoder)->SetMemoryAllocator(allocator);
      using_external_allocator = true;
    }
  }

  ImageFrame* frame = (*decoder)->FrameBufferAtIndex(index);

  // SetMemoryAllocator() can try to access decoder's data, so
  // we have to do it before clearing SegmentReader.
  if (using_external_allocator)
    (*decoder)->SetMemoryAllocator(nullptr);
  (*decoder)->SetData(PassRefPtr<SegmentReader>(nullptr),
                      false);  // Unref SegmentReader from ImageDecoder.
  (*decoder)->ClearCacheExceptFrame(index);

  if (!frame || frame->GetStatus() == ImageFrame::kFrameEmpty)
    return false;

  // A cache object is considered complete if we can decode a complete frame.
  // Or we have received all data. The image might not be fully decoded in
  // the latter case.
  const bool is_decode_complete =
      frame->GetStatus() == ImageFrame::kFrameComplete || all_data_received;

  SkBitmap full_size_bitmap = frame->Bitmap();
  if (!full_size_bitmap.isNull()) {
    DCHECK_EQ(full_size_bitmap.width(), full_size_.width());
    DCHECK_EQ(full_size_bitmap.height(), full_size_.height());
    SetHasAlpha(index, !full_size_bitmap.isOpaque());
  }

  *bitmap = full_size_bitmap;
  return is_decode_complete;
}

bool ImageFrameGenerator::HasAlpha(size_t index) {
  MutexLocker lock(alpha_mutex_);
  if (index < has_alpha_.size())
    return has_alpha_[index];
  return true;
}

bool ImageFrameGenerator::GetYUVComponentSizes(SegmentReader* data,
                                               SkYUVSizeInfo* size_info) {
  TRACE_EVENT2("blink", "ImageFrameGenerator::getYUVComponentSizes", "width",
               full_size_.width(), "height", full_size_.height());

  if (yuv_decoding_failed_)
    return false;

  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      data, true, ImageDecoder::kAlphaPremultiplied, decoder_color_behavior_);
  if (!decoder)
    return false;

  // Setting a dummy ImagePlanes object signals to the decoder that we want to
  // do YUV decoding.
  std::unique_ptr<ImagePlanes> dummy_image_planes =
      WTF::WrapUnique(new ImagePlanes);
  decoder->SetImagePlanes(std::move(dummy_image_planes));

  return UpdateYUVComponentSizes(decoder.get(), size_info->fSizes,
                                 size_info->fWidthBytes);
}

}  // namespace blink
