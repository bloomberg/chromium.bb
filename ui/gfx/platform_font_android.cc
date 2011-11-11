// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font.h"

#include "base/logging.h"

namespace gfx {

// static
PlatformFont* PlatformFont::CreateDefault() {
  NOTIMPLEMENTED();
  return NULL;
}

// static
PlatformFont* PlatformFont::CreateFromFont(const Font& other) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const string16& font_name,
                                                  int font_size) {
  NOTIMPLEMENTED();
  return NULL;
}

} // namespace gfx
