// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-encoders/ImageEncoder.h"

namespace blink {

bool ImageEncoder::Encode(Vector<unsigned char>* dst,
                          const SkPixmap& src,
                          const SkJpegEncoder::Options& options) {
  VectorWStream dst_stream(dst);
  return SkJpegEncoder::Encode(&dst_stream, src, options);
}

bool ImageEncoder::Encode(Vector<unsigned char>* dst,
                          const SkPixmap& src,
                          const SkPngEncoder::Options& options) {
  VectorWStream dst_stream(dst);
  return SkPngEncoder::Encode(&dst_stream, src, options);
}

bool ImageEncoder::Encode(Vector<unsigned char>* dst,
                          const SkPixmap& src,
                          const SkWebpEncoder::Options& options) {
  VectorWStream dst_stream(dst);
  return SkWebpEncoder::Encode(&dst_stream, src, options);
}

std::unique_ptr<ImageEncoder> ImageEncoder::Create(
    Vector<unsigned char>* dst,
    const SkPixmap& src,
    const SkJpegEncoder::Options& options) {
  std::unique_ptr<ImageEncoder> image_encoder(new ImageEncoder(dst));
  image_encoder->encoder_ =
      SkJpegEncoder::Make(&image_encoder->dst_, src, options);
  if (!image_encoder->encoder_) {
    return nullptr;
  }

  return image_encoder;
}

std::unique_ptr<ImageEncoder> ImageEncoder::Create(
    Vector<unsigned char>* dst,
    const SkPixmap& src,
    const SkPngEncoder::Options& options) {
  std::unique_ptr<ImageEncoder> image_encoder(new ImageEncoder(dst));
  image_encoder->encoder_ =
      SkPngEncoder::Make(&image_encoder->dst_, src, options);
  if (!image_encoder->encoder_) {
    return nullptr;
  }

  return image_encoder;
}

int ImageEncoder::ComputeJpegQuality(double quality) {
  int compression_quality = 92;  // Default value
  if (0.0f <= quality && quality <= 1.0)
    compression_quality = static_cast<int>(quality * 100 + 0.5);
  return compression_quality;
}

SkWebpEncoder::Options ImageEncoder::ComputeWebpOptions(
    double quality,
    SkTransferFunctionBehavior unpremulBehavior) {
  SkWebpEncoder::Options options;
  options.fUnpremulBehavior = unpremulBehavior;

  if (quality == 1.0) {
    // Choose a lossless encode.  When performing a lossless encode, higher
    // quality corresponds to slower encoding and smaller output size.
    options.fCompression = SkWebpEncoder::Compression::kLossless;
    options.fQuality = 75.0f;
  } else {
    options.fQuality = 80.0f;  // Default value
    if (0.0f <= quality && quality <= 1.0)
      options.fQuality = quality * 100.0f;
  }

  return options;
}
};
