// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJO_BUFFER_TYPES_ENUM_TRAITS_H_
#define UI_GFX_MOJO_BUFFER_TYPES_ENUM_TRAITS_H_

#include "ui/gfx/buffer_types.h"
#include "ui/gfx/mojo/buffer_types.mojom.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::BufferFormat, gfx::BufferFormat> {
  static gfx::mojom::BufferFormat ToMojom(gfx::BufferFormat format) {
    switch (format) {
      case gfx::BufferFormat::ATC:
        return gfx::mojom::BufferFormat::ATC;
      case gfx::BufferFormat::ATCIA:
        return gfx::mojom::BufferFormat::ATCIA;
      case gfx::BufferFormat::DXT1:
        return gfx::mojom::BufferFormat::DXT1;
      case gfx::BufferFormat::DXT5:
        return gfx::mojom::BufferFormat::DXT5;
      case gfx::BufferFormat::ETC1:
        return gfx::mojom::BufferFormat::ETC1;
      case gfx::BufferFormat::R_8:
        return gfx::mojom::BufferFormat::R_8;
      case gfx::BufferFormat::BGR_565:
        return gfx::mojom::BufferFormat::BGR_565;
      case gfx::BufferFormat::RGBA_4444:
        return gfx::mojom::BufferFormat::RGBA_4444;
      case gfx::BufferFormat::RGBX_8888:
        return gfx::mojom::BufferFormat::RGBX_8888;
      case gfx::BufferFormat::RGBA_8888:
        return gfx::mojom::BufferFormat::RGBA_8888;
      case gfx::BufferFormat::BGRX_8888:
        return gfx::mojom::BufferFormat::BGRX_8888;
      case gfx::BufferFormat::BGRA_8888:
        return gfx::mojom::BufferFormat::BGRA_8888;
      case gfx::BufferFormat::YVU_420:
        return gfx::mojom::BufferFormat::YVU_420;
      case gfx::BufferFormat::YUV_420_BIPLANAR:
        return gfx::mojom::BufferFormat::YUV_420_BIPLANAR;
      case gfx::BufferFormat::UYVY_422:
        return gfx::mojom::BufferFormat::UYVY_422;
    }
    NOTREACHED();
    return gfx::mojom::BufferFormat::LAST;
  }

  static bool FromMojom(gfx::mojom::BufferFormat input,
                        gfx::BufferFormat* out) {
    switch (input) {
      case gfx::mojom::BufferFormat::ATC:
        *out = gfx::BufferFormat::ATC;
        return true;
      case gfx::mojom::BufferFormat::ATCIA:
        *out = gfx::BufferFormat::ATCIA;
        return true;
      case gfx::mojom::BufferFormat::DXT1:
        *out = gfx::BufferFormat::DXT1;
        return true;
      case gfx::mojom::BufferFormat::DXT5:
        *out = gfx::BufferFormat::DXT5;
        return true;
      case gfx::mojom::BufferFormat::ETC1:
        *out = gfx::BufferFormat::ETC1;
        return true;
      case gfx::mojom::BufferFormat::R_8:
        *out = gfx::BufferFormat::R_8;
        return true;
      case gfx::mojom::BufferFormat::BGR_565:
        *out = gfx::BufferFormat::BGR_565;
        return true;
      case gfx::mojom::BufferFormat::RGBA_4444:
        *out = gfx::BufferFormat::RGBA_4444;
        return true;
      case gfx::mojom::BufferFormat::RGBX_8888:
        *out = gfx::BufferFormat::RGBX_8888;
        return true;
      case gfx::mojom::BufferFormat::RGBA_8888:
        *out = gfx::BufferFormat::RGBA_8888;
        return true;
      case gfx::mojom::BufferFormat::BGRX_8888:
        *out = gfx::BufferFormat::BGRX_8888;
        return true;
      case gfx::mojom::BufferFormat::BGRA_8888:
        *out = gfx::BufferFormat::BGRA_8888;
        return true;
      case gfx::mojom::BufferFormat::YVU_420:
        *out = gfx::BufferFormat::YVU_420;
        return true;
      case gfx::mojom::BufferFormat::YUV_420_BIPLANAR:
        *out = gfx::BufferFormat::YUV_420_BIPLANAR;
        return true;
      case gfx::mojom::BufferFormat::UYVY_422:
        *out = gfx::BufferFormat::UYVY_422;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct EnumTraits<gfx::mojom::BufferUsage, gfx::BufferUsage> {
  static gfx::mojom::BufferUsage ToMojom(gfx::BufferUsage usage) {
    switch (usage) {
      case gfx::BufferUsage::GPU_READ:
        return gfx::mojom::BufferUsage::GPU_READ;
      case gfx::BufferUsage::SCANOUT:
        return gfx::mojom::BufferUsage::SCANOUT;
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
        return gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE;
      case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
        return gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT;
    }
    NOTREACHED();
    return gfx::mojom::BufferUsage::LAST;
  }

  static bool FromMojom(gfx::mojom::BufferUsage input, gfx::BufferUsage* out) {
    switch (input) {
      case gfx::mojom::BufferUsage::GPU_READ:
        *out = gfx::BufferUsage::GPU_READ;
        return true;
      case gfx::mojom::BufferUsage::SCANOUT:
        *out = gfx::BufferUsage::SCANOUT;
        return true;
      case gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE:
        *out = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
        return true;
      case gfx::mojom::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
        *out = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJO_BUFFER_TYPES_ENUM_TRAITS_H_
