// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStringValue_h
#define CSSStringValue_h

#include "core/css/CSSValue.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSStringValue : public CSSValue {
 public:
  static CSSStringValue* Create(const String& str) {
    return new CSSStringValue(str);
  }

  String Value() const { return string_; }

  String CustomCSSText() const;

  bool Equals(const CSSStringValue& other) const {
    return string_ == other.string_;
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  CSSStringValue(const String&);

  String string_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSStringValue, IsStringValue());

}  // namespace blink

#endif  // CSSStringValue_h
