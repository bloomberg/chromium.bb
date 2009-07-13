// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_IMAGE_H_
#define PRINTING_IMAGE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/size.h"
#include "base/logging.h"

namespace printing {

// Lightweight raw-bitmap management. The image, once initialized, is immuable.
// The main purpose is testing image contents.
class Image {
 public:
  // Creates the image from the given filename on disk.  Uses extension to
  // defer file type. PNG and EMF (on Windows) currently supported.
  // If image loading fails size().IsEmpty() will be true.
  explicit Image(const std::wstring& filename);

  const gfx::Size& size() const {
    return size_;
  }

  // Save image as PNG.
  bool SaveToPng(const std::wstring& filename);

  // Returns % of pixels different
  double PercentageDifferent(const Image& rhs) const;

  // Returns the 0x0RGB or 0xARGB value of the pixel at the given location.
  uint32 Color(uint32 color) const {
    if (ignore_alpha_)
      return color & 0xFFFFFF;  // Strip out A.
    else
      return color;
  }

  uint32 pixel_at(int x, int y) const {
    DCHECK(x >= 0 && x < size_.width());
    DCHECK(y >= 0 && y < size_.height());
    const uint32* data = reinterpret_cast<const uint32*>(&*data_.begin());
    const uint32* data_row = data + y * row_length_ / sizeof(uint32);
    return Color(data_row[x]);
  }

 private:
  bool LoadPng(const std::string& compressed);

  bool LoadMetafile(const std::string& data);

  // Pixel dimensions of the image.
  gfx::Size size_;

  // Length of a line in bytes.
  int row_length_;

  // Actual bitmap data in arrays of RGBAs (so when loaded as uint32, it's
  // 0xABGR).
  std::vector<unsigned char> data_;

  // Flag to signal if the comparison functions should ignore the alpha channel.
  const bool ignore_alpha_;  // Currently always true.

  DISALLOW_EVIL_CONSTRUCTORS(Image);
};

}  // namespace printing

#endif  // PRINTING_IMAGE_H_
