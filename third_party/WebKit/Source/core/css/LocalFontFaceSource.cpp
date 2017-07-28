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

PassRefPtr<SimpleFontData> LocalFontFaceSource::CreateFontData(
    const FontDescription& font_description) {
  // FIXME(drott) crbug.com/627143: We still have the issue of matching family
  // name instead of postscript name for local fonts. However, we should
  // definitely not try to take into account the full requested font description
  // including the width, slope, weight styling when trying to match against
  // local fonts.
  FontDescription description_without_styling(font_description);
  description_without_styling.SetStretch(NormalWidthValue());
  description_without_styling.SetStyle(NormalSlopeValue());
  description_without_styling.SetWeight(NormalWeightValue());
  RefPtr<SimpleFontData> font_data = FontCache::GetFontCache()->GetFontData(
      description_without_styling, font_name_,
      AlternateFontName::kLocalUniqueFace);
  histograms_.Record(font_data.Get());
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
