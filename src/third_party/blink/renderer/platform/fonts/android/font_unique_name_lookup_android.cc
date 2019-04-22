// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/android/font_unique_name_lookup_android.h"
#include "mojo/public/mojom/base/shared_memory.mojom-blink.h"
#include "third_party/blink/public/mojom/font_unique_name_lookup/font_unique_name_lookup.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

FontUniqueNameLookupAndroid::~FontUniqueNameLookupAndroid() = default;

sk_sp<SkTypeface> FontUniqueNameLookupAndroid::MatchUniqueName(
    const String& font_unique_name) {
  if (!EnsureMatchingServiceConnected())
    return nullptr;
  base::Optional<FontTableMatcher::MatchResult> match_result =
      font_table_matcher_->MatchName(font_unique_name.Utf8().data());
  if (!match_result)
    return nullptr;
  return SkTypeface::MakeFromFile(match_result->font_path.c_str(),
                                  match_result->ttc_index);
}

bool FontUniqueNameLookupAndroid::EnsureMatchingServiceConnected() {
  if (font_table_matcher_)
    return true;

  mojom::blink::FontUniqueNameLookupPtr service;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&service));

  base::ReadOnlySharedMemoryRegion region_ptr;
  if (!service->GetUniqueNameLookupTable(&region_ptr)) {
    // Tests like StyleEngineTest do not set up a full browser where Blink can
    // connect to a browser side service for font lookups. Placing a DCHECK here
    // is too strict for such a case.
    LOG(ERROR) << "Unable to connect to browser side service for src: local() "
                  "font lookup.";
    return false;
  }

  font_table_matcher_ = std::make_unique<FontTableMatcher>(region_ptr.Map());
  return true;
}

}  // namespace blink
