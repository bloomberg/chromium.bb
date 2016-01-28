// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Deprecation_h
#define Deprecation_h

#include "core/CSSPropertyNames.h"
#include "wtf/BitVector.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LocalFrame;

class Deprecation {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(Deprecation);
public:
    Deprecation();
    ~Deprecation();

    static void warnOnDeprecatedProperties(const LocalFrame*, CSSPropertyID unresolvedProperty);

protected:
    void suppress(CSSPropertyID unresolvedProperty);
    bool isSuppressed(CSSPropertyID unresolvedProperty);
    // CSSPropertyIDs that aren't deprecated return an empty string.
    static String deprecationMessage(CSSPropertyID unresolvedProperty);

    BitVector m_cssPropertyDeprecationBits;
};

} // namespace blink

#endif // Deprecation_h
