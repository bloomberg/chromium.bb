// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGPropertyHelper_h
#define SVGPropertyHelper_h

#include "core/svg/properties/SVGProperty.h"

namespace blink {

template <typename Derived>
class SVGPropertyHelper : public SVGPropertyBase {
 public:
  virtual SVGPropertyBase* CloneForAnimation(const String& value) const {
    Derived* property = Derived::Create();
    property->SetValueAsString(value);
    return property;
  }

  AnimatedPropertyType GetType() const override { return Derived::ClassType(); }
};

}  // namespace blink

#endif  // SVGPropertyHelper_h
