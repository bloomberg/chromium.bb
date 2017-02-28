// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSPaintValue.h"

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/layout/LayoutObject.h"
#include "platform/graphics/Image.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSPaintValue::CSSPaintValue(CSSCustomIdentValue* name)
    : CSSImageGeneratorValue(PaintClass),
      m_name(name),
      m_paintImageGeneratorObserver(new Observer(this)) {}

CSSPaintValue::CSSPaintValue(CSSCustomIdentValue* name,
                             Vector<RefPtr<CSSVariableData>>& variableData)
    : CSSPaintValue(name) {
  m_argumentVariableData.swap(variableData);
}

CSSPaintValue::~CSSPaintValue() {}

String CSSPaintValue::customCSSText() const {
  StringBuilder result;
  result.append("paint(");
  result.append(m_name->customCSSText());
  for (const auto& variableData : m_argumentVariableData) {
    result.append(", ");
    result.append(variableData.get()->tokenRange().serialize());
  }
  result.append(')');
  return result.toString();
}

String CSSPaintValue::name() const {
  return m_name->value();
}

PassRefPtr<Image> CSSPaintValue::image(const LayoutObject& layoutObject,
                                       const IntSize& size,
                                       float zoom) {
  if (!m_generator)
    m_generator = CSSPaintImageGenerator::create(
        name(), layoutObject.document(), m_paintImageGeneratorObserver);

  if (!parseInputArguments())
    return nullptr;

  return m_generator->paint(layoutObject, size, zoom, m_parsedInputArguments);
}

bool CSSPaintValue::parseInputArguments() {
  if (m_inputArgumentsInvalid)
    return false;

  if (m_parsedInputArguments ||
      !RuntimeEnabledFeatures::cssPaintAPIArgumentsEnabled())
    return true;

  if (!m_generator->isImageGeneratorReady())
    return false;

  const Vector<CSSSyntaxDescriptor>& inputArgumentTypes =
      m_generator->inputArgumentTypes();
  if (m_argumentVariableData.size() != inputArgumentTypes.size()) {
    m_inputArgumentsInvalid = true;
    return false;
  }

  m_parsedInputArguments = new CSSStyleValueVector();

  for (size_t i = 0; i < m_argumentVariableData.size(); ++i) {
    const CSSValue* parsedValue =
        m_argumentVariableData[i]->parseForSyntax(inputArgumentTypes[i]);
    if (!parsedValue) {
      m_inputArgumentsInvalid = true;
      m_parsedInputArguments = nullptr;
      return false;
    }
    m_parsedInputArguments->appendVector(
        StyleValueFactory::cssValueToStyleValueVector(*parsedValue));
  }
  return true;
}

void CSSPaintValue::Observer::paintImageGeneratorReady() {
  m_ownerValue->paintImageGeneratorReady();
}

void CSSPaintValue::paintImageGeneratorReady() {
  for (const LayoutObject* client : clients().keys()) {
    const_cast<LayoutObject*>(client)->imageChanged(
        static_cast<WrappedImagePtr>(this));
  }
}

bool CSSPaintValue::knownToBeOpaque(const LayoutObject& layoutObject) const {
  return m_generator && !m_generator->hasAlpha();
}

bool CSSPaintValue::equals(const CSSPaintValue& other) const {
  return name() == other.name() && customCSSText() == other.customCSSText();
}

DEFINE_TRACE_AFTER_DISPATCH(CSSPaintValue) {
  visitor->trace(m_name);
  visitor->trace(m_generator);
  visitor->trace(m_paintImageGeneratorObserver);
  visitor->trace(m_parsedInputArguments);
  CSSImageGeneratorValue::traceAfterDispatch(visitor);
}

}  // namespace blink
