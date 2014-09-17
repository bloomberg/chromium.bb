// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParser_h
#define CSSParser_h

#include "core/css/parser/BisonCSSParser.h"

namespace blink {

// This class serves as the public API for the css/parser subsystem

// FIXME: This should probably be a static-only class or a singleton class
class CSSParser {
    STACK_ALLOCATED();
public:
    explicit CSSParser(const CSSParserContext&);

    bool parseDeclaration(MutableStylePropertySet*, const String&, CSSParserObserver*, StyleSheetContents* contextStyleSheet);
    void parseSelector(const String&, CSSSelectorList&);

    static PassRefPtrWillBeRawPtr<StyleRuleBase> parseRule(const CSSParserContext&, StyleSheetContents*, const String&);
    static void parseSheet(const CSSParserContext&, StyleSheetContents*, const String&, const TextPosition& startPosition, CSSParserObserver*, bool logErrors = false);
    static bool parseValue(MutableStylePropertySet*, CSSPropertyID, const String&, bool important, CSSParserMode, StyleSheetContents*);

    // This is for non-shorthands only
    static PassRefPtrWillBeRawPtr<CSSValue> parseSingleValue(CSSPropertyID, const String&, const CSSParserContext& = strictCSSParserContext());

    static PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> parseInlineStyleDeclaration(const String&, Element*);

    static PassOwnPtr<Vector<double> > parseKeyframeKeyList(const String&);
    static PassRefPtrWillBeRawPtr<StyleKeyframe> parseKeyframeRule(const CSSParserContext&, StyleSheetContents*, const String&);

    static bool parseSupportsCondition(const String&);

    static PassRefPtrWillBeRawPtr<CSSValue> parseAnimationTimingFunctionValue(const String&);

    static bool parseColor(RGBA32& color, const String&, bool strict = false);
    static bool parseSystemColor(RGBA32& color, const String&);
    static StyleColor colorFromRGBColorString(const String&);

private:
    static bool parseValue(MutableStylePropertySet*, CSSPropertyID, const String&, bool important, const CSSParserContext&);

    BisonCSSParser m_bisonParser;
};

CSSPropertyID cssPropertyID(const String&);

} // namespace blink

#endif // CSSParser_h
