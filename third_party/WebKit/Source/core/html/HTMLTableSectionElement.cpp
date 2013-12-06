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

#include "config.h"
#include "core/html/HTMLTableSectionElement.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/HTMLTableRowElement.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLTableSectionElement::HTMLTableSectionElement(const QualifiedName& tagName, Document& document)
    : HTMLTablePartElement(tagName, document)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLTableSectionElement> HTMLTableSectionElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLTableSectionElement(tagName, document));
}

const StylePropertySet* HTMLTableSectionElement::additionalPresentationAttributeStyle()
{
    if (HTMLTableElement* table = findParentTable())
        return table->additionalGroupStyle(true);
    return 0;
}

// these functions are rather slow, since we need to get the row at
// the index... but they aren't used during usual HTML parsing anyway
PassRefPtr<HTMLElement> HTMLTableSectionElement::insertRow(int index, ExceptionState& exceptionState)
{
    RefPtr<HTMLTableRowElement> row;
    RefPtr<HTMLCollection> children = rows();
    int numRows = children ? (int)children->length() : 0;
    if (index < -1 || index > numRows)
        exceptionState.throwUninformativeAndGenericDOMException(IndexSizeError); // per the DOM
    else {
        row = HTMLTableRowElement::create(document());
        if (numRows == index || index == -1)
            appendChild(row, exceptionState);
        else {
            Node* n;
            if (index < 1)
                n = firstChild();
            else
                n = children->item(index);
            insertBefore(row, n, exceptionState);
        }
    }
    return row.release();
}

void HTMLTableSectionElement::deleteRow(int index, ExceptionState& exceptionState)
{
    RefPtr<HTMLCollection> children = rows();
    int numRows = children ? (int)children->length() : 0;
    if (index == -1)
        index = numRows - 1;
    if (index >= 0 && index < numRows) {
        RefPtr<Node> row = children->item(index);
        HTMLElement::removeChild(row.get(), exceptionState);
    } else {
        exceptionState.throwUninformativeAndGenericDOMException(IndexSizeError);
    }
}

int HTMLTableSectionElement::numRows() const
{
    int rows = 0;
    const Node *n = firstChild();
    while (n) {
        if (n->hasTagName(trTag))
            rows++;
        n = n->nextSibling();
    }

    return rows;
}

const AtomicString& HTMLTableSectionElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLTableSectionElement::setAlign(const AtomicString& value)
{
    setAttribute(alignAttr, value);
}

const AtomicString& HTMLTableSectionElement::ch() const
{
    return getAttribute(charAttr);
}

void HTMLTableSectionElement::setCh(const AtomicString& value)
{
    setAttribute(charAttr, value);
}

const AtomicString& HTMLTableSectionElement::chOff() const
{
    return getAttribute(charoffAttr);
}

void HTMLTableSectionElement::setChOff(const AtomicString& value)
{
    setAttribute(charoffAttr, value);
}

const AtomicString& HTMLTableSectionElement::vAlign() const
{
    return getAttribute(valignAttr);
}

void HTMLTableSectionElement::setVAlign(const AtomicString& value)
{
    setAttribute(valignAttr, value);
}

PassRefPtr<HTMLCollection> HTMLTableSectionElement::rows()
{
    return ensureCachedHTMLCollection(TSectionRows);
}

}
