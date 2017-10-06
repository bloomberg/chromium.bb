// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BitmapGlyphsBlacklist_h
#define BitmapGlyphsBlacklist_h

#include "platform/PlatformExport.h"

class SkTypeface;

namespace blink {

class PLATFORM_EXPORT BitmapGlyphsBlacklist {
 public:
  static bool AvoidEmbeddedBitmapsForTypeface(SkTypeface*);
};

}  // namespace blink

#endif
