// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include <CoreFoundation/CoreFoundation.h>
#include "platform/wtf/RetainPtr.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

class HyphenationCF : public Hyphenation {
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

 private:
  RetainPtr<CFLocaleRef> locale_cf_;
};

RefPtr<Hyphenation> Hyphenation::PlatformGetHyphenation(
    const AtomicString& locale) {
  RetainPtr<CFStringRef> locale_cf_string = locale.Impl()->CreateCFString();
  RetainPtr<CFLocaleRef> locale_cf =
      AdoptCF(CFLocaleCreate(kCFAllocatorDefault, locale_cf_string.Get()));
  if (!CFStringIsHyphenationAvailableForLocale(locale_cf.Get()))
    return nullptr;
  return AdoptRef(new HyphenationCF(locale_cf));
}

}  // namespace blink
