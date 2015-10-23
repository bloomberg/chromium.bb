// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BUFFER_TYPES_H_
#define UI_GFX_BUFFER_TYPES_H_

namespace gfx {

// The format needs to be taken into account when mapping a buffer into the
// client's address space.
enum class BufferFormat {
  ATC,
  ATCIA,
  DXT1,
  DXT5,
  ETC1,
  R_8,
  RGBA_4444,
  RGBX_8888,
  RGBA_8888,
  BGRX_8888,
  BGRA_8888,
  YUV_420,
  YUV_420_BIPLANAR,
  UYVY_422,

  LAST = UYVY_422
};

// The usage mode affects how a buffer can be used. Only buffers created with
// MAP can be mapped into the client's address space and accessed by the CPU.
// PERSISTENT_MAP adds the additional condition that successive Map() calls
// (with Unmap() calls between) will return a pointer to the same memory
// contents.
enum class BufferUsage { MAP, PERSISTENT_MAP, SCANOUT, LAST = SCANOUT };

}  // namespace gfx

#endif  // UI_GFX_BUFFER_TYPES_H_
