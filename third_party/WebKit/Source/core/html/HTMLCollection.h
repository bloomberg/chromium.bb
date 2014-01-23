/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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

#include "core/dom/LiveNodeList.h"
#include "core/html/CollectionType.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace WebCore {

class HTMLCollection : public ScriptWrappable, public RefCounted<HTMLCollection>, public LiveNodeListBase {
public:
    static PassRefPtr<HTMLCollection> create(ContainerNode* base, CollectionType);
    virtual ~HTMLCollection();
    virtual void invalidateCache() const OVERRIDE;

    // DOM API
    virtual Node* namedItem(const AtomicString& name) const;

    // Non-DOM API
    void namedItems(const AtomicString& name, Vector<RefPtr<Node> >&) const;
    bool isEmpty() const
    {
        if (isLengthCacheValid())
            return !cachedLength();
        if (cachedItem())
            return false;
        return !item(0);
    }
    bool hasExactlyOneItem() const
    {
        if (isLengthCacheValid())
            return cachedLength() == 1;
        if (cachedItem())
            return !cachedItemOffset() && !item(1);
        return item(0) && !item(1);
    }

    virtual Element* virtualItemAfter(Element*) const;

    Element* traverseToFirstElement(const ContainerNode& root) const;
    Element* traverseForwardToOffset(unsigned offset, Element& currentElement, unsigned& currentOffset, const ContainerNode& root) const;

protected:
    HTMLCollection(ContainerNode* base, CollectionType, ItemAfterOverrideType);

    virtual void updateNameCache() const;
    bool hasNameCache() const { return m_isNameCacheValid; }
    void setHasNameCache() const { m_isNameCacheValid = true; }

    typedef HashMap<StringImpl*, OwnPtr<Vector<Element*> > > NodeCacheMap;
    Vector<Element*>* idCache(const AtomicString& name) const { return m_idCache.get(name.impl()); }
    Vector<Element*>* nameCache(const AtomicString& name) const { return m_nameCache.get(name.impl()); }
    void appendIdCache(const AtomicString& name, Element* element) const { append(m_idCache, name, element); }
    void appendNameCache(const AtomicString& name, Element* element) const { append(m_nameCache, name, element); }

private:
    bool checkForNameMatch(const Element&, bool checkName, const AtomicString& name) const;
    Element* traverseNextElement(Element& previous, const ContainerNode& root) const;

    static void append(NodeCacheMap&, const AtomicString&, Element*);
    void invalidateIdNameCacheMaps() const
    {
        m_idCache.clear();
        m_nameCache.clear();
        m_isNameCacheValid = false;
    }

    mutable unsigned m_isNameCacheValid : 1;
    mutable NodeCacheMap m_idCache;
    mutable NodeCacheMap m_nameCache;

    friend class LiveNodeListBase;
};

} // namespace

#endif
