/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#include "core/html/HTMLTableCellElement.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutTableCell.h"

namespace blink {

using namespace HTMLNames;

inline HTMLTableCellElement::HTMLTableCellElement(const QualifiedName& tagName,
                                                  Document& document)
    : HTMLTablePartElement(tagName, document) {}

DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(HTMLTableCellElement)

unsigned HTMLTableCellElement::colSpan() const {
  const AtomicString& colSpanValue = fastGetAttribute(colspanAttr);
  unsigned value = 0;
  if (colSpanValue.isEmpty() ||
      !parseHTMLNonNegativeInteger(colSpanValue, value))
    return 1;
  // Counting for https://github.com/whatwg/html/issues/1198
  UseCounter::count(document(), UseCounter::HTMLTableCellElementColspan);
  if (value > 8190) {
    UseCounter::count(document(),
                      UseCounter::HTMLTableCellElementColspanGreaterThan8190);
  } else if (value > 1000) {
    UseCounter::count(document(),
                      UseCounter::HTMLTableCellElementColspanGreaterThan1000);
  }
  return std::max(1u, std::min(value, maxColSpan()));
}

unsigned HTMLTableCellElement::rowSpan() const {
  const AtomicString& rowSpanValue = fastGetAttribute(rowspanAttr);
  unsigned value = 0;
  if (rowSpanValue.isEmpty() ||
      !parseHTMLNonNegativeInteger(rowSpanValue, value))
    return 1;
  return std::max(1u, std::min(value, maxRowSpan()));
}

int HTMLTableCellElement::cellIndex() const {
  if (!isHTMLTableRowElement(parentElement()))
    return -1;

  int index = 0;
  for (const HTMLTableCellElement* element =
           Traversal<HTMLTableCellElement>::previousSibling(*this);
       element;
       element = Traversal<HTMLTableCellElement>::previousSibling(*element))
    ++index;

  return index;
}

bool HTMLTableCellElement::isPresentationAttribute(
    const QualifiedName& name) const {
  if (name == nowrapAttr || name == widthAttr || name == heightAttr)
    return true;
  return HTMLTablePartElement::isPresentationAttribute(name);
}

void HTMLTableCellElement::collectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == nowrapAttr) {
    addPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace,
                                            CSSValueWebkitNowrap);
  } else if (name == widthAttr) {
    if (!value.isEmpty()) {
      int widthInt = value.toInt();
      if (widthInt > 0)  // width="0" is ignored for compatibility with WinIE.
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    }
  } else if (name == heightAttr) {
    if (!value.isEmpty()) {
      int heightInt = value.toInt();
      if (heightInt > 0)  // height="0" is ignored for compatibility with WinIE.
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    }
  } else {
    HTMLTablePartElement::collectStyleForPresentationAttribute(name, value,
                                                               style);
  }
}

void HTMLTableCellElement::parseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == rowspanAttr || params.name == colspanAttr) {
    if (layoutObject() && layoutObject()->isTableCell())
      toLayoutTableCell(layoutObject())->colSpanOrRowSpanChanged();
  } else {
    HTMLTablePartElement::parseAttribute(params);
  }
}

const StylePropertySet*
HTMLTableCellElement::additionalPresentationAttributeStyle() {
  if (HTMLTableElement* table = findParentTable())
    return table->additionalCellStyle();
  return nullptr;
}

bool HTMLTableCellElement::isURLAttribute(const Attribute& attribute) const {
  return attribute.name() == backgroundAttr ||
         HTMLTablePartElement::isURLAttribute(attribute);
}

bool HTMLTableCellElement::hasLegalLinkAttribute(
    const QualifiedName& name) const {
  return (hasTagName(tdTag) && name == backgroundAttr) ||
         HTMLTablePartElement::hasLegalLinkAttribute(name);
}

const QualifiedName& HTMLTableCellElement::subResourceAttributeName() const {
  return hasTagName(tdTag) ? backgroundAttr
                           : HTMLTablePartElement::subResourceAttributeName();
}

const AtomicString& HTMLTableCellElement::abbr() const {
  return fastGetAttribute(abbrAttr);
}

const AtomicString& HTMLTableCellElement::axis() const {
  return fastGetAttribute(axisAttr);
}

void HTMLTableCellElement::setColSpan(unsigned n) {
  setUnsignedIntegralAttribute(colspanAttr, n);
}

const AtomicString& HTMLTableCellElement::headers() const {
  return fastGetAttribute(headersAttr);
}

void HTMLTableCellElement::setRowSpan(unsigned n) {
  setUnsignedIntegralAttribute(rowspanAttr, n);
}

}  // namespace blink
