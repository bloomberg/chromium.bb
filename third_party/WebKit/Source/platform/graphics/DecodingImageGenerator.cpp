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

#include "platform/graphics/DecodingImageGenerator.h"

#include <memory>
#include "platform/SharedBuffer.h"
#include "platform/graphics/ImageFrameGenerator.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/SegmentReader.h"
#include "platform/instrumentation/PlatformInstrumentation.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "third_party/skia/include/core/SkData.h"

namespace blink {

DecodingImageGenerator::DecodingImageGenerator(
    PassRefPtr<ImageFrameGenerator> frame_generator,
    const SkImageInfo& info,
    PassRefPtr<SegmentReader> data,
    bool all_data_received,
    size_t index,
    uint32_t unique_id)
    : SkImageGenerator(info, unique_id),
      frame_generator_(std::move(frame_generator)),
      data_(data),
      all_data_received_(all_data_received),
      frame_index_(index),
      can_yuv_decode_(false) {}

DecodingImageGenerator::~DecodingImageGenerator() {}

SkData* DecodingImageGenerator::onRefEncodedData(GrContext* ctx) {
  TRACE_EVENT0("blink", "DecodingImageGenerator::refEncodedData");

  // The GPU only wants the data if it has all been received, since the GPU
  // only wants a complete texture. getAsSkData() may require copying, so
  // skip it and just return nullptr to avoid a slowdown. (See
  // crbug.com/568016 for details about such a slowdown.)
  // TODO (scroggo): Stop relying on the internal knowledge of how Skia uses
  // this. skbug.com/5485
  if (ctx && !all_data_received_)
    return nullptr;

  // Other clients are serializers, which want the data even if it requires
  // copying, and even if the data is incomplete. (Otherwise they would
  // potentially need to decode the partial image in order to re-encode it.)
  return data_->GetAsSkData().release();
}

static void doColorSpaceXform(const SkImageInfo& dst_info,
                              void* pixels,
                              size_t row_bytes,
                              SkColorSpace* src_color_space) {
  TRACE_EVENT0("blink", "DecodingImageGenerator::getPixels - apply xform");
  std::unique_ptr<SkColorSpaceXform> xform =
      SkColorSpaceXform::New(src_color_space, dst_info.colorSpace());

  uint32_t* row = reinterpret_cast<uint32_t*>(pixels);
  for (int y = 0; y < dst_info.height(); y++) {
    SkColorSpaceXform::ColorFormat format =
        SkColorSpaceXform::kRGBA_8888_ColorFormat;
    if (kN32_SkColorType == kBGRA_8888_SkColorType) {
      format = SkColorSpaceXform::kBGRA_8888_ColorFormat;
    }
    SkAlphaType alpha_type =
        dst_info.isOpaque() ? kOpaque_SkAlphaType : kUnpremul_SkAlphaType;
    bool xformed =
        xform->apply(format, row, format, row, dst_info.width(), alpha_type);
    DCHECK(xformed);

    // To be compatible with dst space blending, premultiply in the dst space.
    if (kPremul_SkAlphaType == dst_info.alphaType()) {
      for (int x = 0; x < dst_info.width(); x++) {
        row[x] =
            SkPreMultiplyARGB(SkGetPackedA32(row[x]), SkGetPackedR32(row[x]),
                              SkGetPackedG32(row[x]), SkGetPackedB32(row[x]));
      }
    }

    row = reinterpret_cast<uint32_t*>(
        (reinterpret_cast<uint8_t*>(row) + row_bytes));
  }
}

bool DecodingImageGenerator::onGetPixels(const SkImageInfo& dst_info,
                                         void* pixels,
                                         size_t row_bytes,
                                         SkPMColor*,
                                         int*) {
  TRACE_EVENT1("blink", "DecodingImageGenerator::getPixels", "frame index",
               static_cast<int>(frame_index_));

  // Implementation doesn't support scaling yet, so make sure we're not given a
  // different size.
  if (dst_info.dimensions() != getInfo().dimensions()) {
    return false;
  }

  if (dst_info.colorType() != kN32_SkColorType) {
    return false;
  }

  // Skip the check for alphaType.  blink::ImageFrame may have changed the
  // owning SkBitmap to kOpaque_SkAlphaType after fully decoding the image
  // frame, so if we see a request for opaque, that is ok even if our initial
  // alpha type was not opaque.

  // Pass decodeColorSpace to the decoder.  That is what we can expect the
  // output to be.
  SkColorSpace* decode_color_space = getInfo().colorSpace();
  SkImageInfo decode_info =
      dst_info.makeColorSpace(sk_ref_sp(decode_color_space));

  const bool needs_color_xform =
      decode_color_space && dst_info.colorSpace() &&
      !SkColorSpace::Equals(decode_color_space, dst_info.colorSpace());
  ImageDecoder::AlphaOption alpha_option = ImageDecoder::kAlphaPremultiplied;
  if (needs_color_xform && !decode_info.isOpaque()) {
    alpha_option = ImageDecoder::kAlphaNotPremultiplied;
    decode_info = decode_info.makeAlphaType(kUnpremul_SkAlphaType);
  }

  PlatformInstrumentation::WillDecodeLazyPixelRef(uniqueID());
  bool decoded = frame_generator_->DecodeAndScale(
      data_.Get(), all_data_received_, frame_index_, decode_info, pixels,
      row_bytes, alpha_option);
  PlatformInstrumentation::DidDecodeLazyPixelRef();

  if (decoded && needs_color_xform) {
    doColorSpaceXform(dst_info, pixels, row_bytes, decode_color_space);
  }

  return decoded;
}

bool DecodingImageGenerator::onQueryYUV8(SkYUVSizeInfo* size_info,
                                         SkYUVColorSpace* color_space) const {
  // YUV decoding does not currently support progressive decoding. See comment
  // in ImageFrameGenerator.h.
  if (!can_yuv_decode_ || !all_data_received_)
    return false;

  TRACE_EVENT1("blink", "DecodingImageGenerator::queryYUV8", "sizes",
               static_cast<int>(frame_index_));

  if (color_space)
    *color_space = kJPEG_SkYUVColorSpace;

  return frame_generator_->GetYUVComponentSizes(data_.Get(), size_info);
}

bool DecodingImageGenerator::onGetYUV8Planes(const SkYUVSizeInfo& size_info,
                                             void* planes[3]) {
  // YUV decoding does not currently support progressive decoding. See comment
  // in ImageFrameGenerator.h.
  DCHECK(can_yuv_decode_);
  DCHECK(all_data_received_);

  TRACE_EVENT1("blink", "DecodingImageGenerator::getYUV8Planes", "frame index",
               static_cast<int>(frame_index_));

  PlatformInstrumentation::WillDecodeLazyPixelRef(uniqueID());
  bool decoded =
      frame_generator_->DecodeToYUV(data_.Get(), frame_index_, size_info.fSizes,
                                    planes, size_info.fWidthBytes);
  PlatformInstrumentation::DidDecodeLazyPixelRef();

  return decoded;
}

SkImageGenerator* DecodingImageGenerator::Create(SkData* data) {
  RefPtr<SegmentReader> segment_reader =
      SegmentReader::CreateFromSkData(sk_ref_sp(data));
  // We just need the size of the image, so we have to temporarily create an
  // ImageDecoder. Since we only need the size, the premul and gamma settings
  // don't really matter.
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      segment_reader, true, ImageDecoder::kAlphaPremultiplied,
      ColorBehavior::TransformToGlobalTarget());
  if (!decoder || !decoder->IsSizeAvailable())
    return nullptr;

  const IntSize size = decoder->Size();
  const SkImageInfo info =
      SkImageInfo::MakeN32(size.Width(), size.Height(), kPremul_SkAlphaType,
                           decoder->ColorSpaceForSkImages());

  RefPtr<ImageFrameGenerator> frame =
      ImageFrameGenerator::Create(SkISize::Make(size.Width(), size.Height()),
                                  false, decoder->GetColorBehavior());
  if (!frame)
    return nullptr;

  return new DecodingImageGenerator(frame, info, segment_reader.Release(), true,
                                    0);
}

}  // namespace blink
