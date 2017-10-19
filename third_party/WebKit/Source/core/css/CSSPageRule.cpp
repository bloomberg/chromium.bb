/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/css/CSSPageRule.h"

#include "core/css/CSSSelector.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/parser/CSSParser.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

CSSPageRule::CSSPageRule(StyleRulePage* page_rule, CSSStyleSheet* parent)
    : CSSRule(parent), page_rule_(page_rule) {}

CSSPageRule::~CSSPageRule() {}

CSSStyleDeclaration* CSSPageRule::style() const {
  if (!properties_cssom_wrapper_)
    properties_cssom_wrapper_ = StyleRuleCSSStyleDeclaration::Create(
        page_rule_->MutableProperties(), const_cast<CSSPageRule*>(this));
  return properties_cssom_wrapper_.Get();
}

String CSSPageRule::selectorText() const {
  StringBuilder text;
  const CSSSelector* selector = page_rule_->Selector();
  if (selector) {
    String page_specification = selector->SelectorText();
    if (!page_specification.IsEmpty())
      text.Append(page_specification);
  }
  return text.ToString();
}

void CSSPageRule::setSelectorText(const String& selector_text) {
  CSSParserContext* context =
      CSSParserContext::Create(ParserContext(), nullptr);
  DCHECK(context);
  CSSSelectorList selector_list = CSSParser::ParsePageSelector(
      *context, parentStyleSheet() ? parentStyleSheet()->Contents() : nullptr,
      selector_text);
  if (!selector_list.IsValid())
    return;

  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  page_rule_->WrapperAdoptSelectorList(std::move(selector_list));
}

String CSSPageRule::cssText() const {
  StringBuilder result;
  result.Append("@page ");
  String page_selectors = selectorText();
  result.Append(page_selectors);
  if (!page_selectors.IsEmpty())
    result.Append(' ');
  result.Append("{ ");
  String decls = page_rule_->Properties().AsText();
  result.Append(decls);
  if (!decls.IsEmpty())
    result.Append(' ');
  result.Append('}');
  return result.ToString();
}

void CSSPageRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  page_rule_ = ToStyleRulePage(rule);
  if (properties_cssom_wrapper_)
    properties_cssom_wrapper_->Reattach(page_rule_->MutableProperties());
}

void CSSPageRule::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
