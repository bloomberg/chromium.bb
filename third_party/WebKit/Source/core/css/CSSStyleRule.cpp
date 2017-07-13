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

#include "core/css/CSSStyleRule.h"

#include "core/css/CSSSelector.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/parser/CSSParser.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using SelectorTextCache =
    PersistentHeapHashMap<WeakMember<const CSSStyleRule>, String>;

static SelectorTextCache& GetSelectorTextCache() {
  DEFINE_STATIC_LOCAL(SelectorTextCache, cache, ());
  return cache;
}

CSSStyleRule::CSSStyleRule(StyleRule* style_rule, CSSStyleSheet* parent)
    : CSSRule(parent), style_rule_(style_rule) {}

CSSStyleRule::~CSSStyleRule() {}

CSSStyleDeclaration* CSSStyleRule::style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ = StyleRuleCSSStyleDeclaration::Create(
        style_rule_->MutableProperties(), const_cast<CSSStyleRule*>(this));
  }
  return properties_cssom_wrapper_.Get();
}

String CSSStyleRule::selectorText() const {
  if (HasCachedSelectorText()) {
    DCHECK(GetSelectorTextCache().Contains(this));
    return GetSelectorTextCache().at(this);
  }

  DCHECK(!GetSelectorTextCache().Contains(this));
  String text = style_rule_->SelectorList().SelectorsText();
  GetSelectorTextCache().Set(this, text);
  SetHasCachedSelectorText(true);
  return text;
}

void CSSStyleRule::setSelectorText(const String& selector_text) {
  const CSSParserContext* context =
      CSSParserContext::Create(ParserContext(), nullptr);
  CSSSelectorList selector_list = CSSParser::ParseSelector(
      context, parentStyleSheet() ? parentStyleSheet()->Contents() : nullptr,
      selector_text);
  if (!selector_list.IsValid())
    return;

  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  style_rule_->WrapperAdoptSelectorList(std::move(selector_list));

  if (HasCachedSelectorText()) {
    GetSelectorTextCache().erase(this);
    SetHasCachedSelectorText(false);
  }
}

String CSSStyleRule::cssText() const {
  StringBuilder result;
  result.Append(selectorText());
  result.Append(" { ");
  String decls = style_rule_->Properties().AsText();
  result.Append(decls);
  if (!decls.IsEmpty())
    result.Append(' ');
  result.Append('}');
  return result.ToString();
}

void CSSStyleRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  style_rule_ = ToStyleRule(rule);
  if (properties_cssom_wrapper_)
    properties_cssom_wrapper_->Reattach(style_rule_->MutableProperties());
}

DEFINE_TRACE(CSSStyleRule) {
  visitor->Trace(style_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
