// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserImpl_h
#define CSSParserImpl_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class StyleRule;
class StyleRuleBase;
class ImmutableStylePropertySet;
class Element;
class MutableStylePropertySet;

class CSSParserImpl {
    STACK_ALLOCATED();
public:
    CSSParserImpl(const CSSParserContext&, const String&);
    static bool parseValue(MutableStylePropertySet*, CSSPropertyID, const String&, bool important, const CSSParserContext&);
    static PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> parseInlineStyleDeclaration(const String&, Element*);
    static bool parseDeclaration(MutableStylePropertySet*, const String&, const CSSParserContext&);
    static PassRefPtrWillBeRawPtr<StyleRuleBase> parseRule(const String&, const CSSParserContext&);

private:
    // These two functions update the range they're given
    PassRefPtrWillBeRawPtr<StyleRuleBase> consumeAtRule(CSSParserTokenRange&);
    PassRefPtrWillBeRawPtr<StyleRuleBase> consumeQualifiedRule(CSSParserTokenRange&);

    PassRefPtrWillBeRawPtr<StyleRule> consumeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block);

    // FIXME: We should use a CSSRule::Type here
    void consumeDeclarationList(CSSParserTokenRange, CSSRuleSourceData::Type);
    void consumeDeclaration(CSSParserTokenRange, CSSRuleSourceData::Type);
    void consumeDeclarationValue(CSSParserTokenRange, CSSPropertyID, bool important, CSSRuleSourceData::Type);

    // FIXME: Can we build StylePropertySets directly?
    // FIXME: Investigate using a smaller inline buffer
    WillBeHeapVector<CSSProperty, 256> m_parsedProperties;
    Vector<CSSParserToken> m_tokens;
    CSSParserContext m_context;

    // FIXME: We need to store a context style sheet, similar to the Bison parser
    // for at least crbug.com/9877 and marking that we've seen rem units.
};

} // namespace blink

#endif // CSSParserImpl_h
