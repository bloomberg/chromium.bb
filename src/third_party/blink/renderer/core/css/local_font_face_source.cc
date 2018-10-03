// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/local_font_face_source.h"

#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/histogram.h"

namespace blink {

static void AdjustedFontDescriptionForBoldItalic(FontDescription& fontDescription,
                                                 WTF::String& fontName)
{
    if (fontName.EndsWith(" Italic")) {
        fontDescription.SetStyle(ItalicSlopeValue());
        fontName = fontName.Substring(0, fontName.length() - 7);
    }
    if (fontName.EndsWith(" Bold")) {
        fontDescription.SetWeight(BoldWeightValue());
        fontName = fontName.Substring(0, fontName.length() - 5);
    }
}

LocalFontFaceSource::LocalFontFaceSource(const String& fontName)
    : font_name_(fontName)
    , need_to_adjust_for_bold_italic_(fontName.EndsWith(" Bold") ||
                                      fontName.EndsWith(" Italic"))
{
}

bool LocalFontFaceSource::IsLocalFontAvailable(
    const FontDescription& font_description) {
  if (need_to_adjust_for_bold_italic_) {
    FontDescription adjustedFontDescription = font_description;
    WTF::String adjustedFontName = font_name_.GetString();
    AdjustedFontDescriptionForBoldItalic(adjustedFontDescription, adjustedFontName);
    return FontCache::GetFontCache()->IsPlatformFontUniqueNameMatchAvailable(
        adjustedFontDescription, WTF::AtomicString(adjustedFontName));
  }

  return FontCache::GetFontCache()->IsPlatformFontUniqueNameMatchAvailable(
      font_description, font_name_);
}

scoped_refptr<SimpleFontData> LocalFontFaceSource::CreateFontData(
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
  scoped_refptr<SimpleFontData> font_data;
  if (need_to_adjust_for_bold_italic_) {
      FontDescription adjustedFontDescription = font_description;
      WTF::String adjustedFontName = font_name_.GetString();
      AdjustedFontDescriptionForBoldItalic(adjustedFontDescription, adjustedFontName);

      font_data = FontCache::GetFontCache()->GetFontData(
          adjustedFontDescription,
          WTF::AtomicString(adjustedFontName),
          AlternateFontName::kLocalUniqueFace);
  }
  else {
      font_data = FontCache::GetFontCache()->GetFontData(
          unstyled_description, font_name_,  AlternateFontName::kLocalUniqueFace);
  }


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
