// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserImpl_h
#define CSSParserImpl_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserToken.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class MutableStylePropertySet;

class CSSParserImpl {
public:
    CSSParserImpl(const CSSParserContext&, const String&);
    static bool parseValue(MutableStylePropertySet*, CSSPropertyID, const String&, bool important, const CSSParserContext&);

private:
    void consumeDeclarationValue(CSSParserTokenIterator start, CSSParserTokenIterator end, CSSPropertyID, bool important, CSSRuleSourceData::Type);

    // FIXME: Can we build StylePropertySets directly?
    // FIXME: Investigate using a smaller inline buffer
    WillBeHeapVector<CSSProperty, 256> m_parsedProperties;
    Vector<CSSParserToken> m_tokens;
    CSSParserContext m_context;
};

} // namespace blink

#endif // CSSParserImpl_h
