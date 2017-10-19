// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSContentDistributionValue_h
#define CSSContentDistributionValue_h

#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSValuePair.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class CSSContentDistributionValue : public CSSValue {
 public:
  static CSSContentDistributionValue* Create(CSSValueID distribution,
                                             CSSValueID position,
                                             CSSValueID overflow) {
    return new CSSContentDistributionValue(distribution, position, overflow);
  }
  ~CSSContentDistributionValue();

  // TODO(sashab): Make these return CSSValueIDs instead of CSSValues.
  CSSIdentifierValue* Distribution() const {
    return CSSIdentifierValue::Create(distribution_);
  }

  CSSIdentifierValue* GetPosition() const {
    return CSSIdentifierValue::Create(position_);
  }

  CSSIdentifierValue* Overflow() const {
    return CSSIdentifierValue::Create(overflow_);
  }

  String CustomCSSText() const;

  bool Equals(const CSSContentDistributionValue&) const;

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  CSSContentDistributionValue(CSSValueID distribution,
                              CSSValueID position,
                              CSSValueID overflow);

  CSSValueID distribution_;
  CSSValueID position_;
  CSSValueID overflow_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSContentDistributionValue,
                            IsContentDistributionValue());

}  // namespace blink

#endif  // CSSContentDistributionValue_h
