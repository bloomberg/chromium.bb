/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#ifndef HTMLCollection_h
#define HTMLCollection_h

#include "core/dom/LiveNodeListBase.h"
#include "core/html/CollectionIndexCache.h"
#include "core/html/CollectionType.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace WebCore {

class HTMLCollection : public ScriptWrappable, public RefCounted<HTMLCollection>, public LiveNodeListBase {
public:
    enum ItemAfterOverrideType {
        OverridesItemAfter,
        DoesNotOverrideItemAfter,
    };

    static PassRefPtr<HTMLCollection> create(ContainerNode* base, CollectionType);
    virtual ~HTMLCollection();
    virtual void invalidateCache(Document* oldDocument = 0) const OVERRIDE;

    // DOM API
    unsigned length() const { return m_collectionIndexCache.nodeCount(*this); }
    Element* item(unsigned offset) const { return m_collectionIndexCache.nodeAt(*this, offset); }
    virtual Element* namedItem(const AtomicString& name) const;
    bool namedPropertyQuery(const AtomicString&, ExceptionState&);
    void namedPropertyEnumerator(Vector<String>& names, ExceptionState&);

    // Non-DOM API
    void namedItems(const AtomicString& name, Vector<RefPtr<Element> >&) const;
    bool isEmpty() const { return m_collectionIndexCache.isEmpty(*this); }
    bool hasExactlyOneItem() const { return m_collectionIndexCache.hasExactlyOneNode(*this); }

    // CollectionIndexCache API.
    bool canTraverseBackward() const { return !overridesItemAfter(); }
    Element* itemBefore(const Element* previousItem) const;
    Element* traverseToFirstElement(const ContainerNode& root) const;
    Element* traverseForwardToOffset(unsigned offset, Element& currentElement, unsigned& currentOffset, const ContainerNode& root) const;

protected:
    HTMLCollection(ContainerNode* base, CollectionType, ItemAfterOverrideType);

    bool overridesItemAfter() const { return m_overridesItemAfter; }
    virtual Element* virtualItemAfter(Element*) const;
    bool shouldOnlyIncludeDirectChildren() const { return m_shouldOnlyIncludeDirectChildren; }
    virtual void supportedPropertyNames(Vector<String>& names);

    virtual void updateIdNameCache() const;
    bool hasValidIdNameCache() const { return m_hasValidIdNameCache; }
    void setHasValidIdNameCache() const
    {
        ASSERT(!m_hasValidIdNameCache);
        m_hasValidIdNameCache = true;
        document().incrementNodeListWithIdNameCacheCount();
    }

    typedef HashMap<StringImpl*, OwnPtr<Vector<Element*> > > NodeCacheMap;
    Vector<Element*>* idCache(const AtomicString& name) const { return m_idCache.get(name.impl()); }
    Vector<Element*>* nameCache(const AtomicString& name) const { return m_nameCache.get(name.impl()); }
    void appendIdCache(const AtomicString& name, Element* element) const { append(m_idCache, name, element); }
    void appendNameCache(const AtomicString& name, Element* element) const { append(m_nameCache, name, element); }

private:
    Element* traverseNextElement(Element& previous, const ContainerNode& root) const;

    static void append(NodeCacheMap&, const AtomicString&, Element*);
    void invalidateIdNameCacheMaps(Document* oldDocument = 0) const
    {
        if (!m_hasValidIdNameCache)
            return;

        // Make sure we decrement the NodeListWithIdNameCache count from
        // the old document instead of the new one in the case the collection
        // is moved to a new document.
        unregisterIdNameCacheFromDocument(oldDocument ? *oldDocument : document());

        m_idCache.clear();
        m_nameCache.clear();
        m_hasValidIdNameCache = false;
    }

    void unregisterIdNameCacheFromDocument(Document& document) const
    {
        ASSERT(m_hasValidIdNameCache);
        document.decrementNodeListWithIdNameCacheCount();
    }

    const unsigned m_overridesItemAfter : 1;
    const unsigned m_shouldOnlyIncludeDirectChildren : 1;
    mutable unsigned m_hasValidIdNameCache : 1;
    mutable NodeCacheMap m_idCache;
    mutable NodeCacheMap m_nameCache;
    mutable CollectionIndexCache<HTMLCollection, Element> m_collectionIndexCache;

    friend class LiveNodeListBase;
};

} // namespace

#endif
