// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableReferenceValue_h
#define CSSVariableReferenceValue_h

#include "core/css/CSSValue.h"
#include "core/css/CSSVariableData.h"
#include "wtf/RefPtr.h"

namespace blink {

class CSSVariableReferenceValue : public CSSValue {
 public:
  static CSSVariableReferenceValue* Create(PassRefPtr<CSSVariableData> data) {
    return new CSSVariableReferenceValue(std::move(data));
  }

  CSSVariableData* VariableDataValue() const { return data_.Get(); }

  bool Equals(const CSSVariableReferenceValue& other) const {
    return data_ == other.data_;
  }
  String CustomCSSText() const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSVariableReferenceValue(PassRefPtr<CSSVariableData> data)
      : CSSValue(kVariableReferenceClass), data_(std::move(data)) {}

  RefPtr<CSSVariableData> data_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSVariableReferenceValue,
                            IsVariableReferenceValue());

}  // namespace blink

#endif  // CSSVariableReferenceValue_h
