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

#include "core/html/HTMLTableSectionElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/HTMLTableRowElement.h"
#include "core/html_names.h"

namespace blink {

using namespace HTMLNames;

inline HTMLTableSectionElement::HTMLTableSectionElement(
    const QualifiedName& tag_name,
    Document& document)
    : HTMLTablePartElement(tag_name, document) {}

DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(HTMLTableSectionElement)

const StylePropertySet*
HTMLTableSectionElement::AdditionalPresentationAttributeStyle() {
  if (HTMLTableElement* table = FindParentTable())
    return table->AdditionalGroupStyle(true);
  return nullptr;
}

// these functions are rather slow, since we need to get the row at
// the index... but they aren't used during usual HTML parsing anyway
HTMLElement* HTMLTableSectionElement::insertRow(
    int index,
    ExceptionState& exception_state) {
  HTMLCollection* children = rows();
  int num_rows = children ? static_cast<int>(children->length()) : 0;
  if (index < -1 || index > num_rows) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The provided index (" + String::Number(index) +
                             " is outside the range [-1, " +
                             String::Number(num_rows) + "].");
    return nullptr;
  }

  HTMLTableRowElement* row = HTMLTableRowElement::Create(GetDocument());
  if (num_rows == index || index == -1)
    AppendChild(row, exception_state);
  else
    InsertBefore(row, children->item(index), exception_state);
  return row;
}

void HTMLTableSectionElement::deleteRow(int index,
                                        ExceptionState& exception_state) {
  HTMLCollection* children = rows();
  int num_rows = children ? (int)children->length() : 0;
  if (index == -1) {
    if (!num_rows)
      return;
    index = num_rows - 1;
  }
  if (index >= 0 && index < num_rows) {
    Element* row = children->item(index);
    HTMLElement::RemoveChild(row, exception_state);
  } else {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The provided index (" + String::Number(index) +
                             " is outside the range [-1, " +
                             String::Number(num_rows) + "].");
  }
}

HTMLCollection* HTMLTableSectionElement::rows() {
  return EnsureCachedCollection<HTMLCollection>(kTSectionRows);
}

}  // namespace blink
