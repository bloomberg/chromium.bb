/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef CSSStyleRule_h
#define CSSStyleRule_h

#include "core/css/CSSRule.h"
#include "core/css/cssom/StylePropertyMap.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSStyleDeclaration;
class ExecutionContext;
class StyleRuleCSSStyleDeclaration;
class StyleRule;

class CORE_EXPORT CSSStyleRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSStyleRule* Create(StyleRule* rule, CSSStyleSheet* sheet) {
    return new CSSStyleRule(rule, sheet);
  }

  ~CSSStyleRule() override;

  String cssText() const override;
  void Reattach(StyleRuleBase*) override;

  String selectorText() const;
  void setSelectorText(const ExecutionContext*, const String&);

  CSSStyleDeclaration* style() const;

  StylePropertyMap* attributeStyleMap() const {
    return attribute_style_map_.Get();
  }

  // FIXME: Not CSSOM. Remove.
  StyleRule* GetStyleRule() const { return style_rule_.Get(); }

  virtual void Trace(blink::Visitor*);

 private:
  CSSStyleRule(StyleRule*, CSSStyleSheet*);

  CSSRule::Type type() const override { return kStyleRule; }

  Member<StyleRule> style_rule_;
  mutable Member<StyleRuleCSSStyleDeclaration> properties_cssom_wrapper_;
  Member<StylePropertyMap> attribute_style_map_;
};

DEFINE_CSS_RULE_TYPE_CASTS(CSSStyleRule, kStyleRule);

}  // namespace blink

#endif  // CSSStyleRule_h
