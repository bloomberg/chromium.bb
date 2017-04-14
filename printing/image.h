// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_IMAGE_H_
#define PRINTING_IMAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "printing/printing_export.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class FilePath;
}

namespace printing {

// Lightweight raw-bitmap management. The image, once initialized, is immutable.
// The main purpose is testing image contents.
class PRINTING_EXPORT Image {
 public:
  // Creates the image from the metafile.  Deduces bounds based on bounds in
  // metafile.  If loading fails size().IsEmpty() will be true.
  Image(const void* metafile_src_buffer, size_t metafile_src_buffer_size);

  Image(const Image& image);
  Image(Image&& image);

  ~Image();

  const gfx::Size& size() const {
    return size_;
  }

  // Return a checksum of the image (MD5 over the internal data structure).
  std::string checksum() const;

  // Save image as PNG.
  bool SaveToPng(const base::FilePath& filepath) const;

 private:
  bool LoadMetafile(const void* metafile_src_buffer,
                    size_t metafile_src_buffer_size);

  // Pixel dimensions of the image.
  gfx::Size size_;

  // Length of a line in bytes.
  int row_length_;

  // Actual bitmap data in arrays of RGBAs (so when loaded as uint32_t, it's
  // 0xABGR).
  std::vector<unsigned char> data_;

  // Flag to signal if the comparison functions should ignore the alpha channel.
  const bool ignore_alpha_;  // Currently always true.

  // Prevent operator= (this function has no implementation)
  Image& operator=(const Image& image);
};

}  // namespace printing

#endif  // PRINTING_IMAGE_H_
