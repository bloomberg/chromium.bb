// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleWillChangeData_h
#define StyleWillChangeData_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

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
    return properties_ == o.properties_ && contents_ == o.contents_ &&
           scroll_position_ == o.scroll_position_;
  }

  bool operator!=(const StyleWillChangeData& o) const { return !(*this == o); }

  Vector<CSSPropertyID> properties_;
  unsigned contents_ : 1;
  unsigned scroll_position_ : 1;

 private:
  StyleWillChangeData();
  StyleWillChangeData(const StyleWillChangeData&);
};

}  // namespace blink

#endif  // StyleWillChangeData_h
