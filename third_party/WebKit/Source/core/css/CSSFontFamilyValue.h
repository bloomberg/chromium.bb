// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSFontFamilyValue_h
#define CSSFontFamilyValue_h

#include "core/css/CSSValue.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT CSSFontFamilyValue : public CSSValue {
 public:
  static CSSFontFamilyValue* Create(const String& family_name);

  String Value() const { return string_; }

  String CustomCSSText() const;

  bool Equals(const CSSFontFamilyValue& other) const {
    return string_ == other.string_;
  }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  friend class CSSValuePool;

  CSSFontFamilyValue(const String&);

  // TODO(sashab): Change this to an AtomicString.
  String string_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSFontFamilyValue, IsFontFamilyValue());

}  // namespace blink

#endif  // CSSFontFamilyValue_h
