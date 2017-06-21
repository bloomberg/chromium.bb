// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include <dlfcn.h>
#import <Foundation/Foundation.h>
#include <string>
#include <vector>

#include "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#import "base/strings/sys_string_conversions.h"
#include "ui/gfx/font.h"

// TODO(thakis): Remove this prototype once the deployment target is 10.8+.
extern "C" CFArrayRef CTFontCopyDefaultCascadeListForLanguages(
    CTFontRef font,
    CFArrayRef languagePrefList);

namespace gfx {

std::vector<Font> GetFallbackFonts(const Font& font) {
  // On Mac "There is a system default cascade list (which is polymorphic, based
  // on the user's language setting and current font)" - CoreText Programming
  // Guide.
  // The CoreText APIs provide CTFontCreateForString(font, string, range), but
  // it requires a text string "hint", and the returned font can't be
  // represented by name for easy retrieval later.
  // In 10.8, CTFontCopyDefaultCascadeListForLanguages(font, language_list)
  // showed up which is a good fit GetFallbackFonts().
  NSArray* languages = [[NSUserDefaults standardUserDefaults]
      stringArrayForKey:@"AppleLanguages"];
  CFArrayRef languages_cf = base::mac::NSToCFCast(languages);
  base::ScopedCFTypeRef<CFArrayRef> cascade_list(
      CTFontCopyDefaultCascadeListForLanguages(
          static_cast<CTFontRef>(font.GetNativeFont()), languages_cf));

  std::vector<Font> fallback_fonts;

  const CFIndex fallback_count = CFArrayGetCount(cascade_list);
  for (CFIndex i = 0; i < fallback_count; ++i) {
    CTFontDescriptorRef descriptor =
        base::mac::CFCastStrict<CTFontDescriptorRef>(
            CFArrayGetValueAtIndex(cascade_list, i));
    base::ScopedCFTypeRef<CTFontRef> font(
        CTFontCreateWithFontDescriptor(descriptor, 0.0, nullptr));
    if (font.get())
      fallback_fonts.push_back(Font(static_cast<NSFont*>(font.get())));
  }

  if (fallback_fonts.empty())
    return std::vector<Font>(1, font);

  return fallback_fonts;
}

}  // namespace gfx
