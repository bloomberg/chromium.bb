// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSURLImageValue_h
#define CSSURLImageValue_h

#include "core/css/cssom/CSSStyleImageValue.h"

namespace blink {

class CORE_EXPORT CSSURLImageValue final : public CSSStyleImageValue {
  WTF_MAKE_NONCOPYABLE(CSSURLImageValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSURLImageValue* Create(const AtomicString& url) {
    return new CSSURLImageValue(CSSImageValue::Create(url));
  }
  static CSSURLImageValue* Create(const CSSImageValue* image_value) {
    return new CSSURLImageValue(image_value);
  }

  StyleValueType GetType() const override { return kURLImageType; }

  const CSSValue* ToCSSValue() const override { return CssImageValue(); }

  const String& url() const { return CssImageValue()->Url(); }

 private:
  explicit CSSURLImageValue(const CSSImageValue* image_value)
      : CSSStyleImageValue(image_value) {}
};

}  // namespace blink

#endif  // CSSResourceValue_h
