// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_CANVAS_PAINT_COMMON_H_
#define SKIA_EXT_CANVAS_PAINT_COMMON_H_
#pragma once

namespace skia {
class PlatformCanvas;

template<class T> inline PlatformCanvas* GetPlatformCanvas(T* t) {
  return t;
}

// TODO(pkotwicz): Push scale into PlatformCanvas such that this function
// is not needed.
template<class T> inline void RecreateBackingCanvas(T* t,
    int width, int height, float scale, bool opaque) {
}

}  // namespace skia

#endif  // SKIA_EXT_CANVAS_PAINT_COMMON_H_
