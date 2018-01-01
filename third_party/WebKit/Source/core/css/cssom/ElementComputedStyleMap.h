// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ElementComputedStyleMap_h
#define ElementComputedStyleMap_h

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/ComputedStylePropertyMap.h"

namespace blink {

class ElementComputedStyleMap {
  STATIC_ONLY(ElementComputedStyleMap);

 public:
  static StylePropertyMapReadOnly* computedStyleMap(Element& element) {
    return ComputedStylePropertyMap::Create(&element);
  }
};

}  // namespace blink

#endif  // ElementComputedStyleMap_h
