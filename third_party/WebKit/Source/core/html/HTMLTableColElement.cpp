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

#include "core/html/HTMLTableColElement.h"

#include <algorithm>
#include "core/CSSPropertyNames.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/TableConstants.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html_names.h"
#include "core/layout/LayoutTableCol.h"

namespace blink {

using namespace HTMLNames;

inline HTMLTableColElement::HTMLTableColElement(const QualifiedName& tag_name,
                                                Document& document)
    : HTMLTablePartElement(tag_name, document), span_(kDefaultColSpan) {}

DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(HTMLTableColElement)

bool HTMLTableColElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr)
    return true;
  return HTMLTablePartElement::IsPresentationAttribute(name);
}

void HTMLTableColElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == widthAttr)
    AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  else
    HTMLTablePartElement::CollectStyleForPresentationAttribute(name, value,
                                                               style);
}

void HTMLTableColElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == spanAttr) {
    unsigned new_span = 0;
    if (!ParseHTMLClampedNonNegativeInteger(params.new_value, kMinColSpan,
                                            kMaxColSpan, new_span)) {
      new_span = kDefaultColSpan;
    }
    span_ = new_span;
    if (GetLayoutObject() && GetLayoutObject()->IsLayoutTableCol())
      GetLayoutObject()->UpdateFromElement();
  } else if (params.name == widthAttr) {
    if (!params.new_value.IsEmpty()) {
      if (GetLayoutObject() && GetLayoutObject()->IsLayoutTableCol()) {
        LayoutTableCol* col = ToLayoutTableCol(GetLayoutObject());
        int new_width = Width().ToInt();
        if (new_width != col->Size().Width())
          col->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
              LayoutInvalidationReason::kAttributeChanged);
      }
    }
  } else {
    HTMLTablePartElement::ParseAttribute(params);
  }
}

const StylePropertySet*
HTMLTableColElement::AdditionalPresentationAttributeStyle() {
  if (!HasTagName(colgroupTag))
    return nullptr;
  if (HTMLTableElement* table = FindParentTable())
    return table->AdditionalGroupStyle(false);
  return nullptr;
}

void HTMLTableColElement::setSpan(unsigned n) {
  SetUnsignedIntegralAttribute(spanAttr, n, kDefaultColSpan);
}

const AtomicString& HTMLTableColElement::Width() const {
  return getAttribute(widthAttr);
}

}  // namespace blink
