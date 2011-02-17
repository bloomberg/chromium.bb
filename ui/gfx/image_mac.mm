// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
namespace internal {

const SkBitmap* NSImageToSkBitmap(NSImage* image) {
  return new SkBitmap(::gfx::NSImageToSkBitmap(image, [image size], false));
}

}  // namespace internal
}  // namespace gfx
