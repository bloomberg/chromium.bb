// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSNamespaceRule.h"

#include "core/css/CSSMarkup.h"
#include "core/css/StyleRuleNamespace.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

CSSNamespaceRule::CSSNamespaceRule(StyleRuleNamespace* namespace_rule,
                                   CSSStyleSheet* parent)
    : CSSRule(parent), namespace_rule_(namespace_rule) {}

CSSNamespaceRule::~CSSNamespaceRule() {}

String CSSNamespaceRule::cssText() const {
  StringBuilder result;
  result.Append("@namespace ");
  SerializeIdentifier(prefix(), result);
  if (!prefix().IsEmpty())
    result.Append(' ');
  result.Append("url(");
  result.Append(SerializeString(namespaceURI()));
  result.Append(");");
  return result.ToString();
}

AtomicString CSSNamespaceRule::namespaceURI() const {
  return namespace_rule_->Uri();
}

AtomicString CSSNamespaceRule::prefix() const {
  return namespace_rule_->Prefix();
}

void CSSNamespaceRule::Trace(blink::Visitor* visitor) {
  visitor->Trace(namespace_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
