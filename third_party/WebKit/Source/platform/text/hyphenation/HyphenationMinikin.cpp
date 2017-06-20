// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/hyphenation/HyphenationMinikin.h"

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "platform/LayoutLocale.h"
#include "platform/text/hyphenation/HyphenatorAOSP.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/hyphenation/hyphenation.mojom-blink.h"

namespace blink {

using Hyphenator = android::Hyphenator;

static mojom::blink::HyphenationPtr ConnectToRemoteService() {
  mojom::blink::HyphenationPtr service;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&service));
  return service;
}

static const mojom::blink::HyphenationPtr& GetService() {
  DEFINE_STATIC_LOCAL(mojom::blink::HyphenationPtr, service,
                      (ConnectToRemoteService()));
  return service;
}

bool HyphenationMinikin::OpenDictionary(const AtomicString& locale) {
  const mojom::blink::HyphenationPtr& service = GetService();
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

  hyphenator_ = WTF::WrapUnique(Hyphenator::loadBinary(file_.data()));

  return true;
}

std::vector<uint8_t> HyphenationMinikin::Hyphenate(
    const StringView& text) const {
  std::vector<uint8_t> result;
  if (text.Is8Bit()) {
    String text16_bit = text.ToString();
    text16_bit.Ensure16Bit();
    hyphenator_->hyphenate(&result, text16_bit.Characters16(),
                           text16_bit.length());
  } else {
    hyphenator_->hyphenate(&result, text.Characters16(), text.length());
  }
  return result;
}

size_t HyphenationMinikin::LastHyphenLocation(const StringView& text,
                                              size_t before_index) const {
  if (text.length() < kMinimumPrefixLength + kMinimumSuffixLength ||
      before_index <= kMinimumPrefixLength)
    return 0;

  std::vector<uint8_t> result = Hyphenate(text);
  before_index =
      std::min<size_t>(before_index, text.length() - kMinimumSuffixLength);
  CHECK_LE(before_index, result.size());
  CHECK_GE(before_index, 1u);
  static_assert(kMinimumPrefixLength >= 1, "|beforeIndex - 1| can underflow");
  for (size_t i = before_index - 1; i >= kMinimumPrefixLength; i--) {
    if (result[i])
      return i;
  }
  return 0;
}

Vector<size_t, 8> HyphenationMinikin::HyphenLocations(
    const StringView& text) const {
  Vector<size_t, 8> hyphen_locations;
  if (text.length() < kMinimumPrefixLength + kMinimumSuffixLength)
    return hyphen_locations;

  std::vector<uint8_t> result = Hyphenate(text);
  static_assert(kMinimumPrefixLength >= 1,
                "Change the 'if' above if this fails");
  for (size_t i = text.length() - kMinimumSuffixLength - 1;
       i >= kMinimumPrefixLength; i--) {
    if (result[i])
      hyphen_locations.push_back(i);
  }
  return hyphen_locations;
}

using LocaleMap = HashMap<AtomicString, AtomicString, CaseFoldingHash>;

static LocaleMap CreateLocaleFallbackMap() {
  // This data is from CLDR, compiled by AOSP.
  // https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/text/Hyphenator.java
  using LocaleFallback = const char * [2];
  static LocaleFallback locale_fallback_data[] = {
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
      {"mn", "mn-cyrl"},    // Mongolian
      {"am", "und-ethi"},   // Amharic
      {"byn", "und-ethi"},  // Blin
      {"gez", "und-ethi"},  // Geʻez
      {"ti", "und-ethi"},   // Tigrinya
      {"wal", "und-ethi"},  // Wolaytta
  };
  LocaleMap map;
  for (const auto& it : locale_fallback_data)
    map.insert(it[0], it[1]);
  return map;
}

RefPtr<Hyphenation> Hyphenation::PlatformGetHyphenation(
    const AtomicString& locale) {
  RefPtr<HyphenationMinikin> hyphenation(AdoptRef(new HyphenationMinikin));
  if (hyphenation->OpenDictionary(locale.LowerASCII()))
    return hyphenation;
  hyphenation.Clear();

  DEFINE_STATIC_LOCAL(LocaleMap, locale_fallback, (CreateLocaleFallbackMap()));
  const auto& it = locale_fallback.find(locale);
  if (it != locale_fallback.end())
    return LayoutLocale::Get(it->value)->GetHyphenation();

  return nullptr;
}

RefPtr<HyphenationMinikin> HyphenationMinikin::FromFileForTesting(
    base::File file) {
  RefPtr<HyphenationMinikin> hyphenation(AdoptRef(new HyphenationMinikin));
  if (hyphenation->OpenDictionary(std::move(file)))
    return hyphenation;
  return nullptr;
}

}  // namespace blink
