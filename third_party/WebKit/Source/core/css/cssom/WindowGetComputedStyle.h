// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowGetComputedStyle_h
#define WindowGetComputedStyle_h

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/cssom/ComputedStylePropertyMap.h"

namespace blink {

class WindowGetComputedStyle {
  STATIC_ONLY(WindowGetComputedStyle);

 public:
  static StylePropertyMapReadonly* getComputedStyleMap(
      const LocalDOMWindow&,
      Element* element,
      const String& pseudo_element) {
    DCHECK(element);
    return ComputedStylePropertyMap::Create(element, pseudo_element);
  }
};

}  // namespace blink

#endif  // WindowGetComputedStyle_h
