// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleWillChangeData_h
#define StyleWillChangeData_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"

namespace blink {

class StyleWillChangeData : public RefCounted<StyleWillChangeData> {
 public:
  static PassRefPtr<StyleWillChangeData> Create() {
    return AdoptRef(new StyleWillChangeData);
  }
  PassRefPtr<StyleWillChangeData> Copy() const {
    return AdoptRef(new StyleWillChangeData(*this));
  }

  bool operator==(const StyleWillChangeData& o) const {
    return will_change_properties_ == o.will_change_properties_ &&
           will_change_contents_ == o.will_change_contents_ &&
           will_change_scroll_position_ == o.will_change_scroll_position_;
  }

  bool operator!=(const StyleWillChangeData& o) const { return !(*this == o); }

  Vector<CSSPropertyID> will_change_properties_;
  unsigned will_change_contents_ : 1;
  unsigned will_change_scroll_position_ : 1;

 private:
  StyleWillChangeData();
  StyleWillChangeData(const StyleWillChangeData&);
};

}  // namespace blink

#endif  // StyleWillChangeData_h
