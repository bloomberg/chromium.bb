// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_NSIMAGE_CACHE_H_
#define UI_GFX_MAC_NSIMAGE_CACHE_H_
#pragma once

#include "ui/ui_api.h"

#ifdef __OBJC__
@class NSImage;
@class NSString;
#else
class NSImage;
class NSString;
#endif

namespace gfx {

// Returns an autoreleased image from the main app bundle
// (mac_util::MainAppBundle()) with the given name, and keeps it in memory so
// future fetches are fast.
// NOTE:
//   - This should only be called on the main thread.
//   - The caller should retain the image if they want to keep it around, as
//     the cache could have limit on size/lifetime, etc.
UI_API NSImage* GetCachedImageWithName(NSString* name);

// Clears the image cache.
void ClearCachedImages(void);

}  // namespace gfx

#endif  // UI_GFX_MAC_NSIMAGE_CACHE_H_
