// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include <stdint.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/md5.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "printing/metafile.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"

namespace printing {

Image::Image(const void* metafile_src_buffer, size_t metafile_src_buffer_size)
    : row_length_(0), ignore_alpha_(true) {
  LoadMetafile(metafile_src_buffer, metafile_src_buffer_size);
}

Image::Image(const Image&) = default;
Image::Image(Image&&) = default;

Image::~Image() {}

std::string Image::checksum() const {
  base::MD5Digest digest;
  base::MD5Sum(&data_[0], data_.size(), &digest);
  return base::MD5DigestToBase16(digest);
}

bool Image::SaveToPng(const base::FilePath& filepath) const {
  DCHECK(!data_.empty());
  std::vector<unsigned char> compressed;
  bool success = gfx::PNGCodec::Encode(&*data_.begin(),
                                       gfx::PNGCodec::FORMAT_BGRA,
                                       size_,
                                       row_length_,
                                       true,
                                       std::vector<gfx::PNGCodec::Comment>(),
                                       &compressed);
  DCHECK(success && compressed.size());
  if (success) {
    int write_bytes = base::WriteFile(
        filepath,
        reinterpret_cast<char*>(&*compressed.begin()),
        base::checked_cast<int>(compressed.size()));
    success = (write_bytes == static_cast<int>(compressed.size()));
    DCHECK(success);
  }
  return success;
}
}  // namespace printing
