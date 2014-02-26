/*
 * Copyright (C) 2008, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/HTMLTableRowsCollection.h"

#include "HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/HTMLTableRowElement.h"

namespace WebCore {

using namespace HTMLNames;

static bool isInHead(Element* row)
{
    return row->parentNode() && toElement(row->parentNode())->hasLocalName(theadTag);
}

static bool isInBody(Element* row)
{
    return row->parentNode() && toElement(row->parentNode())->hasLocalName(tbodyTag);
}

static bool isInFoot(Element* row)
{
    return row->parentNode() && toElement(row->parentNode())->hasLocalName(tfootTag);
}

static inline HTMLTableRowElement* findTableRowElementInChildren(Element& current)
{
    for (Element* child = ElementTraversal::firstWithin(current); child; child = ElementTraversal::nextSibling(*child)) {
        if (isHTMLTableRowElement(child))
            return toHTMLTableRowElement(child);
    }
    return 0;
}

HTMLTableRowElement* HTMLTableRowsCollection::rowAfter(HTMLTableElement& table, HTMLTableRowElement* previous)
{
    Element* child = 0;

    // Start by looking for the next row in this section.
    // Continue only if there is none.
    if (previous && previous->parentNode() != table) {
        for (child = ElementTraversal::nextSibling(*previous); child; child = ElementTraversal::nextSibling(*child)) {
            if (isHTMLTableRowElement(child))
                return toHTMLTableRowElement(child);
        }
    }

    // If still looking at head sections, find the first row in the next head section.
    if (!previous)
        child = ElementTraversal::firstWithin(table);
    else if (isInHead(previous))
        child = ElementTraversal::nextSibling(*previous->parentNode());
    for (; child; child = ElementTraversal::nextSibling(*child)) {
        if (child->hasTagName(theadTag)) {
            if (HTMLTableRowElement* row = findTableRowElementInChildren(*child))
                return row;
        }
    }

    // If still looking at top level and bodies, find the next row in top level or the first in the next body section.
    if (!previous || isInHead(previous))
        child = ElementTraversal::firstWithin(table);
    else if (previous->parentNode() == table)
        child = ElementTraversal::nextSibling(*previous);
    else if (isInBody(previous))
        child = ElementTraversal::nextSibling(*previous->parentNode());
    for (; child; child = ElementTraversal::nextSibling(*child)) {
        if (isHTMLTableRowElement(child))
            return toHTMLTableRowElement(child);
        if (child->hasTagName(tbodyTag)) {
            if (HTMLTableRowElement* row = findTableRowElementInChildren(*child))
                return row;
        }
    }

    // Find the first row in the next foot section.
    if (!previous || !isInFoot(previous))
        child = ElementTraversal::firstWithin(table);
    else
        child = ElementTraversal::nextSibling(*previous->parentNode());
    for (; child; child = ElementTraversal::nextSibling(*child)) {
        if (child->hasTagName(tfootTag)) {
            if (HTMLTableRowElement* row = findTableRowElementInChildren(*child))
                return row;
        }
    }

    return 0;
}

HTMLTableRowElement* HTMLTableRowsCollection::lastRow(HTMLTableElement& table)
{
    for (Node* child = table.lastChild(); child; child = child->previousSibling()) {
        if (child->hasTagName(tfootTag)) {
            for (Node* grandchild = child->lastChild(); grandchild; grandchild = grandchild->previousSibling()) {
                if (isHTMLTableRowElement(grandchild))
                    return toHTMLTableRowElement(grandchild);
            }
        }
    }

    for (Node* child = table.lastChild(); child; child = child->previousSibling()) {
        if (isHTMLTableRowElement(child))
            return toHTMLTableRowElement(child);
        if (child->hasTagName(tbodyTag)) {
            for (Node* grandchild = child->lastChild(); grandchild; grandchild = grandchild->previousSibling()) {
                if (isHTMLTableRowElement(grandchild))
                    return toHTMLTableRowElement(grandchild);
            }
        }
    }

    for (Node* child = table.lastChild(); child; child = child->previousSibling()) {
        if (child->hasTagName(theadTag)) {
            for (Node* grandchild = child->lastChild(); grandchild; grandchild = grandchild->previousSibling()) {
                if (isHTMLTableRowElement(grandchild))
                    return toHTMLTableRowElement(grandchild);
            }
        }
    }

    return 0;
}

// Must call get() on the table in case that argument is compiled before dereferencing the
// table to get at the collection cache. Order of argument evaluation is undefined and can
// differ between compilers.
HTMLTableRowsCollection::HTMLTableRowsCollection(ContainerNode* table)
    : HTMLCollection(table, TableRows, OverridesItemAfter)
{
    ASSERT(isHTMLTableElement(table));
}

PassRefPtr<HTMLTableRowsCollection> HTMLTableRowsCollection::create(ContainerNode* table, CollectionType)
{
    return adoptRef(new HTMLTableRowsCollection(table));
}

Element* HTMLTableRowsCollection::virtualItemAfter(Element* previous) const
{
    return rowAfter(toHTMLTableElement(ownerNode()), toHTMLTableRowElement(previous));
}

}
