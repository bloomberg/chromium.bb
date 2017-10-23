/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/FontCache.h"

#include "platform/fonts/FontPlatformData.h"

namespace blink {

// static
const AtomicString& FontCache::SystemFontFamily() {
  // TODO(fuchsia): Implement this when UI support is ready. crbug.com/750946
  NOTIMPLEMENTED();

  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, font_family, ());
  return font_family;
}

void FontCache::GetFontForCharacter(
    UChar32 c,
    const char* preferred_locale,
    FontCache::PlatformFallbackFont* fallback_font) {
  // TODO(fuchsia): Implement this when UI support is ready. crbug.com/750946
  NOTIMPLEMENTED();
}

scoped_refptr<SimpleFontData> FontCache::FallbackFontForCharacter(
    const FontDescription&,
    UChar32,
    const SimpleFontData* font_data_to_substitute,
    FontFallbackPriority priority) {
  // TODO(fuchsia): Implement this when UI support is ready. crbug.com/750946
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace blink
