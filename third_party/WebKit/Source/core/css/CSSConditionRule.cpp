// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSConditionRule.h"

#include "core/css/CSSStyleSheet.h"

namespace blink {

CSSConditionRule::CSSConditionRule(StyleRuleCondition* condition_rule,
                                   CSSStyleSheet* parent)
    : CSSGroupingRule(condition_rule, parent) {}

CSSConditionRule::~CSSConditionRule() = default;

String CSSConditionRule::conditionText() const {
  return static_cast<StyleRuleCondition*>(group_rule_.Get())->ConditionText();
}

}  // namespace blink
