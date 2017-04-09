// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSNumericValue_h
#define CSSNumericValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT CSSNumericValue : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSNumericValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSNumericValue* parse(const String& css_text, ExceptionState&);

  virtual CSSNumericValue* add(const CSSNumericValue*, ExceptionState&) {
    // TODO(meade): Implement.
    return nullptr;
  }
  virtual CSSNumericValue* sub(const CSSNumericValue*, ExceptionState&) {
    // TODO(meade): Implement.
    return nullptr;
  }
  virtual CSSNumericValue* mul(double, ExceptionState&) {
    // TODO(meade): Implement.
    return nullptr;
  }
  virtual CSSNumericValue* div(double, ExceptionState&) {
    // TODO(meade): Implement.
    return nullptr;
  }

  virtual CSSNumericValue* to(const String&, ExceptionState&) {
    // TODO(meade): Implement.
    return nullptr;
  }

 protected:
  CSSNumericValue() {}
};

}  // namespace blink

#endif  // CSSNumericValue_h
