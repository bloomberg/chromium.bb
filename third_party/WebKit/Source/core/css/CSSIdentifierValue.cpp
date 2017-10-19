// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSIdentifierValue.h"

#include "core/css/CSSMarkup.h"
#include "core/css/CSSValuePool.h"
#include "platform/Length.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

CSSIdentifierValue* CSSIdentifierValue::Create(CSSValueID value_id) {
  CSSIdentifierValue* css_value = CssValuePool().IdentifierCacheValue(value_id);
  if (!css_value) {
    css_value = CssValuePool().SetIdentifierCacheValue(
        value_id, new CSSIdentifierValue(value_id));
  }
  return css_value;
}

String CSSIdentifierValue::CustomCSSText() const {
  return AtomicString(getValueName(value_id_));
}

CSSIdentifierValue::CSSIdentifierValue(CSSValueID value_id)
    : CSSValue(kIdentifierClass), value_id_(value_id) {
  // TODO(sashab): Add a DCHECK_NE(valueID, CSSValueInvalid) once no code paths
  // cause this to happen.
}

CSSIdentifierValue::CSSIdentifierValue(const Length& length)
    : CSSValue(kIdentifierClass) {
  switch (length.GetType()) {
    case kAuto:
      value_id_ = CSSValueAuto;
      break;
    case kMinContent:
      value_id_ = CSSValueMinContent;
      break;
    case kMaxContent:
      value_id_ = CSSValueMaxContent;
      break;
    case kFillAvailable:
      value_id_ = CSSValueWebkitFillAvailable;
      break;
    case kFitContent:
      value_id_ = CSSValueFitContent;
      break;
    case kExtendToZoom:
      value_id_ = CSSValueInternalExtendToZoom;
    case kPercent:
    case kFixed:
    case kCalculated:
    case kDeviceWidth:
    case kDeviceHeight:
    case kMaxSizeNone:
      NOTREACHED();
      break;
  }
}

void CSSIdentifierValue::TraceAfterDispatch(blink::Visitor* visitor) {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
