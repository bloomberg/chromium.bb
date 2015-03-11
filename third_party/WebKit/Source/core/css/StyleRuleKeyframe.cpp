// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/StyleRuleKeyframe.h"

#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

StyleRuleKeyframe::StyleRuleKeyframe()
: StyleRuleBase(Keyframe)
{
}

String StyleRuleKeyframe::keyText() const
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

bool StyleRuleKeyframe::setKeyText(const String& keyText)
{
    ASSERT(!keyText.isNull());

    OwnPtr<Vector<double>> keys = CSSParser::parseKeyframeKeyList(keyText);
    if (!keys || keys->isEmpty())
        return false;

    m_keys = *keys;
    return true;
}

const Vector<double>& StyleRuleKeyframe::keys() const
{
    return m_keys;
}

void StyleRuleKeyframe::setKeys(PassOwnPtr<Vector<double>> keys)
{
    ASSERT(keys && !keys->isEmpty());
    m_keys = *keys;
}

MutableStylePropertySet& StyleRuleKeyframe::mutableProperties()
{
    if (!m_properties->isMutable())
        m_properties = m_properties->mutableCopy();
    return *toMutableStylePropertySet(m_properties.get());
}

void StyleRuleKeyframe::setProperties(PassRefPtrWillBeRawPtr<StylePropertySet> properties)
{
    ASSERT(properties);
    m_properties = properties;
}

String StyleRuleKeyframe::cssText() const
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

PassOwnPtr<Vector<double>> StyleRuleKeyframe::createKeyList(CSSParserValueList* keys)
{
    size_t numKeys = keys ? keys->size() : 0;
    OwnPtr<Vector<double>> keyVector = adoptPtr(new Vector<double>(numKeys));
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

DEFINE_TRACE_AFTER_DISPATCH(StyleRuleKeyframe)
{
    visitor->trace(m_properties);
    StyleRuleBase::traceAfterDispatch(visitor);
}

} // namespace blink
