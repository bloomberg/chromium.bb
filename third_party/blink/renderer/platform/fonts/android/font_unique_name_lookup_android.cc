// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/android/font_unique_name_lookup_android.h"
#include "mojo/public/mojom/base/shared_memory.mojom-blink.h"
#include "third_party/blink/public/platform/modules/font_unique_name_lookup/font_unique_name_lookup.mojom-blink.h"

namespace blink {

FontUniqueNameLookupAndroid::~FontUniqueNameLookupAndroid() = default;

sk_sp<SkTypeface> FontUniqueNameLookupAndroid::MatchUniqueName(
    const String& font_unique_name) {
  if (!EnsureMatchingServiceConnected<mojom::blink::FontUniqueNameLookupPtr>())
    return nullptr;
  base::Optional<FontTableMatcher::MatchResult> match_result =
      font_table_matcher_->MatchName(font_unique_name.Utf8().data());
  if (!match_result)
    return nullptr;
  return SkTypeface::MakeFromFile(match_result->font_path.c_str(),
                                  match_result->ttc_index);
}

}  // namespace blink
