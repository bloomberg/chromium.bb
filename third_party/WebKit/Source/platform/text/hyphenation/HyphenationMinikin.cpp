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

static mojom::blink::HyphenationPtr connectToRemoteService() {
  mojom::blink::HyphenationPtr service;
  Platform::current()->interfaceProvider()->getInterface(
      mojo::MakeRequest(&service));
  return service;
}

static const mojom::blink::HyphenationPtr& getService() {
  DEFINE_STATIC_LOCAL(mojom::blink::HyphenationPtr, service,
                      (connectToRemoteService()));
  return service;
}

bool HyphenationMinikin::openDictionary(const AtomicString& locale) {
  const mojom::blink::HyphenationPtr& service = getService();
  base::File file;
  base::ElapsedTimer timer;
  service->OpenDictionary(locale, &file);
  UMA_HISTOGRAM_TIMES("Hyphenation.Open", timer.Elapsed());

  return openDictionary(std::move(file));
}

bool HyphenationMinikin::openDictionary(base::File file) {
  if (!file.IsValid())
    return false;
  if (!m_file.Initialize(std::move(file))) {
    DLOG(ERROR) << "mmap failed";
    return false;
  }

  m_hyphenator = WTF::wrapUnique(Hyphenator::loadBinary(m_file.data()));

  return true;
}

std::vector<uint8_t> HyphenationMinikin::hyphenate(
    const StringView& text) const {
  std::vector<uint8_t> result;
  if (text.is8Bit()) {
    String text16Bit = text.toString();
    text16Bit.ensure16Bit();
    m_hyphenator->hyphenate(&result, text16Bit.characters16(),
                            text16Bit.length());
  } else {
    m_hyphenator->hyphenate(&result, text.characters16(), text.length());
  }
  return result;
}

size_t HyphenationMinikin::lastHyphenLocation(const StringView& text,
                                              size_t beforeIndex) const {
  if (text.length() < minimumPrefixLength + minimumSuffixLength ||
      beforeIndex <= minimumPrefixLength)
    return 0;

  std::vector<uint8_t> result = hyphenate(text);
  beforeIndex =
      std::min<size_t>(beforeIndex, text.length() - minimumSuffixLength);
  CHECK_LE(beforeIndex, result.size());
  CHECK_GE(beforeIndex, 1u);
  static_assert(minimumPrefixLength >= 1, "|beforeIndex - 1| can underflow");
  for (size_t i = beforeIndex - 1; i >= minimumPrefixLength; i--) {
    if (result[i])
      return i;
  }
  return 0;
}

Vector<size_t, 8> HyphenationMinikin::hyphenLocations(
    const StringView& text) const {
  Vector<size_t, 8> hyphenLocations;
  if (text.length() < minimumPrefixLength + minimumSuffixLength)
    return hyphenLocations;

  std::vector<uint8_t> result = hyphenate(text);
  static_assert(minimumPrefixLength >= 1,
                "Change the 'if' above if this fails");
  for (size_t i = text.length() - minimumSuffixLength - 1;
       i >= minimumPrefixLength; i--) {
    if (result[i])
      hyphenLocations.push_back(i);
  }
  return hyphenLocations;
}

using LocaleMap = HashMap<AtomicString, AtomicString, CaseFoldingHash>;

static LocaleMap createLocaleFallbackMap() {
  // This data is from CLDR, compiled by AOSP.
  // https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/text/Hyphenator.java
  using LocaleFallback = const char * [2];
  static LocaleFallback localeFallbackData[] = {
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
  for (const auto& it : localeFallbackData)
    map.insert(it[0], it[1]);
  return map;
}

PassRefPtr<Hyphenation> Hyphenation::platformGetHyphenation(
    const AtomicString& locale) {
  RefPtr<HyphenationMinikin> hyphenation(adoptRef(new HyphenationMinikin));
  if (hyphenation->openDictionary(locale.lowerASCII()))
    return hyphenation.release();
  hyphenation.clear();

  DEFINE_STATIC_LOCAL(LocaleMap, localeFallback, (createLocaleFallbackMap()));
  const auto& it = localeFallback.find(locale);
  if (it != localeFallback.end())
    return LayoutLocale::get(it->value)->getHyphenation();

  return nullptr;
}

PassRefPtr<HyphenationMinikin> HyphenationMinikin::fromFileForTesting(
    base::File file) {
  RefPtr<HyphenationMinikin> hyphenation(adoptRef(new HyphenationMinikin));
  if (hyphenation->openDictionary(std::move(file)))
    return hyphenation.release();
  return nullptr;
}

}  // namespace blink
