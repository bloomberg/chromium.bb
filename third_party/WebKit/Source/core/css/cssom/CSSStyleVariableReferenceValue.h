// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleVariableReferenceValue_h
#define CSSStyleVariableReferenceValue_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSUnparsedValue.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

// CSSStyleVariableReferenceValue represents a CSS var() value for CSS Typed OM.
// The corresponding idl file is CSSVariableReferenceValue.idl.
class CORE_EXPORT CSSStyleVariableReferenceValue final
    : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~CSSStyleVariableReferenceValue() {}

  static CSSStyleVariableReferenceValue* Create(
      const String& variable,
      const CSSUnparsedValue* fallback) {
    return new CSSStyleVariableReferenceValue(variable, fallback);
  }

  const String& variable() const { return variable_; }

  CSSUnparsedValue* fallback() {
    return const_cast<CSSUnparsedValue*>(fallback_.Get());
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(fallback_);
    ScriptWrappable::Trace(visitor);
  }

 protected:
  CSSStyleVariableReferenceValue(const String& variable,
                                 const CSSUnparsedValue* fallback)
      : variable_(variable), fallback_(fallback) {}

  String variable_;
  Member<const CSSUnparsedValue> fallback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CSSStyleVariableReferenceValue);
};

}  // namespace blink

#endif  // CSSStyleVariableReference_h
