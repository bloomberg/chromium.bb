// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSFontFamilyValue.h"

#include "core/css/CSSMarkup.h"
#include "core/css/CSSValuePool.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

CSSFontFamilyValue* CSSFontFamilyValue::Create(const String& family_name) {
  if (family_name.IsNull())
    return new CSSFontFamilyValue(family_name);
  CSSValuePool::FontFamilyValueCache::AddResult entry =
      CssValuePool().GetFontFamilyCacheEntry(family_name);
  if (!entry.stored_value->value)
    entry.stored_value->value = new CSSFontFamilyValue(family_name);
  return entry.stored_value->value;
}

CSSFontFamilyValue::CSSFontFamilyValue(const String& str)
    : CSSValue(kFontFamilyClass), string_(str) {}

String CSSFontFamilyValue::CustomCSSText() const {
  return SerializeFontFamily(string_);
}

void CSSFontFamilyValue::TraceAfterDispatch(blink::Visitor* visitor) {
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
