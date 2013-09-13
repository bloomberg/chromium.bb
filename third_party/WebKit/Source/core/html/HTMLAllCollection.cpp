/*
 * Copyright (C) 2009, 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/HTMLAllCollection.h"

#include "core/dom/Element.h"
#include "core/dom/NamedNodesCollection.h"

namespace WebCore {

PassRefPtr<HTMLAllCollection> HTMLAllCollection::create(Node* node, CollectionType type)
{
    return adoptRef(new HTMLAllCollection(node, type));
}

HTMLAllCollection::HTMLAllCollection(Node* node, CollectionType type)
    : HTMLCollection(node, type, DoesNotOverrideItemAfter)
{
    ScriptWrappable::init(this);
}

HTMLAllCollection::~HTMLAllCollection()
{
}

Node* HTMLAllCollection::namedItemWithIndex(const AtomicString& name, unsigned index) const
{
    updateNameCache();

    if (Vector<Element*>* cache = idCache(name)) {
        if (index < cache->size())
            return cache->at(index);
        index -= cache->size();
    }

    if (Vector<Element*>* cache = nameCache(name)) {
        if (index < cache->size())
            return cache->at(index);
    }

    return 0;
}

void HTMLAllCollection::anonymousNamedGetter(const AtomicString& name, bool& returnValue0Enabled, RefPtr<NodeList>& returnValue0, bool& returnValue1Enabled, RefPtr<Node>& returnValue1)
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

    // FIXME: HTML5 specification says this should be a HTMLCollection.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmlallcollection
    returnValue0Enabled = true;
    returnValue0 = NamedNodesCollection::create(namedItems);
}

PassRefPtr<NodeList> HTMLAllCollection::tags(const String& name)
{
    return ownerNode()->getElementsByTagName(name);
}

} // namespace WebCore
