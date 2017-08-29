/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef CSSComputedStyleDeclaration_h
#define CSSComputedStyleDeclaration_h

#include "core/CoreExport.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSVariableData;
class ExceptionState;
class LayoutObject;
class MutableStylePropertySet;
class Node;
class ComputedStyle;

class CORE_EXPORT CSSComputedStyleDeclaration final
    : public CSSStyleDeclaration {
 public:
  static CSSComputedStyleDeclaration* Create(
      Node* node,
      bool allow_visited_style = false,
      const String& pseudo_element_name = String()) {
    return new CSSComputedStyleDeclaration(node, allow_visited_style,
                                           pseudo_element_name);
  }

  static const Vector<CSSPropertyID>& ComputableProperties();
  ~CSSComputedStyleDeclaration() override;

  String GetPropertyValue(CSSPropertyID) const;
  bool GetPropertyPriority(CSSPropertyID) const;

  MutableStylePropertySet* CopyProperties() const;

  const CSSValue* GetPropertyCSSValue(CSSPropertyID) const;
  const CSSValue* GetPropertyCSSValue(AtomicString custom_property_name) const;
  std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>> GetVariables()
      const;

  const CSSValue* GetFontSizeCSSValuePreferringKeyword() const;
  bool IsMonospaceFont() const;

  MutableStylePropertySet* CopyPropertiesInSet(
      const Vector<CSSPropertyID>&) const;

  // CSSOM functions.
  unsigned length() const override;
  String item(unsigned index) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  CSSComputedStyleDeclaration(Node*, bool allow_visited_style, const String&);

  // The styled node is either the node passed into getComputedStyle, or the
  // PseudoElement for :before and :after if they exist.
  // FIXME: This should be styledElement since in JS getComputedStyle only works
  // on Elements, but right now editing creates these for text nodes. We should
  // fix that.
  Node* StyledNode() const;

  // The styled layout object is the layout object corresponding to the node
  // being queried, if any.
  LayoutObject* StyledLayoutObject() const;

  // CSSOM functions.
  CSSRule* parentRule() const override;
  const ComputedStyle* ComputeComputedStyle() const;
  String getPropertyValue(const String& property_name) override;
  String getPropertyPriority(const String& property_name) override;
  String GetPropertyShorthand(const String& property_name) override;
  bool IsPropertyImplicit(const String& property_name) override;
  void setProperty(const String& property_name,
                   const String& value,
                   const String& priority,
                   ExceptionState&) override;
  String removeProperty(const String& property_name, ExceptionState&) override;
  String CssFloat() const;
  void SetCSSFloat(const String&, ExceptionState&);
  String cssText() const override;
  void setCSSText(const String&, ExceptionState&) override;
  const CSSValue* GetPropertyCSSValueInternal(CSSPropertyID) override;
  const CSSValue* GetPropertyCSSValueInternal(
      AtomicString custom_property_name) override;
  String GetPropertyValueInternal(CSSPropertyID) override;
  void SetPropertyInternal(CSSPropertyID,
                           const String& custom_property_name,
                           const String& value,
                           bool important,
                           ExceptionState&) override;

  bool CssPropertyMatches(CSSPropertyID, const CSSValue*) const override;

  Member<Node> node_;
  PseudoId pseudo_element_specifier_;
  bool allow_visited_style_;
};

}  // namespace blink

#endif  // CSSComputedStyleDeclaration_h
