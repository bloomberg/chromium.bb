// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/public/cpp/image_processor.h"

#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace image_annotation {

namespace {

// Returns a scaled version of the given bitmap.
SkBitmap ScaleImage(const SkBitmap& source, const float scale) {
  // Set up another bitmap to hold the scaled image.
  SkBitmap dest;
  dest.setInfo(source.info().makeWH(static_cast<int>(scale * source.width()),
                                    static_cast<int>(scale * source.height())));
  dest.allocPixels();
  dest.eraseColor(0);

  // Use a canvas to scale the source image onto the new bitmap.
  SkCanvas canvas(dest);
  canvas.scale(scale, scale);
  canvas.drawBitmap(source, 0, 0, nullptr /* paint */);

  return dest;
}

// Returns the bytes for a scaled and re-encoded version of the given bitmap.
std::vector<uint8_t> ScaleAndEncodeImage(const SkBitmap& image,
                                         const int max_pixels,
                                         const int jpg_quality) {
  const int num_pixels = image.width() * image.height();
  const SkBitmap scaled_image =
      num_pixels <= max_pixels
          ? image
          : ScaleImage(image, std::sqrt(1.0 * max_pixels / num_pixels));

  std::vector<uint8_t> encoded;
  if (!gfx::JPEGCodec::Encode(scaled_image, jpg_quality, &encoded))
    encoded.clear();

  return encoded;
}

}  // namespace

// static
constexpr int ImageProcessor::kMaxPixels;
// static
constexpr int ImageProcessor::kJpgQuality;

ImageProcessor::ImageProcessor(base::RepeatingCallback<SkBitmap()> get_pixels)
    : get_pixels_(std::move(get_pixels)),
      background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

ImageProcessor::~ImageProcessor() = default;

mojom::ImageProcessorPtr ImageProcessor::GetPtr() {
  mojom::ImageProcessorPtr ptr;
  bindings_.AddBinding(this, mojo::MakeRequest(&ptr));
  return ptr;
}

void ImageProcessor::GetJpgImageData(GetJpgImageDataCallback callback) {
  PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ScaleAndEncodeImage, get_pixels_.Run(), kMaxPixels,
                     kJpgQuality),
      std::move(callback));
}

}  // namespace image_annotation
