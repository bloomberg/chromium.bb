// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSCustomIdentValue.h"

#include "core/css/CSSMarkup.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

CSSCustomIdentValue::CSSCustomIdentValue(const AtomicString& str)
    : CSSValue(kCustomIdentClass),
      string_(str),
      property_id_(CSSPropertyInvalid) {}

CSSCustomIdentValue::CSSCustomIdentValue(CSSPropertyID id)
    : CSSValue(kCustomIdentClass), string_(), property_id_(id) {
  DCHECK(IsKnownPropertyID());
}

String CSSCustomIdentValue::CustomCSSText() const {
  if (IsKnownPropertyID())
    return getPropertyNameAtomicString(property_id_);
  StringBuilder builder;
  SerializeIdentifier(string_, builder);
  return builder.ToString();
}

void CSSCustomIdentValue::TraceAfterDispatch(blink::Visitor* visitor) {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
