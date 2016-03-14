// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSPaintValue.h"

#include "core/css/CSSCustomIdentValue.h"
#include "platform/graphics/Image.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSPaintValue::CSSPaintValue(PassRefPtrWillBeRawPtr<CSSCustomIdentValue> name)
    : CSSImageGeneratorValue(PaintClass)
    , m_name(name)
{
}

CSSPaintValue::~CSSPaintValue()
{
}

String CSSPaintValue::customCSSText() const
{
    StringBuilder result;
    result.appendLiteral("paint(");
    result.append(m_name->customCSSText());
    result.append(')');
    return result.toString();
}

String CSSPaintValue::name() const
{
    return m_name->value();
}

PassRefPtr<Image> CSSPaintValue::image(const LayoutObject*, const IntSize&)
{
    // TODO(ikilpatrick): implement.
    return nullptr;
}

bool CSSPaintValue::equals(const CSSPaintValue& other) const
{
    return name() == other.name();
}

DEFINE_TRACE_AFTER_DISPATCH(CSSPaintValue)
{
    visitor->trace(m_name);
    CSSImageGeneratorValue::traceAfterDispatch(visitor);
}

} // namespace blink
