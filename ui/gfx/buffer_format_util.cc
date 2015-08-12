// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/buffer_format_util.h"

#include "base/logging.h"

namespace gfx {

size_t NumberOfPlanesForBufferFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::ATC:
    case BufferFormat::ATCIA:
    case BufferFormat::DXT1:
    case BufferFormat::DXT5:
    case BufferFormat::ETC1:
    case BufferFormat::R_8:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_8888:
    case BufferFormat::RGBX_8888:
    case BufferFormat::BGRA_8888:
      return 1;
    case BufferFormat::YUV_420:
      return 3;
  }
  NOTREACHED();
  return 0;
}

}  // namespace gfx
