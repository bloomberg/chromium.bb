// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/image_decoder.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebImage.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/skia/include/core/SkBitmap.h"

using WebKit::WebData;
using WebKit::WebImage;

namespace webkit_glue {

ImageDecoder::ImageDecoder() : desired_icon_size_(0, 0) {
}

ImageDecoder::ImageDecoder(const gfx::Size& desired_icon_size)
    : desired_icon_size_(desired_icon_size) {
}

ImageDecoder::~ImageDecoder() {
}

SkBitmap ImageDecoder::Decode(const unsigned char* data, size_t size) const {
  const WebImage& image = WebImage::fromData(
      WebData(reinterpret_cast<const char*>(data), size), desired_icon_size_);
  return image.getSkBitmap();
}

// static
std::vector<SkBitmap> ImageDecoder::DecodeAll(
      const unsigned char* data, size_t size) {
  const WebKit::WebVector<WebImage>& images = WebImage::framesFromData(
      WebData(reinterpret_cast<const char*>(data), size));
  std::vector<SkBitmap> result;
  for (size_t i = 0; i < images.size(); ++i)
    result.push_back(images[i].getSkBitmap());
  return result;
}

}  // namespace webkit_glue
