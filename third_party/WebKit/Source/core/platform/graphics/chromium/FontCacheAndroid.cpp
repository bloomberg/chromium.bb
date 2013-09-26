/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/graphics/FontCache.h"

#include "core/platform/graphics/Font.h"
#include "core/platform/graphics/FontDescription.h"
#include "core/platform/graphics/SimpleFontData.h"

#include "SkTypeface_android.h"

namespace WebCore {

static AtomicString getFamilyNameForCharacter(UChar32 c, UScriptCode script)
{
    // This is a hack to use the preferred font for CJK scripts.
    // FIXME: Use new Skia API once Android system supports per-family and per-script fallback fonts.
    const char* locale;
    switch (script) {
    case USCRIPT_SIMPLIFIED_HAN:
        locale = "zh-CN";
        break;
    case USCRIPT_TRADITIONAL_HAN:
        locale = "zh-TW";
        break;
    case USCRIPT_KATAKANA_OR_HIRAGANA:
        locale = "ja";
        break;
    case USCRIPT_HANGUL:
        locale = "ko";
        break;
    default:
        locale = 0;
        break;
    }

    SkString skiaFamilyName;
    if (!SkGetFallbackFamilyNameForChar(c, locale, &skiaFamilyName) || skiaFamilyName.isEmpty())
        return AtomicString();
    return skiaFamilyName.c_str();
}

PassRefPtr<SimpleFontData> FontCache::getFontDataForCharacter(const Font& font, UChar32 c)
{
    AtomicString familyName = getFamilyNameForCharacter(c, font.fontDescription().script());
    if (familyName.isEmpty())
        return 0;
    return getFontResourceData(getFontResourcePlatformData(font.fontDescription(), familyName, DoNotRetain), DoNotRetain);
}

// static
AtomicString FontCache::getGenericFamilyNameForScript(const AtomicString& familyName, UScriptCode script)
{
    // This is a hack to use the preferred font for CJK scripts.
    // FIXME: Use new Skia API once Android system supports per-family and per-script fallback fonts.
    UChar32 examplerChar;
    switch (script) {
    case USCRIPT_SIMPLIFIED_HAN:
    case USCRIPT_TRADITIONAL_HAN:
    case USCRIPT_KATAKANA_OR_HIRAGANA:
        examplerChar = 0x4E00; // A common character in Japanese and Chinese.
        break;
    case USCRIPT_HANGUL:
        examplerChar = 0xAC00;
        break;
    default:
        // For other scripts, use the default generic family mapping logic.
        return familyName;
    }

    return getFamilyNameForCharacter(examplerChar, script);
}

} // namespace WebCore
