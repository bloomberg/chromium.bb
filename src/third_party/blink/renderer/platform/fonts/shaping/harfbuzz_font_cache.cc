// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_font_data.h"
#include "third_party/harfbuzz-ng/utils/hb_scoped.h"

namespace blink {

HarfBuzzFontCache::HarfBuzzFontCache() = default;
HarfBuzzFontCache::~HarfBuzzFontCache() = default;

// See "harfbuzz_face.cc" for |HarfBuzzFontCache::GetOrCreateFontData()|
// implementation.

}  // namespace blink
