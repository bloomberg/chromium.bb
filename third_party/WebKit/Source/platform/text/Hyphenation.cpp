// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/Hyphenation.h"

#include "wtf/text/StringView.h"

namespace blink {

Hyphenation::HyphenationMap& Hyphenation::getHyphenationMap()
{
    DEFINE_STATIC_LOCAL(HyphenationMap, hyphenationMap, ());
    return hyphenationMap;
}

Hyphenation* Hyphenation::get(const AtomicString& locale)
{
    Hyphenation::HyphenationMap& hyphenationMap = getHyphenationMap();
    auto result = hyphenationMap.add(locale, nullptr);
    if (result.isNewEntry)
        result.storedValue->value = platformGetHyphenation(locale);
    return result.storedValue->value.get();
}

void Hyphenation::setForTesting(const AtomicString& locale, PassRefPtr<Hyphenation> hyphenation)
{
    getHyphenationMap().set(locale, hyphenation);
}

void Hyphenation::clearForTesting()
{
    getHyphenationMap().clear();
}

} // namespace blink
