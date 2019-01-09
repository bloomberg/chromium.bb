// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/font_unique_name_lookup_win.h"

#include "base/files/file_path.h"
#include "mojo/public/mojom/base/shared_memory.mojom-blink.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

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
  // TODO(drott): Add a UMA histogram for when this fails and recording whether
  // the font file path was outside of the Windows Fonts directory.
  return SkTypeface::MakeFromFile(match_result->font_path.c_str(),
                                  match_result->ttc_index);
}

}  // namespace blink
