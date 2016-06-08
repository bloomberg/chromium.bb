// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMap.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"

namespace blink {

namespace {

class StylePropertyMapIterationSource final : public PairIterable<String, CSSStyleValueOrCSSStyleValueSequence>::IterationSource {
public:
    explicit StylePropertyMapIterationSource(HeapVector<StylePropertyMap::StylePropertyMapEntry> values)
        : m_index(0)
        , m_values(values)
    {
    }

    bool next(ScriptState*, String& key, CSSStyleValueOrCSSStyleValueSequence& value, ExceptionState&) override
    {
        if (m_index >= m_values.size())
            return false;

        const StylePropertyMap::StylePropertyMapEntry& pair = m_values.at(m_index++);
        key = pair.first;
        value = pair.second;
        return true;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_values);
        PairIterable<String, CSSStyleValueOrCSSStyleValueSequence>::IterationSource::trace(visitor);
    }

private:
    size_t m_index;
    const HeapVector<StylePropertyMap::StylePropertyMapEntry> m_values;
};

} // namespace

CSSStyleValue* StylePropertyMap::get(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID == CSSPropertyInvalid) {
        // TODO(meade): Handle custom properties here.
        exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
        return nullptr;
    }

    StyleValueVector styleVector = getAll(propertyID);
    if (styleVector.isEmpty())
        return nullptr;

    return styleVector[0];
}

StylePropertyMap::StyleValueVector StylePropertyMap::getAll(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid)
        return getAll(propertyID);

    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return StyleValueVector();
}

bool StylePropertyMap::has(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid)
        return !getAll(propertyID).isEmpty();

    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return false;
}

void StylePropertyMap::set(const String& propertyName, CSSStyleValueOrCSSStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid) {
        set(propertyID, item, exceptionState);
        return;
    }
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

void StylePropertyMap::append(const String& propertyName, CSSStyleValueOrCSSStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid) {
        append(propertyID, item, exceptionState);
        return;
    }
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

void StylePropertyMap::remove(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid) {
        remove(propertyID, exceptionState);
        return;
    }
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

StylePropertyMap::StyleValueVector StylePropertyMap::cssValueToStyleValueVector(CSSPropertyID propertyID, const CSSValue& cssValue)
{
    StyleValueVector styleValueVector;

    if (!cssValue.isValueList()) {
        CSSStyleValue* styleValue = StyleValueFactory::create(propertyID, cssValue);
        if (styleValue)
            styleValueVector.append(styleValue);
        return styleValueVector;
    }

    for (const CSSValue* value : toCSSValueList(cssValue)) {
        CSSStyleValue* styleValue = StyleValueFactory::create(propertyID, *value);
        if (!styleValue)
            return StyleValueVector();
        styleValueVector.append(styleValue);
    }
    return styleValueVector;
}

StylePropertyMap::IterationSource* StylePropertyMap::startIteration(ScriptState*, ExceptionState&)
{
    return new StylePropertyMapIterationSource(getIterationEntries());
}

} // namespace blink
