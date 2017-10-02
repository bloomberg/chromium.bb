// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/LocalFontFaceSource.h"

#include "platform/Histogram.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"

namespace blink {

bool LocalFontFaceSource::IsLocalFontAvailable(
    const FontDescription& font_description) {
  return FontCache::GetFontCache()->IsPlatformFontUniqueNameMatchAvailable(
      font_description, font_name_);
}

RefPtr<SimpleFontData> LocalFontFaceSource::CreateFontData(
    const FontDescription& font_description,
    const FontSelectionCapabilities&) {
  // FIXME(drott) crbug.com/627143: We still have the issue of matching
  // family name instead of postscript name for local fonts. However, we
  // should definitely not try to take into account the full requested
  // font description including the width, slope, weight styling when
  // trying to match against local fonts. An unstyled FontDescription
  // needs to be used here, or practically none at all. Instead we
  // should only look for the postscript or full font name.
  // However, when passing a style-neutral FontDescription we can't
  // match Roboto Bold and Thin anymore on Android given the CSS Google
  // Fonts sends, compare crbug.com/765980. So for now, we continue to
  // pass font_description to avoid breaking Google Fonts.
  FontDescription unstyled_description(font_description);
#if !defined(OS_ANDROID)
  unstyled_description.SetStretch(NormalWidthValue());
  unstyled_description.SetStyle(NormalSlopeValue());
  unstyled_description.SetWeight(NormalWeightValue());
#endif
  RefPtr<SimpleFontData> font_data = FontCache::GetFontCache()->GetFontData(
      unstyled_description, font_name_, AlternateFontName::kLocalUniqueFace);
  histograms_.Record(font_data.get());
  return font_data;
}

void LocalFontFaceSource::LocalFontHistograms::Record(bool load_success) {
  if (reported_)
    return;
  reported_ = true;
  DEFINE_STATIC_LOCAL(EnumerationHistogram, local_font_used_histogram,
                      ("WebFont.LocalFontUsed", 2));
  local_font_used_histogram.Count(load_success ? 1 : 0);
}

}  // namespace blink
