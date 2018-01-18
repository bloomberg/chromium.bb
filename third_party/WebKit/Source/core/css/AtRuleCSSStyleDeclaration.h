// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AtRuleCSSStyleDeclaration_h
#define AtRuleCSSStyleDeclaration_h

#include "core/css/AtRuleDescriptorSerializer.h"
#include "core/css/CSSFontFaceRule.h"
#include "core/css/CSSRule.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/parser/AtRuleDescriptorValueSet.h"

namespace blink {

/**
 * AtRuleCSSStyleDeclaration is a subclass of CSSStyleDeclaration that is used
 * specifically for the .style attribute of the JavaScript FontFace object.
 *
 * Since @rule related objects use AtRuleDescriptorID instead of CSSProperty or
 * CSSPropertyID, it's convenient to use a separate CSSStyleDeclaration
 * subclass.
 *
 * Spec here: https://drafts.csswg.org/css-fonts-4/#om-fontface
 */
class CORE_EXPORT AtRuleCSSStyleDeclaration : public CSSStyleDeclaration {
 public:
  static AtRuleCSSStyleDeclaration* Create(AtRuleDescriptorValueSet* set,
                                           CSSFontFaceRule* parent_rule) {
    return new AtRuleCSSStyleDeclaration(set, parent_rule);
  }

  CSSRule* parentRule() const final { return parent_rule_.Get(); }

  String cssText() const final;
  void setCSSText(const ExecutionContext*,
                  const String&,
                  ExceptionState&) final;

  unsigned length() const final;

  String item(unsigned index) const final;

  String getPropertyValue(const String& property_name) final;
  String getPropertyPriority(const String& property_name) final;
  String GetPropertyShorthand(const String& property_name) final;
  bool IsPropertyImplicit(const String& property_name) final;

  void setProperty(const ExecutionContext*,
                   const String& property_name,
                   const String& value,
                   const String& priority,
                   ExceptionState&) final;
  String removeProperty(const String& property_name, ExceptionState&) final;

  void Reattach(AtRuleDescriptorValueSet&);

  // To support bindings and editing. See CSSStyleDeclaration.h for details.
  // TODO(meade): Detangle this from using CSSPropertyID when handling @-rules.
  const CSSValue* GetPropertyCSSValueInternal(CSSPropertyID) final;
  String GetPropertyValueInternal(CSSPropertyID) final;
  const CSSValue* GetPropertyCSSValueInternal(
      AtomicString custom_property_name) final {
    NOTREACHED();
    return nullptr;
  }
  void SetPropertyInternal(CSSPropertyID,
                           const String& custom_property_name,
                           const String& value,
                           bool important,
                           SecureContextMode,
                           ExceptionState&);
  bool CssPropertyMatches(CSSPropertyID, const CSSValue*) const final {
    return false;
  }

  void Trace(blink::Visitor*) override;

 private:
  AtRuleCSSStyleDeclaration(AtRuleDescriptorValueSet* set, CSSRule* parent_rule)
      : descriptor_value_set_(set), parent_rule_(parent_rule) {}

  void SetPropertyInternal(AtRuleDescriptorID,
                           const String& value,
                           SecureContextMode);

  Member<AtRuleDescriptorValueSet> descriptor_value_set_;
  TraceWrapperMember<CSSRule> parent_rule_;
};

}  // namespace blink

#endif  // AtRuleCSSStyleDeclaration_h
