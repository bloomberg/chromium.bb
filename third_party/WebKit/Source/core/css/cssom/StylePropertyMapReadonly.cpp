// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMapReadonly.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"

namespace blink {

namespace {

class StylePropertyMapIterationSource final
    : public PairIterable<String, CSSStyleValueOrCSSStyleValueSequence>::
          IterationSource {
 public:
  explicit StylePropertyMapIterationSource(
      HeapVector<StylePropertyMapReadonly::StylePropertyMapEntry> values)
      : m_index(0), m_values(values) {}

  bool next(ScriptState*,
            String& key,
            CSSStyleValueOrCSSStyleValueSequence& value,
            ExceptionState&) override {
    if (m_index >= m_values.size())
      return false;

    const StylePropertyMapReadonly::StylePropertyMapEntry& pair =
        m_values.at(m_index++);
    key = pair.first;
    value = pair.second;
    return true;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_values);
    PairIterable<String, CSSStyleValueOrCSSStyleValueSequence>::
        IterationSource::trace(visitor);
  }

 private:
  size_t m_index;
  const HeapVector<StylePropertyMapReadonly::StylePropertyMapEntry> m_values;
};

}  // namespace

CSSStyleValue* StylePropertyMapReadonly::get(const String& propertyName,
                                             ExceptionState& exceptionState) {
  CSSPropertyID propertyID = cssPropertyID(propertyName);
  if (propertyID == CSSPropertyInvalid || propertyID == CSSPropertyVariable) {
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return nullptr;
  }

  CSSStyleValueVector styleVector = getAllInternal(propertyID);
  if (styleVector.isEmpty())
    return nullptr;

  return styleVector[0];
}

CSSStyleValueVector StylePropertyMapReadonly::getAll(
    const String& propertyName,
    ExceptionState& exceptionState) {
  CSSPropertyID propertyID = cssPropertyID(propertyName);
  if (propertyID != CSSPropertyInvalid && propertyID != CSSPropertyVariable)
    return getAllInternal(propertyID);

  // TODO(meade): Handle custom properties here.
  exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
  return CSSStyleValueVector();
}

bool StylePropertyMapReadonly::has(const String& propertyName,
                                   ExceptionState& exceptionState) {
  CSSPropertyID propertyID = cssPropertyID(propertyName);
  if (propertyID != CSSPropertyInvalid && propertyID != CSSPropertyVariable)
    return !getAllInternal(propertyID).isEmpty();

  // TODO(meade): Handle custom properties here.
  exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
  return false;
}

StylePropertyMapReadonly::IterationSource*
StylePropertyMapReadonly::startIteration(ScriptState*, ExceptionState&) {
  return new StylePropertyMapIterationSource(getIterationEntries());
}

}  // namespace blink
