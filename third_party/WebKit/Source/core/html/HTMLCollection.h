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

    static PassRefPtr<HTMLCollection> create(ContainerNode& base, CollectionType);
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
    Element* traverseToFirstElement() const;
    Element* traverseForwardToOffset(unsigned offset, Element& currentElement, unsigned& currentOffset) const;

protected:
    HTMLCollection(ContainerNode& base, CollectionType, ItemAfterOverrideType);

    class NamedItemCache {
    public:
        Vector<Element*>* getElementsById(const AtomicString& id) const { return m_idCache.get(id.impl()); }
        Vector<Element*>* getElementsByName(const AtomicString& name) const { return m_nameCache.get(name.impl()); }
        void addElementWithId(const AtomicString& id, Element* element) { addElementToMap(m_idCache, id, element); }
        void addElementWithName(const AtomicString& name, Element* element) { addElementToMap(m_nameCache, name, element); }

    private:
        typedef HashMap<StringImpl*, OwnPtr<Vector<Element*> > > StringToElementsMap;
        static void addElementToMap(StringToElementsMap& map, const AtomicString& key, Element* element)
        {
            OwnPtr<Vector<Element*> >& vector = map.add(key.impl(), nullptr).storedValue->value;
            if (!vector)
                vector = adoptPtr(new Vector<Element*>);
            vector->append(element);
        }

        StringToElementsMap m_idCache;
        StringToElementsMap m_nameCache;
    };

    bool overridesItemAfter() const { return m_overridesItemAfter; }
    virtual Element* virtualItemAfter(Element*) const;
    bool shouldOnlyIncludeDirectChildren() const { return m_shouldOnlyIncludeDirectChildren; }
    virtual void supportedPropertyNames(Vector<String>& names);

    virtual void updateIdNameCache() const;
    bool hasValidIdNameCache() const { return m_namedItemCache; }

    NamedItemCache& createNamedItemCache() const
    {
        ASSERT(!m_namedItemCache);
        document().incrementNodeListWithIdNameCacheCount();
        m_namedItemCache = adoptPtr(new NamedItemCache);
        return *m_namedItemCache;
    }
    NamedItemCache& namedItemCache() const
    {
        ASSERT(m_namedItemCache);
        return *m_namedItemCache;
    }

private:
    Element* traverseNextElement(Element& previous) const;

    void invalidateIdNameCacheMaps(Document* oldDocument = 0) const
    {
        if (!hasValidIdNameCache())
            return;

        // Make sure we decrement the NodeListWithIdNameCache count from
        // the old document instead of the new one in the case the collection
        // is moved to a new document.
        unregisterIdNameCacheFromDocument(oldDocument ? *oldDocument : document());

        m_namedItemCache.clear();
    }

    void unregisterIdNameCacheFromDocument(Document& document) const
    {
        ASSERT(hasValidIdNameCache());
        document.decrementNodeListWithIdNameCacheCount();
    }

    const unsigned m_overridesItemAfter : 1;
    const unsigned m_shouldOnlyIncludeDirectChildren : 1;
    mutable OwnPtr<NamedItemCache> m_namedItemCache;
    mutable CollectionIndexCache<HTMLCollection, Element> m_collectionIndexCache;

    friend class LiveNodeListBase;
};

} // namespace

#endif
