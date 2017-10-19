// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSCustomIdentValue_h
#define CSSCustomIdentValue_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSValue.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class CSSCustomIdentValue : public CSSValue {
 public:
  static CSSCustomIdentValue* Create(const AtomicString& str) {
    return new CSSCustomIdentValue(str);
  }

  // TODO(sashab, timloh): Remove this and lazily parse the CSSPropertyID in
  // isKnownPropertyID().
  static CSSCustomIdentValue* Create(CSSPropertyID id) {
    return new CSSCustomIdentValue(id);
  }

  AtomicString Value() const {
    DCHECK(!IsKnownPropertyID());
    return string_;
  }
  bool IsKnownPropertyID() const { return property_id_ != CSSPropertyInvalid; }
  CSSPropertyID ValueAsPropertyID() const {
    DCHECK(IsKnownPropertyID());
    return property_id_;
  }

  String CustomCSSText() const;

  bool Equals(const CSSCustomIdentValue& other) const {
    return IsKnownPropertyID() ? property_id_ == other.property_id_
                               : string_ == other.string_;
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  explicit CSSCustomIdentValue(const AtomicString&);
  explicit CSSCustomIdentValue(CSSPropertyID);

  AtomicString string_;
  CSSPropertyID property_id_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCustomIdentValue, IsCustomIdentValue());

}  // namespace blink

#endif  // CSSCustomIdentValue_h
