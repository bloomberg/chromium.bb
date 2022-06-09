// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_rule.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSContainerRule::CSSContainerRule(StyleRuleContainer* container_rule,
                                   CSSStyleSheet* parent)
    : CSSConditionRule(container_rule, parent) {}

CSSContainerRule::~CSSContainerRule() = default;

String CSSContainerRule::cssText() const {
  StringBuilder result;
  result.Append("@container ");
  const ContainerSelector& selector = ContainerQuery().Selector();
  if (!selector.IsNearest()) {
    result.Append(selector.ToString());
    result.Append(' ');
  }
  result.Append(ContainerQuery().ToString());
  result.Append(' ');
  result.Append("{\n");
  AppendCSSTextForItems(result);
  result.Append('}');
  return result.ReleaseString();
}

const AtomicString& CSSContainerRule::Name() const {
  return ContainerQuery().Selector().Name();
}

void CSSContainerRule::SetConditionText(
    const ExecutionContext* execution_context,
    String value) {
  CSSStyleSheet::RuleMutationScope mutation_scope(this);
  To<StyleRuleContainer>(group_rule_.Get())
      ->SetConditionText(execution_context, value);
}

const ContainerQuery& CSSContainerRule::ContainerQuery() const {
  return To<StyleRuleContainer>(group_rule_.Get())->GetContainerQuery();
}

}  // namespace blink
