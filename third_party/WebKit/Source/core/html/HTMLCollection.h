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
    enum ItemAfterOverrideType {
        OverridesItemAfter,
        DoesNotOverrideItemAfter,
    };

    static PassRefPtr<HTMLCollection> create(ContainerNode* base, CollectionType);
    virtual ~HTMLCollection();
    virtual void invalidateCache() const OVERRIDE;

    // DOM API
    unsigned length() const { return m_collectionIndexCache.nodeCount(*this); }
    Node* item(unsigned offset) const { return m_collectionIndexCache.nodeAt(*this, offset); }
    virtual Element* namedItem(const AtomicString& name) const;

    // Non-DOM API
    void namedItems(const AtomicString& name, Vector<RefPtr<Element> >&) const;
    bool isEmpty() const { return m_collectionIndexCache.isEmpty(*this); }
    bool hasExactlyOneItem() const { return m_collectionIndexCache.hasExactlyOneNode(*this); }

    // CollectionIndexCache API.
    bool canTraverseBackward() const { return !overridesItemAfter(); }
    Element* traverseToFirstElement(const ContainerNode& root) const;
    Element* traverseForwardToOffset(unsigned offset, Node& currentElement, unsigned& currentOffset, const ContainerNode& root) const;

protected:
    HTMLCollection(ContainerNode* base, CollectionType, ItemAfterOverrideType);

    bool overridesItemAfter() const { return m_overridesItemAfter; }
    virtual Element* virtualItemAfter(Element*) const;

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

    const unsigned m_overridesItemAfter : 1;
    mutable unsigned m_isNameCacheValid : 1;
    mutable NodeCacheMap m_idCache;
    mutable NodeCacheMap m_nameCache;
    mutable CollectionIndexCache<HTMLCollection> m_collectionIndexCache;

    friend class LiveNodeListBase;
};

} // namespace

#endif
