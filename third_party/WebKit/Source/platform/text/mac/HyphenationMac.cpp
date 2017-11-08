// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include <CoreFoundation/CoreFoundation.h>
#include "platform/wtf/RetainPtr.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

class HyphenationCF final : public Hyphenation {
 public:
  HyphenationCF(RetainPtr<CFLocaleRef>& locale_cf) : locale_cf_(locale_cf) {
    DCHECK(locale_cf_);
  }

  size_t LastHyphenLocation(const StringView& text,
                            size_t before_index) const override {
    CFIndex result = CFStringGetHyphenationLocationBeforeIndex(
        text.ToString().Impl()->CreateCFString().Get(), before_index,
        CFRangeMake(0, text.length()), 0, locale_cf_.Get(), 0);
    return result == kCFNotFound ? 0 : result;
  }

  // While Hyphenation::FirstHyphenLocation() works good, it computes all
  // locations and discards ones after |after_index|.
  // This version minimizes the computation for platforms that supports
  // LastHyphenLocation() but does not support HyphenLocations().
  size_t FirstHyphenLocation(const StringView& text, size_t after_index) const {
    after_index =
        std::max(after_index, static_cast<size_t>(kMinimumPrefixLength - 1));
    size_t hyphen_location = text.length();
    if (hyphen_location <= kMinimumSuffixLength)
      return 0;
    size_t max_hyphen_location = hyphen_location - kMinimumSuffixLength;
    hyphen_location = max_hyphen_location;
    for (;;) {
      size_t previous = LastHyphenLocation(text, hyphen_location);
      if (previous <= after_index)
        break;
      hyphen_location = previous;
    }
    return hyphen_location >= max_hyphen_location ? 0 : hyphen_location;
  }

 private:
  RetainPtr<CFLocaleRef> locale_cf_;
};

scoped_refptr<Hyphenation> Hyphenation::PlatformGetHyphenation(
    const AtomicString& locale) {
  RetainPtr<CFStringRef> locale_cf_string = locale.Impl()->CreateCFString();
  RetainPtr<CFLocaleRef> locale_cf =
      AdoptCF(CFLocaleCreate(kCFAllocatorDefault, locale_cf_string.Get()));
  if (!CFStringIsHyphenationAvailableForLocale(locale_cf.Get()))
    return nullptr;
  return base::AdoptRef(new HyphenationCF(locale_cf));
}

}  // namespace blink
