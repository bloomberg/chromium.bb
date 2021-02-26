// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hyphenation/hyphenation_minikin.h"

#include <algorithm>
#include <utility>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/hyphenation/hyphenation.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/hyphenation/hyphenator_aosp.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"

namespace blink {

namespace {

template <typename CharType>
StringView SkipLeadingSpaces(const CharType* text,
                             unsigned length,
                             unsigned* num_leading_spaces_out) {
  const CharType* begin = text;
  const CharType* end = text + length;
  while (text != end && Character::TreatAsSpace(*text))
    text++;
  *num_leading_spaces_out = text - begin;
  return StringView(text, static_cast<unsigned>(end - text));
}

StringView SkipLeadingSpaces(const StringView& text,
                             unsigned* num_leading_spaces_out) {
  if (text.Is8Bit()) {
    return SkipLeadingSpaces(text.Characters8(), text.length(),
                             num_leading_spaces_out);
  }
  return SkipLeadingSpaces(text.Characters16(), text.length(),
                           num_leading_spaces_out);
}

}  // namespace

using Hyphenator = android::Hyphenator;

static mojo::Remote<mojom::blink::Hyphenation> ConnectToRemoteService() {
  mojo::Remote<mojom::blink::Hyphenation> service;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      service.BindNewPipeAndPassReceiver());
  return service;
}

static mojom::blink::Hyphenation* GetService() {
  DEFINE_STATIC_LOCAL(mojo::Remote<mojom::blink::Hyphenation>, service,
                      (ConnectToRemoteService()));
  return service.get();
}

bool HyphenationMinikin::OpenDictionary(const AtomicString& locale) {
  mojom::blink::Hyphenation* service = GetService();
  base::File file;
  base::ElapsedTimer timer;
  service->OpenDictionary(locale, &file);
  UMA_HISTOGRAM_TIMES("Hyphenation.Open", timer.Elapsed());

  return OpenDictionary(std::move(file));
}

bool HyphenationMinikin::OpenDictionary(base::File file) {
  if (!file.IsValid())
    return false;
  if (!file_.Initialize(std::move(file))) {
    DLOG(ERROR) << "mmap failed";
    return false;
  }

  hyphenator_ = base::WrapUnique(Hyphenator::loadBinary(file_.data()));

  return true;
}

Vector<uint8_t> HyphenationMinikin::Hyphenate(const StringView& text) const {
  Vector<uint8_t> result;
  if (text.Is8Bit()) {
    String text16_bit = text.ToString();
    text16_bit.Ensure16Bit();
    hyphenator_->hyphenate(
        &result, reinterpret_cast<const uint16_t*>(text16_bit.Characters16()),
        text16_bit.length());
  } else {
    hyphenator_->hyphenate(
        &result, reinterpret_cast<const uint16_t*>(text.Characters16()),
        text.length());
  }
  return result;
}

wtf_size_t HyphenationMinikin::LastHyphenLocation(
    const StringView& text,
    wtf_size_t before_index) const {
  unsigned num_leading_spaces;
  StringView word = SkipLeadingSpaces(text, &num_leading_spaces);
  if (before_index <= num_leading_spaces)
    return 0;
  before_index = std::min<wtf_size_t>(before_index - num_leading_spaces,
                                      word.length() - kMinimumSuffixLength);

  if (word.length() < kMinimumPrefixLength + kMinimumSuffixLength ||
      before_index <= kMinimumPrefixLength)
    return 0;

  Vector<uint8_t> result = Hyphenate(word);
  CHECK_LE(before_index, result.size());
  CHECK_GE(before_index, 1u);
  static_assert(kMinimumPrefixLength >= 1, "|beforeIndex - 1| can underflow");
  for (wtf_size_t i = before_index - 1; i >= kMinimumPrefixLength; i--) {
    if (result[i])
      return i + num_leading_spaces;
  }
  return 0;
}

Vector<wtf_size_t, 8> HyphenationMinikin::HyphenLocations(
    const StringView& text) const {
  unsigned num_leading_spaces;
  StringView word = SkipLeadingSpaces(text, &num_leading_spaces);

  Vector<wtf_size_t, 8> hyphen_locations;
  if (word.length() < kMinimumPrefixLength + kMinimumSuffixLength)
    return hyphen_locations;

  Vector<uint8_t> result = Hyphenate(word);
  static_assert(kMinimumPrefixLength >= 1,
                "Change the 'if' above if this fails");
  for (wtf_size_t i = word.length() - kMinimumSuffixLength - 1;
       i >= kMinimumPrefixLength; i--) {
    if (result[i])
      hyphen_locations.push_back(i + num_leading_spaces);
  }
  return hyphen_locations;
}

using LocaleMap = HashMap<AtomicString, AtomicString, CaseFoldingHash>;

static LocaleMap CreateLocaleFallbackMap() {
  // This data is from CLDR, compiled by AOSP.
  // https://android.googlesource.com/platform/frameworks/base/+/master/core/jni/android_text_Hyphenator.cpp
  using LocaleFallback = const char * [2];
  static LocaleFallback locale_fallback_data[] = {
      // English locales that fall back to en-US. The data is from CLDR. It's
      // all English locales,
      // minus the locales whose parent is en-001 (from supplementalData.xml,
      // under <parentLocales>).
      {"en-AS", "en-us"},  // English (American Samoa)
      {"en-GU", "en-us"},  // English (Guam)
      {"en-MH", "en-us"},  // English (Marshall Islands)
      {"en-MP", "en-us"},  // English (Northern Mariana Islands)
      {"en-PR", "en-us"},  // English (Puerto Rico)
      {"en-UM", "en-us"},  // English (United States Minor Outlying Islands)
      {"en-VI", "en-us"},  // English (Virgin Islands)
      // All English locales other than those falling back to en-US are mapped
      // to en-GB.
      {"en", "en-gb"},
      // For German, we're assuming the 1996 (and later) orthography by default.
      {"de", "de-1996"},
      // Liechtenstein uses the Swiss hyphenation rules for the 1901
      // orthography.
      {"de-LI-1901", "de-ch-1901"},
      // Norwegian is very probably Norwegian Bokmål.
      {"no", "nb"},
      // Use mn-Cyrl. According to CLDR's likelySubtags.xml, mn is most likely
      // to be mn-Cyrl.
      {"mn", "mn-cyrl"},  // Mongolian
      // Fall back to Ethiopic script for languages likely to be written in
      // Ethiopic.
      // Data is from CLDR's likelySubtags.xml.
      {"am", "und-ethi"},   // Amharic
      {"byn", "und-ethi"},  // Blin
      {"gez", "und-ethi"},  // Geʻez
      {"ti", "und-ethi"},   // Tigrinya
      {"wal", "und-ethi"},  // Wolaytta
      // Use Hindi as a fallback hyphenator for all languages written in
      // Devanagari, etc. This makes
      // sense because our Indic patterns are not really linguistic, but
      // script-based.
      {"und-Beng", "bn"},  // Bengali
      {"und-Deva", "hi"},  // Devanagari -> Hindi
      {"und-Gujr", "gu"},  // Gujarati
      {"und-Guru", "pa"},  // Gurmukhi -> Punjabi
      {"und-Knda", "kn"},  // Kannada
      {"und-Mlym", "ml"},  // Malayalam
      {"und-Orya", "or"},  // Oriya
      {"und-Taml", "ta"},  // Tamil
      {"und-Telu", "te"},  // Telugu

      // List of locales with hyphens not to fall back.
      {"de-1901", nullptr},
      {"de-1996", nullptr},
      {"de-ch-1901", nullptr},
      {"en-gb", nullptr},
      {"en-us", nullptr},
      {"mn-cyrl", nullptr},
      {"und-ethi", nullptr},
  };
  LocaleMap map;
  for (const auto& it : locale_fallback_data)
    map.insert(AtomicString(it[0]), it[1]);
  return map;
}

// static
AtomicString HyphenationMinikin::MapLocale(const AtomicString& locale) {
  DEFINE_STATIC_LOCAL(LocaleMap, locale_fallback, (CreateLocaleFallbackMap()));
  for (AtomicString mapped_locale = locale;;) {
    const auto& it = locale_fallback.find(mapped_locale);
    if (it != locale_fallback.end()) {
      if (it->value)
        return it->value;
      return mapped_locale;
    }
    const wtf_size_t last_hyphen = mapped_locale.ReverseFind('-');
    if (last_hyphen == kNotFound || !last_hyphen)
      return mapped_locale;
    mapped_locale = AtomicString(mapped_locale.GetString().Left(last_hyphen));
  }
}

scoped_refptr<Hyphenation> Hyphenation::PlatformGetHyphenation(
    const AtomicString& locale) {
  const AtomicString mapped_locale = HyphenationMinikin::MapLocale(locale);
  if (mapped_locale.Impl() != locale.Impl())
    return LayoutLocale::Get(mapped_locale)->GetHyphenation();

  scoped_refptr<HyphenationMinikin> hyphenation(
      base::AdoptRef(new HyphenationMinikin));
  if (hyphenation->OpenDictionary(locale.LowerASCII()))
    return hyphenation;

  return nullptr;
}

scoped_refptr<HyphenationMinikin> HyphenationMinikin::FromFileForTesting(
    base::File file) {
  scoped_refptr<HyphenationMinikin> hyphenation(
      base::AdoptRef(new HyphenationMinikin));
  if (hyphenation->OpenDictionary(std::move(file)))
    return hyphenation;
  return nullptr;
}

}  // namespace blink
