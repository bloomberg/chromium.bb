// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleRuleKeyframe_h
#define StyleRuleKeyframe_h

#include "core/css/StyleRule.h"
#include <memory>

namespace blink {

class MutableCSSPropertyValueSet;
class CSSPropertyValueSet;

class StyleRuleKeyframe final : public StyleRuleBase {
 public:
  static StyleRuleKeyframe* Create(std::unique_ptr<Vector<double>> keys,
                                   CSSPropertyValueSet* properties) {
    return new StyleRuleKeyframe(std::move(keys), properties);
  }

  // Exposed to JavaScript.
  String KeyText() const;
  bool SetKeyText(const String&);

  // Used by StyleResolver.
  const Vector<double>& Keys() const;

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();

  String CssText() const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  StyleRuleKeyframe(std::unique_ptr<Vector<double>>, CSSPropertyValueSet*);

  Member<CSSPropertyValueSet> properties_;
  Vector<double> keys_;
};

DEFINE_STYLE_RULE_TYPE_CASTS(Keyframe);

}  // namespace blink

#endif  // StyleRuleKeyframe_h
