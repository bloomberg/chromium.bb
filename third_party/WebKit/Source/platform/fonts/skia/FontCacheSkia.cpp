
/*
 * Copyright (c) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include <unicode/locid.h>

#include <memory>

#include "SkFontMgr.h"
#include "SkStream.h"
#include "SkTypeface.h"

#include "build/build_config.h"
#include "platform/Language.h"
#include "platform/fonts/AlternateFontFamily.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFaceCreationParams.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/CString.h"
#include "public/platform/Platform.h"
#include "public/platform/linux/WebSandboxSupport.h"

#if !defined(OS_WIN) && !defined(OS_ANDROID)
#include "SkFontConfigInterface.h"

static sk_sp<SkTypeface> typefaceForFontconfigInterfaceIdAndTtcIndex(
    int fontconfigInterfaceId,
    int ttcIndex) {
  sk_sp<SkFontConfigInterface> fci(SkFontConfigInterface::RefGlobal());
  SkFontConfigInterface::FontIdentity fontIdentity;
  fontIdentity.fID = fontconfigInterfaceId;
  fontIdentity.fTTCIndex = ttcIndex;
  return fci->makeTypeface(fontIdentity);
}
#endif

namespace blink {

AtomicString ToAtomicString(const SkString& str) {
  return AtomicString::FromUTF8(str.c_str(), str.size());
}

#if defined(OS_ANDROID) || defined(OS_LINUX)
// Android special locale for retrieving the color emoji font
// based on the proposed changes in UTR #51 for introducing
// an Emoji script code:
// http://www.unicode.org/reports/tr51/proposed.html#Emoji_Script
static const char kAndroidColorEmojiLocale[] = "und-Zsye";

// This function is called on android or when we are emulating android fonts on
// linux and the embedder has overriden the default fontManager with
// WebFontRendering::setSkiaFontMgr.
// static
AtomicString FontCache::GetFamilyNameForCharacter(
    SkFontMgr* fm,
    UChar32 c,
    const FontDescription& font_description,
    FontFallbackPriority fallback_priority) {
  DCHECK(fm);

  const size_t kMaxLocales = 4;
  const char* bcp47_locales[kMaxLocales];
  size_t locale_count = 0;

  // Fill in the list of locales in the reverse priority order.
  // Skia expects the highest array index to be the first priority.
  const LayoutLocale* content_locale = font_description.Locale();
  if (const LayoutLocale* han_locale =
          LayoutLocale::LocaleForHan(content_locale))
    bcp47_locales[locale_count++] = han_locale->LocaleForHanForSkFontMgr();
  bcp47_locales[locale_count++] =
      LayoutLocale::GetDefault().LocaleForSkFontMgr();
  if (content_locale)
    bcp47_locales[locale_count++] = content_locale->LocaleForSkFontMgr();
  if (fallback_priority == FontFallbackPriority::kEmojiEmoji)
    bcp47_locales[locale_count++] = kAndroidColorEmojiLocale;
  SECURITY_DCHECK(locale_count <= kMaxLocales);
  sk_sp<SkTypeface> typeface(fm->matchFamilyStyleCharacter(
      0, SkFontStyle(), bcp47_locales, locale_count, c));
  if (!typeface)
    return g_empty_atom;

  SkString skia_family_name;
  typeface->getFamilyName(&skia_family_name);
  return ToAtomicString(skia_family_name);
}
#endif

void FontCache::PlatformInit() {}

PassRefPtr<SimpleFontData> FontCache::FallbackOnStandardFontStyle(
    const FontDescription& font_description,
    UChar32 character) {
  FontDescription substitute_description(font_description);
  substitute_description.SetStyle(kFontStyleNormal);
  substitute_description.SetWeight(kFontWeightNormal);

  FontFaceCreationParams creation_params(
      substitute_description.Family().Family());
  FontPlatformData* substitute_platform_data =
      GetFontPlatformData(substitute_description, creation_params);
  if (substitute_platform_data &&
      substitute_platform_data->FontContainsCharacter(character)) {
    FontPlatformData platform_data =
        FontPlatformData(*substitute_platform_data);
    platform_data.SetSyntheticBold(font_description.Weight() >= kFontWeight600);
    platform_data.SetSyntheticItalic(
        font_description.Style() == kFontStyleItalic ||
        font_description.Style() == kFontStyleOblique);
    return FontDataFromFontPlatformData(&platform_data, kDoNotRetain);
  }

  return nullptr;
}

PassRefPtr<SimpleFontData> FontCache::GetLastResortFallbackFont(
    const FontDescription& description,
    ShouldRetain should_retain) {
  const FontFaceCreationParams fallback_creation_params(
      GetFallbackFontFamily(description));
  const FontPlatformData* font_platform_data = GetFontPlatformData(
      description, fallback_creation_params, AlternateFontName::kLastResort);

  // We should at least have Sans or Arial which is the last resort fallback of
  // SkFontHost ports.
  if (!font_platform_data) {
    DEFINE_STATIC_LOCAL(const FontFaceCreationParams, sans_creation_params,
                        (AtomicString("Sans")));
    font_platform_data = GetFontPlatformData(description, sans_creation_params,
                                             AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_STATIC_LOCAL(const FontFaceCreationParams, arial_creation_params,
                        (AtomicString("Arial")));
    font_platform_data = GetFontPlatformData(description, arial_creation_params,
                                             AlternateFontName::kLastResort);
  }
#if defined(OS_WIN)
  // Try some more Windows-specific fallbacks.
  if (!font_platform_data) {
    DEFINE_STATIC_LOCAL(const FontFaceCreationParams,
                        msuigothic_creation_params,
                        (AtomicString("MS UI Gothic")));
    font_platform_data =
        GetFontPlatformData(description, msuigothic_creation_params,
                            AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_STATIC_LOCAL(const FontFaceCreationParams,
                        mssansserif_creation_params,
                        (AtomicString("Microsoft Sans Serif")));
    font_platform_data =
        GetFontPlatformData(description, mssansserif_creation_params,
                            AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_STATIC_LOCAL(const FontFaceCreationParams, segoeui_creation_params,
                        (AtomicString("Segoe UI")));
    font_platform_data = GetFontPlatformData(
        description, segoeui_creation_params, AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_STATIC_LOCAL(const FontFaceCreationParams, calibri_creation_params,
                        (AtomicString("Calibri")));
    font_platform_data = GetFontPlatformData(
        description, calibri_creation_params, AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_STATIC_LOCAL(const FontFaceCreationParams,
                        timesnewroman_creation_params,
                        (AtomicString("Times New Roman")));
    font_platform_data =
        GetFontPlatformData(description, timesnewroman_creation_params,
                            AlternateFontName::kLastResort);
  }
  if (!font_platform_data) {
    DEFINE_STATIC_LOCAL(const FontFaceCreationParams,
                        couriernew_creation_params,
                        (AtomicString("Courier New")));
    font_platform_data =
        GetFontPlatformData(description, couriernew_creation_params,
                            AlternateFontName::kLastResort);
  }
#endif

  DCHECK(font_platform_data);
  return FontDataFromFontPlatformData(font_platform_data, should_retain);
}

sk_sp<SkTypeface> FontCache::CreateTypeface(
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    CString& name) {
#if !defined(OS_WIN) && !defined(OS_ANDROID)
  if (creation_params.CreationType() == kCreateFontByFciIdAndTtcIndex) {
    if (Platform::Current()->GetSandboxSupport())
      return typefaceForFontconfigInterfaceIdAndTtcIndex(
          creation_params.FontconfigInterfaceId(), creation_params.TtcIndex());
    return SkTypeface::MakeFromFile(creation_params.Filename().data(),
                                    creation_params.TtcIndex());
  }
#endif

  AtomicString family = creation_params.Family();
  DCHECK_NE(family, FontFamilyNames::system_ui);
  // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the
  // name into the fallback name (like "monospace") that fontconfig understands.
  if (!family.length() || family.StartsWith("-webkit-")) {
    name = GetFallbackFontFamily(font_description).GetString().Utf8();
  } else {
    // convert the name to utf8
    name = family.Utf8();
  }

#if defined(OS_WIN)
  if (sideloaded_fonts_) {
    HashMap<String, sk_sp<SkTypeface>>::iterator sideloaded_font =
        sideloaded_fonts_->find(name.data());
    if (sideloaded_font != sideloaded_fonts_->end())
      return sideloaded_font->value;
  }
#endif

#if defined(OS_LINUX) || defined(OS_WIN)
  // On linux if the fontManager has been overridden then we should be calling
  // the embedder provided font Manager rather than calling
  // SkTypeface::CreateFromName which may redirect the call to the default font
  // Manager.  On Windows the font manager is always present.
  if (font_manager_)
    return sk_sp<SkTypeface>(font_manager_->matchFamilyStyle(
        name.data(), font_description.SkiaFontStyle()));
#endif

  // FIXME: Use m_fontManager, matchFamilyStyle instead of
  // legacyCreateTypeface on all platforms.
  sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
  return sk_sp<SkTypeface>(
      fm->legacyCreateTypeface(name.data(), font_description.SkiaFontStyle()));
}

#if !defined(OS_WIN)
std::unique_ptr<FontPlatformData> FontCache::CreateFontPlatformData(
    const FontDescription& font_description,
    const FontFaceCreationParams& creation_params,
    float font_size,
    AlternateFontName) {
  CString name;
  sk_sp<SkTypeface> tf =
      CreateTypeface(font_description, creation_params, name);
  if (!tf)
    return nullptr;

  return WTF::WrapUnique(
      new FontPlatformData(tf, name.data(), font_size,
                           (NumericFontWeight(font_description.Weight()) >
                            200 + tf->fontStyle().weight()) ||
                               font_description.IsSyntheticBold(),
                           ((font_description.Style() == kFontStyleItalic ||
                             font_description.Style() == kFontStyleOblique) &&
                            !tf->isItalic()) ||
                               font_description.IsSyntheticItalic(),
                           font_description.Orientation()));
}
#endif  // !defined(OS_WIN)

}  // namespace blink
