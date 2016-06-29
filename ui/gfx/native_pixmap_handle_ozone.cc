// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_pixmap_handle_ozone.h"

namespace gfx {

NativePixmapPlane::NativePixmapPlane() : stride(0), offset(0), modifier(0) {}

NativePixmapPlane::NativePixmapPlane(int stride, int offset, uint64_t modifier)
    : stride(stride), offset(offset), modifier(modifier) {}

NativePixmapPlane::NativePixmapPlane(const NativePixmapPlane& other) = default;

NativePixmapPlane::~NativePixmapPlane() {}

NativePixmapHandle::NativePixmapHandle() {}
NativePixmapHandle::NativePixmapHandle(const NativePixmapHandle& other) =
    default;

NativePixmapHandle::~NativePixmapHandle() {}

}  // namespace gfx
