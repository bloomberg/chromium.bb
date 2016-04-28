// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Hyphenation_h
#define Hyphenation_h

#include "platform/PlatformExport.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringHash.h"

namespace blink {

class StringView;

class PLATFORM_EXPORT Hyphenation : public RefCounted<Hyphenation> {
public:
    virtual ~Hyphenation() {}

    static Hyphenation* get(const AtomicString& locale);

    virtual size_t lastHyphenLocation(const StringView&, size_t beforeIndex, const AtomicString& locale) const = 0;

    static void setForTesting(const AtomicString& locale, PassRefPtr<Hyphenation>);
    static void clearForTesting();

private:
    using HyphenationMap = HashMap<AtomicString, RefPtr<Hyphenation>, CaseFoldingHash>;

    static HyphenationMap& getHyphenationMap();
    static PassRefPtr<Hyphenation> platformGetHyphenation(const AtomicString& locale);
};

} // namespace blink

#endif // Hyphenation_h
