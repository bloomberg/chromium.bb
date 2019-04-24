// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_UNIQUE_NAME_LOOKUP_WIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_UNIQUE_NAME_LOOKUP_WIN_H_

#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"

namespace blink {

class FontUniqueNameLookupWin : public FontUniqueNameLookup {
 public:
  FontUniqueNameLookupWin();
  ~FontUniqueNameLookupWin() override;
  sk_sp<SkTypeface> MatchUniqueName(const String& font_unique_name) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FontUniqueNameLookupWin);
};

}  // namespace blink

#endif
