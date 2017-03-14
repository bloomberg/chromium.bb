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
  static CSSNumericValue* parse(const String& cssText, ExceptionState&);

  virtual CSSNumericValue* add(const CSSNumericValue*, ExceptionState&) = 0;
  virtual CSSNumericValue* sub(const CSSNumericValue*, ExceptionState&) = 0;
  virtual CSSNumericValue* mul(double, ExceptionState&) = 0;
  virtual CSSNumericValue* div(double, ExceptionState&) = 0;

  virtual CSSNumericValue* to(const String&, ExceptionState&) = 0;
};

}  // namespace blink

#endif  // CSSNumericValue_h
