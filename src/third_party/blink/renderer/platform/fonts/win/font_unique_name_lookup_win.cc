// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/font_unique_name_lookup_win.h"

#include "base/files/file_path.h"
#include "mojo/public/mojom/base/shared_memory.mojom-blink.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace {

// These enum values correspond to the
// "Blink.Fonts.WindowsUniqueLocalFontInstantiationResult" histogram, new values
// can be added, but old values should never be reused.
enum WindowsUniqueLocalFontInstantiationResult {
  kSuccess = 0,
  kErrorOutsideWindowsFontsDirectory = 1,
  kErrorOther = 2,
  kMaxWindowsUniqueLocalFontInstantiationResult = 3
};

}  // namespace

namespace blink {

FontUniqueNameLookupWin::FontUniqueNameLookupWin() = default;

FontUniqueNameLookupWin::~FontUniqueNameLookupWin() = default;

sk_sp<SkTypeface> FontUniqueNameLookupWin::MatchUniqueName(
    const String& font_unique_name) {
  if (!EnsureMatchingServiceConnected<mojom::blink::DWriteFontProxyPtr>())
    return nullptr;

  base::Optional<FontTableMatcher::MatchResult> match_result =
      font_table_matcher_->MatchName(font_unique_name.Utf8().data());
  if (!match_result)
    return nullptr;
  // Record here when a locally uniquely matched font could not be
  // instantiated. One reason could be that the font was outside the
  // C:\Windows\Fonts directory and thus not accessible due to sandbox
  // restrictions.
  sk_sp<SkTypeface> local_typeface = SkTypeface::MakeFromFile(
      match_result->font_path.c_str(), match_result->ttc_index);

  WindowsUniqueLocalFontInstantiationResult result = kSuccess;

  // There is a chance that some systems have managed to register fonts into the
  // Windows system font collection outside the C:\Windows\Fonts directory. For
  // sandboxing reasons, we are unable to access them here. This histogram
  // serves to quantify how often this case occurs and whether we need and
  // additional sandbox helper to open the file handle on the browser process
  // side.
  if (!local_typeface) {
    base::FilePath windows_fonts_path(L"C:\\WINDOWS\\FONTS");
    if (!windows_fonts_path.IsParent(
            base::FilePath::FromUTF8Unsafe(match_result->font_path.c_str())))
      result = kErrorOutsideWindowsFontsDirectory;
    else
      result = kErrorOther;
  }

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, windows_unique_local_font_instantiation_histogram,
      ("Blink.Fonts.WindowsUniqueLocalFontInstantiationResult",
       kMaxWindowsUniqueLocalFontInstantiationResult));
  windows_unique_local_font_instantiation_histogram.Count(result);

  return local_typeface;
}

}  // namespace blink
