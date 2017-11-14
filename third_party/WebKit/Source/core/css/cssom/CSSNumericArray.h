// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSNumericArray_h
#define CSSNumericArray_h

#include "core/css/cssom/CSSNumericValue.h"

namespace blink {

// See CSSNumericArray.idl for more information about this class.
class CORE_EXPORT CSSNumericArray final : public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(CSSNumericArray);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // blink internal
  static CSSNumericArray* Create(CSSNumericValueVector values) {
    return new CSSNumericArray(std::move(values));
  }
  static CSSNumericArray* FromNumberishes(
      const HeapVector<CSSNumberish>& values) {
    CSSNumericValueVector result;
    for (const CSSNumberish& value : values) {
      result.push_back(CSSNumericValue::FromNumberish(value));
    }
    return new CSSNumericArray(result);
  }

  void Trace(Visitor* visitor) {
    visitor->Trace(values_);
    ScriptWrappable::Trace(visitor);
  }

  unsigned length() const { return values_.size(); }
  CSSNumericValue* AnonymousIndexedGetter(unsigned index) {
    if (index < values_.size())
      return values_[index].Get();
    return nullptr;
  }

 private:
  explicit CSSNumericArray(CSSNumericValueVector values)
      : values_(std::move(values)) {}

  CSSNumericValueVector values_;
};

}  // namespace blink

#endif  // CSSNumericArray_h
