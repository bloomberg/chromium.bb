// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "third_party/blink/renderer/platform/fonts/android/font_unique_name_lookup_android.h"
#endif

namespace blink {

FontUniqueNameLookup::FontUniqueNameLookup() = default;

// static
std::unique_ptr<FontUniqueNameLookup>
FontUniqueNameLookup::GetPlatformUniqueNameLookup() {
#if defined(OS_ANDROID)
  return std::make_unique<FontUniqueNameLookupAndroid>();
#else
  NOTREACHED();
  return nullptr;
#endif
}

}  // namespace blink
