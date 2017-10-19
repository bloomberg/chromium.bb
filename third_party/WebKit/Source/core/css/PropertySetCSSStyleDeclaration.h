/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PropertySetCSSStyleDeclaration_h
#define PropertySetCSSStyleDeclaration_h

#include "core/css/CSSStyleDeclaration.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class CSSRule;
class CSSValue;
class Element;
class ExceptionState;
class MutableStylePropertySet;
class PropertyRegistry;
class StyleSheetContents;

class AbstractPropertySetCSSStyleDeclaration : public CSSStyleDeclaration {
 public:
  virtual Element* ParentElement() const { return 0; }
  StyleSheetContents* ContextStyleSheet() const;

  virtual void Trace(blink::Visitor*);

 private:
  CSSRule* parentRule() const override { return 0; }
  unsigned length() const final;
  String item(unsigned index) const final;
  String getPropertyValue(const String& property_name) final;
  String getPropertyPriority(const String& property_name) final;
  String GetPropertyShorthand(const String& property_name) final;
  bool IsPropertyImplicit(const String& property_name) final;
  void setProperty(const String& property_name,
                   const String& value,
                   const String& priority,
                   ExceptionState&) final;
  String removeProperty(const String& property_name, ExceptionState&) final;
  String CssFloat() const;
  void SetCSSFloat(const String&, ExceptionState&);
  String cssText() const final;
  void setCSSText(const String&, ExceptionState&) final;
  const CSSValue* GetPropertyCSSValueInternal(CSSPropertyID) final;
  const CSSValue* GetPropertyCSSValueInternal(
      AtomicString custom_property_name) final;
  String GetPropertyValueInternal(CSSPropertyID) final;
  void SetPropertyInternal(CSSPropertyID,
                           const String& custom_property_name,
                           const String& value,
                           bool important,
                           ExceptionState&) final;

  bool CssPropertyMatches(CSSPropertyID, const CSSValue*) const final;

 protected:
  enum MutationType { kNoChanges, kPropertyChanged };
  virtual void WillMutate() {}
  virtual void DidMutate(MutationType) {}
  virtual MutableStylePropertySet& PropertySet() const = 0;
  virtual PropertyRegistry* GetPropertyRegistry() const = 0;
  virtual bool IsKeyframeStyle() const { return false; }
};

class PropertySetCSSStyleDeclaration
    : public AbstractPropertySetCSSStyleDeclaration {
 public:
  PropertySetCSSStyleDeclaration(MutableStylePropertySet& property_set)
      : property_set_(&property_set) {}

  virtual void Trace(blink::Visitor*);

 protected:
  MutableStylePropertySet& PropertySet() const final {
    DCHECK(property_set_);
    return *property_set_;
  }

  PropertyRegistry* GetPropertyRegistry() const override { return nullptr; }

  Member<MutableStylePropertySet> property_set_;  // Cannot be null
};

class StyleRuleCSSStyleDeclaration : public PropertySetCSSStyleDeclaration {
 public:
  static StyleRuleCSSStyleDeclaration* Create(
      MutableStylePropertySet& property_set,
      CSSRule* parent_rule) {
    return new StyleRuleCSSStyleDeclaration(property_set, parent_rule);
  }

  void Reattach(MutableStylePropertySet&);

  virtual void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

 protected:
  StyleRuleCSSStyleDeclaration(MutableStylePropertySet&, CSSRule*);
  ~StyleRuleCSSStyleDeclaration() override;

  CSSStyleSheet* ParentStyleSheet() const override;

  CSSRule* parentRule() const override { return parent_rule_; }

  void WillMutate() override;
  void DidMutate(MutationType) override;
  PropertyRegistry* GetPropertyRegistry() const final;

  TraceWrapperMember<CSSRule> parent_rule_;
};

class InlineCSSStyleDeclaration final
    : public AbstractPropertySetCSSStyleDeclaration {
 public:
  explicit InlineCSSStyleDeclaration(Element* parent_element)
      : parent_element_(parent_element) {}

  virtual void Trace(blink::Visitor*);

 private:
  MutableStylePropertySet& PropertySet() const override;
  CSSStyleSheet* ParentStyleSheet() const override;
  Element* ParentElement() const override { return parent_element_; }

  void DidMutate(MutationType) override;
  PropertyRegistry* GetPropertyRegistry() const final;

  Member<Element> parent_element_;
};

}  // namespace blink

#endif
