/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/DocumentOrderedMap.h"

#include "HTMLNames.h"
#include "core/dom/Element.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/TreeScope.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLMapElement.h"
#include "core/html/HTMLNameCollection.h"

namespace WebCore {

using namespace HTMLNames;

inline bool keyMatchesId(StringImpl* key, Element* element)
{
    return element->getIdAttribute().impl() == key;
}

inline bool keyMatchesName(StringImpl* key, Element* element)
{
    return element->getNameAttribute().impl() == key;
}

inline bool keyMatchesMapName(StringImpl* key, Element* element)
{
    return element->hasTagName(mapTag) && toHTMLMapElement(element)->getName().impl() == key;
}

inline bool keyMatchesLowercasedMapName(StringImpl* key, Element* element)
{
    return element->hasTagName(mapTag) && toHTMLMapElement(element)->getName().lower().impl() == key;
}

inline bool keyMatchesLabelForAttribute(StringImpl* key, Element* element)
{
    return isHTMLLabelElement(element) && element->getAttribute(forAttr).impl() == key;
}

void DocumentOrderedMap::clear()
{
    m_map.clear();
}

void DocumentOrderedMap::add(StringImpl* key, Element* element)
{
    ASSERT(key);
    ASSERT(element);

    Map::AddResult addResult = m_map.add(key, MapEntry(element));
    if (addResult.isNewEntry)
        return;

    MapEntry& entry = addResult.iterator->value;
    ASSERT(entry.count);
    entry.element = 0;
    entry.count++;
    entry.orderedList.clear();
}

void DocumentOrderedMap::remove(StringImpl* key, Element* element)
{
    ASSERT(key);
    ASSERT(element);

    Map::iterator it = m_map.find(key);
    ASSERT(it != m_map.end());
    MapEntry& entry = it->value;

    ASSERT(entry.count);
    if (entry.count == 1) {
        ASSERT(!entry.element || entry.element == element);
        m_map.remove(it);
    } else {
        if (entry.element == element)
            entry.element = 0;
        entry.count--;
        entry.orderedList.clear(); // FIXME: Remove the element instead if there are only few items left.
    }
}

template<bool keyMatches(StringImpl*, Element*)>
inline Element* DocumentOrderedMap::get(StringImpl* key, const TreeScope* scope) const
{
    ASSERT(key);
    ASSERT(scope);

    Map::iterator it = m_map.find(key);
    if (it == m_map.end())
        return 0;

    MapEntry& entry = it->value;
    ASSERT(entry.count);
    if (entry.element)
        return entry.element;

    // We know there's at least one node that matches; iterate to find the first one.
    for (Element* element = ElementTraversal::firstWithin(scope->rootNode()); element; element = ElementTraversal::next(element)) {
        if (!keyMatches(key, element))
            continue;
        entry.element = element;
        return element;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

Element* DocumentOrderedMap::getElementById(StringImpl* key, const TreeScope* scope) const
{
    return get<keyMatchesId>(key, scope);
}

Element* DocumentOrderedMap::getElementByName(StringImpl* key, const TreeScope* scope) const
{
    return get<keyMatchesName>(key, scope);
}

Element* DocumentOrderedMap::getElementByMapName(StringImpl* key, const TreeScope* scope) const
{
    return get<keyMatchesMapName>(key, scope);
}

Element* DocumentOrderedMap::getElementByLowercasedMapName(StringImpl* key, const TreeScope* scope) const
{
    return get<keyMatchesLowercasedMapName>(key, scope);
}

Element* DocumentOrderedMap::getElementByLabelForAttribute(StringImpl* key, const TreeScope* scope) const
{
    return get<keyMatchesLabelForAttribute>(key, scope);
}

const Vector<Element*>* DocumentOrderedMap::getAllElementsById(StringImpl* key, const TreeScope* scope) const
{
    ASSERT(key);
    ASSERT(scope);

    Map::iterator it = m_map.find(key);
    if (it == m_map.end())
        return 0;

    MapEntry& entry = it->value;
    ASSERT(entry.count);
    if (!entry.count)
        return 0;

    if (entry.orderedList.isEmpty()) {
        entry.orderedList.reserveCapacity(entry.count);
        for (Element* element = entry.element ? entry.element : ElementTraversal::firstWithin(scope->rootNode()); element; element = ElementTraversal::next(element)) {
            if (!keyMatchesId(key, element))
                continue;
            entry.orderedList.append(element);
        }
        ASSERT(entry.orderedList.size() == entry.count);
    }

    return &entry.orderedList;
}

} // namespace WebCore
