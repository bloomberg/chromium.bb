// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

}  // namespace skia

#endif  // SKIA_EXT_CANVAS_PAINT_COMMON_H_
