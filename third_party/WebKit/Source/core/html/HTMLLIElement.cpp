/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2006, 2007, 2010 Apple Inc. All rights reserved.
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
 *
 */

#include "core/html/HTMLLIElement.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/dom/LayoutTreeBuilderTraversal.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutListItem.h"
#include "core/layout/api/LayoutLIItem.h"

namespace blink {

using namespace HTMLNames;

inline HTMLLIElement::HTMLLIElement(Document& document)
    : HTMLElement(liTag, document) {}

DEFINE_NODE_FACTORY(HTMLLIElement)

bool HTMLLIElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == typeAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

CSSValueID ListTypeToCSSValueID(const AtomicString& value) {
  if (value == "a")
    return CSSValueLowerAlpha;
  if (value == "A")
    return CSSValueUpperAlpha;
  if (value == "i")
    return CSSValueLowerRoman;
  if (value == "I")
    return CSSValueUpperRoman;
  if (value == "1")
    return CSSValueDecimal;
  if (DeprecatedEqualIgnoringCase(value, "disc"))
    return CSSValueDisc;
  if (DeprecatedEqualIgnoringCase(value, "circle"))
    return CSSValueCircle;
  if (DeprecatedEqualIgnoringCase(value, "square"))
    return CSSValueSquare;
  if (DeprecatedEqualIgnoringCase(value, "none"))
    return CSSValueNone;
  return CSSValueInvalid;
}

void HTMLLIElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == typeAttr) {
    CSSValueID type_value = ListTypeToCSSValueID(value);
    if (type_value != CSSValueInvalid)
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyListStyleType,
                                              type_value);
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLLIElement::ParseAttribute(const AttributeModificationParams& params) {
  if (params.name == valueAttr) {
    if (GetLayoutObject() && GetLayoutObject()->IsListItem())
      ParseValue(params.new_value);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

void HTMLLIElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);

  if (GetLayoutObject() && GetLayoutObject()->IsListItem()) {
    LayoutLIItem li_layout_item =
        LayoutLIItem(ToLayoutListItem(GetLayoutObject()));

    DCHECK(!GetDocument().ChildNeedsDistributionRecalc());

    // Find the enclosing list node.
    Element* list_node = 0;
    Element* current = this;
    while (!list_node) {
      current = LayoutTreeBuilderTraversal::ParentElement(*current);
      if (!current)
        break;
      if (isHTMLUListElement(*current) || isHTMLOListElement(*current))
        list_node = current;
    }

    // If we are not in a list, tell the layoutObject so it can position us
    // inside.  We don't want to change our style to say "inside" since that
    // would affect nested nodes.
    if (!list_node)
      li_layout_item.SetNotInList(true);

    ParseValue(FastGetAttribute(valueAttr));
  }
}

inline void HTMLLIElement::ParseValue(const AtomicString& value) {
  DCHECK(GetLayoutObject());
  DCHECK(GetLayoutObject()->IsListItem());

  int requested_value = 0;
  if (ParseHTMLInteger(value, requested_value))
    ToLayoutListItem(GetLayoutObject())->SetExplicitValue(requested_value);
  else
    ToLayoutListItem(GetLayoutObject())->ClearExplicitValue();
}

}  // namespace blink
