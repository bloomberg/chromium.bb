// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/win/WebFontRendering.h"

#include "platform/fonts/FontCache.h"

namespace blink {

// static
void WebFontRendering::SetSkiaFontManager(sk_sp<SkFontMgr> font_mgr) {
  FontCache::SetFontManager(std::move(font_mgr));
}

// static
void WebFontRendering::SetDeviceScaleFactor(float device_scale_factor) {
  FontCache::SetDeviceScaleFactor(device_scale_factor);
}

// static
void WebFontRendering::AddSideloadedFontForTesting(SkTypeface* typeface) {
  FontCache::AddSideloadedFontForTesting(typeface);
}

// static
void WebFontRendering::SetMenuFontMetrics(const wchar_t* family_name,
                                          int32_t font_height) {
  FontCache::SetMenuFontMetrics(family_name, font_height);
}

// static
void WebFontRendering::SetSmallCaptionFontMetrics(const wchar_t* family_name,
                                                  int32_t font_height) {
  FontCache::SetSmallCaptionFontMetrics(family_name, font_height);
}

// static
void WebFontRendering::SetStatusFontMetrics(const wchar_t* family_name,
                                            int32_t font_height) {
  FontCache::SetStatusFontMetrics(family_name, font_height);
}

// static
void WebFontRendering::SetAntialiasedTextEnabled(bool enabled) {
  FontCache::SetAntialiasedTextEnabled(enabled);
}

// static
void WebFontRendering::SetLCDTextEnabled(bool enabled) {
  FontCache::SetLCDTextEnabled(enabled);
}

// static
void WebFontRendering::SetUseSkiaFontFallback(bool use_skia_font_fallback) {
  FontCache::SetUseSkiaFontFallback(use_skia_font_fallback);
}

}  // namespace blink
