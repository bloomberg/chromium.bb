// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/test_fonts.h"

#include "skia/ext/fontmgr_default.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_android.h"

namespace skia {

void ConfigureTestFont() {
  // TODO(https://crbugs.com/927980): Use SkFontMgr_Fuchsia instead of
  // SkFontMgr_Android. We just need to start a fonts::Provider instance with
  // a custom manifest files and connect to it instead of the default
  // fonts::Provider instance..
  SkFontMgr_Android_CustomFonts custom;
  custom.fSystemFontUse = SkFontMgr_Android_CustomFonts::kOnlyCustom;
  custom.fBasePath = "/pkg/test_fonts/";
  custom.fFontsXml = "/pkg/test_fonts/android_main_fonts.xml";
  custom.fFallbackFontsXml = "/pkg/test_fonts/android_fallback_fonts.xml";
  custom.fIsolated = false;

  skia::OverrideDefaultSkFontMgr(SkFontMgr_New_Android(&custom));
}

}  // namespace skia
