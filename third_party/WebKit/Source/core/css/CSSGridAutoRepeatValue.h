// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSGridAutoRepeatValue_h
#define CSSGridAutoRepeatValue_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSValueList.h"

namespace blink {

// CSSGridAutoRepeatValue stores the track sizes and line numbers when the
// auto-repeat syntax is used
//
// Right now the auto-repeat syntax is as follows:
// <auto-repeat>  = repeat( [ auto-fill | auto-fit ], <line-names>? <fixed-size>
// <line-names>? )
//
// meaning that only one fixed size track is allowed. It could be argued that a
// different class storing two CSSGridLineNamesValue and one CSSValue (for the
// track size) fits better but the CSSWG has left the door open to allow more
// than one track in the future. That's why we're using a list, it's prepared
// for future changes and it also allows us to keep the parsing algorithm almost
// intact.
class CSSGridAutoRepeatValue : public CSSValueList {
 public:
  static CSSGridAutoRepeatValue* Create(CSSValueID id) {
    return new CSSGridAutoRepeatValue(id);
  }

  String CustomCSSText() const;
  CSSValueID AutoRepeatID() const { return auto_repeat_id_; }

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValueList::TraceAfterDispatch(visitor);
  }

 private:
  CSSGridAutoRepeatValue(CSSValueID id)
      : CSSValueList(kGridAutoRepeatClass, kSpaceSeparator),
        auto_repeat_id_(id) {
    DCHECK(id == CSSValueAutoFill || id == CSSValueAutoFit);
  }

  const CSSValueID auto_repeat_id_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSGridAutoRepeatValue, IsGridAutoRepeatValue());

}  // namespace blink

#endif  // CSSGridAutoRepeatValue_h
