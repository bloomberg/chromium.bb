/*
 * Copyright (C) 2006, 2011, 2012 Apple Computer, Inc.
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

#include "config.h"
#include "core/html/HTMLOptionsCollection.h"

#include "core/dom/ExceptionCode.h"
#include "core/dom/NamedNodesCollection.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"

namespace WebCore {

HTMLOptionsCollection::HTMLOptionsCollection(Node* select)
    : HTMLCollection(select, SelectOptions, DoesNotOverrideItemAfter)
{
    ASSERT(select->hasTagName(HTMLNames::selectTag));
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLOptionsCollection> HTMLOptionsCollection::create(Node* select, CollectionType)
{
    return adoptRef(new HTMLOptionsCollection(select));
}

void HTMLOptionsCollection::add(PassRefPtr<HTMLOptionElement> element, ExceptionCode& ec)
{
    add(element, length(), ec);
}

void HTMLOptionsCollection::add(PassRefPtr<HTMLOptionElement> element, int index, ExceptionCode& ec)
{
    HTMLOptionElement* newOption = element.get();

    if (!newOption) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }

    if (index < -1) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    ec = 0;
    HTMLSelectElement* select = toHTMLSelectElement(ownerNode());

    if (index == -1 || unsigned(index) >= length())
        select->add(newOption, 0, ec);
    else
        select->add(newOption, toHTMLOptionElement(item(index)), ec);

    ASSERT(!ec);
}

void HTMLOptionsCollection::remove(int index)
{
    toHTMLSelectElement(ownerNode())->remove(index);
}

int HTMLOptionsCollection::selectedIndex() const
{
    return toHTMLSelectElement(ownerNode())->selectedIndex();
}

void HTMLOptionsCollection::setSelectedIndex(int index)
{
    toHTMLSelectElement(ownerNode())->setSelectedIndex(index);
}

void HTMLOptionsCollection::setLength(unsigned length, ExceptionCode& ec)
{
    toHTMLSelectElement(ownerNode())->setLength(length, ec);
}

void HTMLOptionsCollection::anonymousNamedGetter(const AtomicString& name, bool& returnValue0Enabled, RefPtr<NodeList>& returnValue0, bool& returnValue1Enabled, RefPtr<Node>& returnValue1)
{
    Vector<RefPtr<Node> > namedItems;
    this->namedItems(name, namedItems);

    if (!namedItems.size())
        return;

    if (namedItems.size() == 1) {
        returnValue1Enabled = true;
        returnValue1 = namedItems.at(0);
        return;
    }

    returnValue0Enabled = true;
    returnValue0 = NamedNodesCollection::create(namedItems);
}

bool HTMLOptionsCollection::anonymousIndexedSetterRemove(unsigned index, ExceptionCode& ec)
{
    HTMLSelectElement* base = toHTMLSelectElement(ownerNode());
    base->remove(index);
    return true;
}

bool HTMLOptionsCollection::anonymousIndexedSetter(unsigned index, PassRefPtr<HTMLOptionElement> value, ExceptionCode& ec)
{
    HTMLSelectElement* base = toHTMLSelectElement(ownerNode());
    if (!value) {
        ec = TYPE_MISMATCH_ERR;
        return true;
    }
    base->setOption(index, value.get(), ec);
    return true;
}

} //namespace

