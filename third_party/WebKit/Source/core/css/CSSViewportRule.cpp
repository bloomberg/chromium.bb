/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/css/CSSViewportRule.h"

#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

CSSViewportRule::CSSViewportRule(StyleRuleViewport* viewport_rule,
                                 CSSStyleSheet* sheet)
    : CSSRule(sheet), viewport_rule_(viewport_rule) {}

CSSViewportRule::~CSSViewportRule() {}

CSSStyleDeclaration* CSSViewportRule::style() const {
  if (!properties_cssom_wrapper_)
    properties_cssom_wrapper_ = StyleRuleCSSStyleDeclaration::Create(
        viewport_rule_->MutableProperties(),
        const_cast<CSSViewportRule*>(this));

  return properties_cssom_wrapper_.Get();
}

String CSSViewportRule::cssText() const {
  StringBuilder result;
  result.Append("@viewport { ");

  String decls = viewport_rule_->Properties().AsText();
  result.Append(decls);
  if (!decls.IsEmpty())
    result.Append(' ');

  result.Append('}');

  return result.ToString();
}

void CSSViewportRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  viewport_rule_ = ToStyleRuleViewport(rule);
  if (properties_cssom_wrapper_)
    properties_cssom_wrapper_->Reattach(viewport_rule_->MutableProperties());
}

void CSSViewportRule::Trace(blink::Visitor* visitor) {
  visitor->Trace(viewport_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
