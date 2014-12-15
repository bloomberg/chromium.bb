/*
 * Copyright (C) 2007, 2008, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/css/CSSKeyframeRule.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/UseCounter.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

StyleKeyframe::StyleKeyframe()
{
}

StyleKeyframe::~StyleKeyframe()
{
}

String StyleKeyframe::keyText() const
{
    ASSERT(!m_keys.isEmpty());

    StringBuilder keyText;
    for (unsigned i = 0; i < m_keys.size(); ++i) {
        if (i)
            keyText.append(',');
        keyText.appendNumber(m_keys.at(i) * 100);
        keyText.append('%');
    }

    return keyText.toString();
}

bool StyleKeyframe::setKeyText(const String& keyText)
{
    ASSERT(!keyText.isNull());

    OwnPtr<Vector<double> > keys = CSSParser::parseKeyframeKeyList(keyText);
    if (keys->isEmpty())
        return false;

    m_keys = *keys;
    return true;
}

const Vector<double>& StyleKeyframe::keys() const
{
    return m_keys;
}

void StyleKeyframe::setKeys(PassOwnPtr<Vector<double> > keys)
{
    ASSERT(keys && !keys->isEmpty());
    m_keys = *keys;
}

MutableStylePropertySet& StyleKeyframe::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->mutableCopy();
    return *toMutableStylePropertySet(m_properties.get());
}

void StyleKeyframe::setProperties(PassRefPtrWillBeRawPtr<StylePropertySet> properties)
{
    ASSERT(properties);
    m_properties = properties;
}

String StyleKeyframe::cssText() const
{
    StringBuilder result;
    result.append(keyText());
    result.appendLiteral(" { ");
    String decls = m_properties->asText();
    result.append(decls);
    if (!decls.isEmpty())
        result.append(' ');
    result.append('}');
    return result.toString();
}

PassOwnPtr<Vector<double> > StyleKeyframe::createKeyList(CSSParserValueList* keys)
{
    size_t numKeys = keys ? keys->size() : 0;
    OwnPtr<Vector<double> > keyVector = adoptPtr(new Vector<double>(numKeys));
    for (size_t i = 0; i < numKeys; ++i) {
        ASSERT(keys->valueAt(i)->unit == blink::CSSPrimitiveValue::CSS_NUMBER);
        double key = keys->valueAt(i)->fValue;
        if (key < 0 || key > 100) {
            // As per http://www.w3.org/TR/css3-animations/#keyframes,
            // "If a keyframe selector specifies negative percentage values
            // or values higher than 100%, then the keyframe will be ignored."
            keyVector->clear();
            break;
        }
        keyVector->at(i) = key / 100;
    }
    return keyVector.release();
}

void StyleKeyframe::trace(Visitor* visitor)
{
    visitor->trace(m_properties);
}

CSSKeyframeRule::CSSKeyframeRule(StyleKeyframe* keyframe, CSSKeyframesRule* parent)
    : CSSRule(0)
    , m_keyframe(keyframe)
{
    setParentRule(parent);
}

CSSKeyframeRule::~CSSKeyframeRule()
{
#if !ENABLE(OILPAN)
    if (m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper->clearParentRule();
#endif
}

void CSSKeyframeRule::setKeyText(const String& keyText, ExceptionState& exceptionState)
{
    if (!m_keyframe->setKeyText(keyText))
        exceptionState.throwDOMException(SyntaxError, "The key '" + keyText + "' is invalid and cannot be parsed");
}

CSSStyleDeclaration* CSSKeyframeRule::style() const
{
    if (!m_propertiesCSSOMWrapper)
        m_propertiesCSSOMWrapper = StyleRuleCSSStyleDeclaration::create(m_keyframe->mutableProperties(), const_cast<CSSKeyframeRule*>(this));
    return m_propertiesCSSOMWrapper.get();
}

void CSSKeyframeRule::reattach(StyleRuleBase*)
{
    // No need to reattach, the underlying data is shareable on mutation.
    ASSERT_NOT_REACHED();
}

void CSSKeyframeRule::trace(Visitor* visitor)
{
    visitor->trace(m_keyframe);
    visitor->trace(m_propertiesCSSOMWrapper);
    CSSRule::trace(visitor);
}

} // namespace blink
