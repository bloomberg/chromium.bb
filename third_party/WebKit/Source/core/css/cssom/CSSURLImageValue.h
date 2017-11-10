// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSURLImageValue_h
#define CSSURLImageValue_h

#include "core/css/cssom/CSSStyleImageValue.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

class CORE_EXPORT CSSURLImageValue final : public CSSStyleImageValue {
  WTF_MAKE_NONCOPYABLE(CSSURLImageValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSURLImageValue* Create(ScriptState* script_state,
                                  const AtomicString& url) {
    const auto* execution_context = ExecutionContext::From(script_state);
    DCHECK(execution_context);
    // Use absolute URL for CSSImageValue but keep relative URL for
    // getter and serialization.
    return new CSSURLImageValue(CSSImageValue::Create(
        url, execution_context->CompleteURL(url), Referrer()));
  }
  static CSSURLImageValue* Create(const CSSImageValue* image_value) {
    return new CSSURLImageValue(image_value);
  }

  StyleValueType GetType() const override { return kURLImageType; }

  const CSSValue* ToCSSValue() const override { return CssImageValue(); }

  const String& url() const { return CssImageValue()->RelativeUrl(); }

 private:
  explicit CSSURLImageValue(const CSSImageValue* image_value)
      : CSSStyleImageValue(image_value) {}
};

}  // namespace blink

#endif  // CSSResourceValue_h
