// Copyright 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFontRendering_h
#define WebFontRendering_h

#include "public/platform/WebCommon.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkFontMgr;
class SkTypeface;

namespace blink {

class WebFontRendering {
 public:
  BLINK_EXPORT static void SetSkiaFontManager(sk_sp<SkFontMgr>);
  BLINK_EXPORT static void SetDeviceScaleFactor(float);
  BLINK_EXPORT static void AddSideloadedFontForTesting(sk_sp<SkTypeface>);
  BLINK_EXPORT static void SetMenuFontMetrics(const wchar_t* family_name,
                                              int32_t font_height);
  BLINK_EXPORT static void SetSmallCaptionFontMetrics(
      const wchar_t* family_name,
      int32_t font_height);
  BLINK_EXPORT static void SetStatusFontMetrics(const wchar_t* family_name,
                                                int32_t font_height);
  BLINK_EXPORT static void SetAntialiasedTextEnabled(bool);
  BLINK_EXPORT static void SetLCDTextEnabled(bool);
  BLINK_EXPORT static void SetUseSkiaFontFallback(bool);
};

}  // namespace blink

#endif
