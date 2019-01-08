// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_UNIQUE_NAME_LOOKUP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_UNIQUE_NAME_LOOKUP_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if defined(OS_ANDROID) || defined(OS_WIN)
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#endif

#include <SkRefCnt.h>
#include <SkTypeface.h>
#include <memory>

namespace blink {

class FontTableMatcher;

class FontUniqueNameLookup {
 public:
  // Factory function to construct a platform specific font unique name lookup
  // instance. Client must not use this directly as it is thread
  // specific. Retrieve it from FontGlobalContext instead.
  static std::unique_ptr<FontUniqueNameLookup> GetPlatformUniqueNameLookup();

  virtual sk_sp<SkTypeface> MatchUniqueName(const String& font_unique_name) = 0;

  virtual ~FontUniqueNameLookup() = default;

 protected:
  FontUniqueNameLookup();

  // Windows and Android share the concept of connecting to a Mojo service for
  // retrieving a ReadOnlySharedMemoryRegion with the lookup table in it.
#if defined(OS_WIN) || defined(OS_ANDROID)
  template <class ServicePtrType>
  bool EnsureMatchingServiceConnected();
  std::unique_ptr<FontTableMatcher> font_table_matcher_;
#endif

  DISALLOW_COPY_AND_ASSIGN(FontUniqueNameLookup);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_UNIQUE_NAME_LOOKUP_
