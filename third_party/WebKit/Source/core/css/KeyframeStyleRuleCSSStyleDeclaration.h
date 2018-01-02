// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KeyframeStyleRuleCSSStyleDeclaration_h
#define KeyframeStyleRuleCSSStyleDeclaration_h

#include "core/css/StyleRuleCSSStyleDeclaration.h"

namespace blink {

class CSSKeyframeRule;

class KeyframeStyleRuleCSSStyleDeclaration final
    : public StyleRuleCSSStyleDeclaration {
 public:
  static KeyframeStyleRuleCSSStyleDeclaration* Create(
      MutableCSSPropertyValueSet& property_set,
      CSSKeyframeRule* parent_rule) {
    return new KeyframeStyleRuleCSSStyleDeclaration(property_set, parent_rule);
  }

 private:
  KeyframeStyleRuleCSSStyleDeclaration(MutableCSSPropertyValueSet&,
                                       CSSKeyframeRule*);

  void DidMutate(MutationType) override;
  bool IsKeyframeStyle() const final { return true; }
};

}  // namespace blink

#endif
