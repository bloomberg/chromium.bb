/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef LiveNodeList_h
#define LiveNodeList_h

#include "HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/NodeList.h"
#include "core/html/CollectionIndexCache.h"
#include "core/html/CollectionType.h"
#include "wtf/Forward.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Element;

enum NodeListRootType {
    NodeListIsRootedAtNode,
    NodeListIsRootedAtDocument,
    NodeListIsRootedAtDocumentIfOwnerHasItemrefAttr,
};

class LiveNodeListBase {
public:
    LiveNodeListBase(ContainerNode* ownerNode, NodeListRootType rootType, NodeListInvalidationType invalidationType,
        bool shouldOnlyIncludeDirectChildren, CollectionType collectionType)
        : m_ownerNode(ownerNode)
        , m_rootType(rootType)
        , m_invalidationType(invalidationType)
        , m_shouldOnlyIncludeDirectChildren(shouldOnlyIncludeDirectChildren)
        , m_collectionType(collectionType)
    {
        ASSERT(m_ownerNode);
        ASSERT(m_rootType == static_cast<unsigned>(rootType));
        ASSERT(m_invalidationType == static_cast<unsigned>(invalidationType));
        ASSERT(m_collectionType == static_cast<unsigned>(collectionType));

        if (collectionType != ChildNodeListType)
            document().registerNodeList(this);
    }

    virtual ~LiveNodeListBase()
    {
        if (type() != ChildNodeListType)
            document().unregisterNodeList(this);
    }

    ContainerNode& rootNode() const;
    Node* itemBefore(const Node* previousItem) const;

    ALWAYS_INLINE bool hasIdNameCache() const { return !isLiveNodeListType(type()); }
    ALWAYS_INLINE bool isRootedAtDocument() const { return m_rootType == NodeListIsRootedAtDocument || m_rootType == NodeListIsRootedAtDocumentIfOwnerHasItemrefAttr; }
    ALWAYS_INLINE NodeListInvalidationType invalidationType() const { return static_cast<NodeListInvalidationType>(m_invalidationType); }
    ALWAYS_INLINE CollectionType type() const { return static_cast<CollectionType>(m_collectionType); }
    ContainerNode* ownerNode() const { return m_ownerNode.get(); }
    ALWAYS_INLINE void invalidateCache(const QualifiedName* attrName) const
    {
        if (!attrName || shouldInvalidateTypeOnAttributeChange(invalidationType(), *attrName))
            invalidateCache();
        else if (hasIdNameCache() && (*attrName == HTMLNames::idAttr || *attrName == HTMLNames::nameAttr))
            invalidateIdNameCacheMaps();
    }
    virtual void invalidateCache() const = 0;

    static bool shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType, const QualifiedName&);

protected:
    Document& document() const { return m_ownerNode->document(); }

    ALWAYS_INLINE NodeListRootType rootType() const { return static_cast<NodeListRootType>(m_rootType); }
    bool shouldOnlyIncludeDirectChildren() const { return m_shouldOnlyIncludeDirectChildren; }

private:
    Node* iterateForPreviousNode(Node* current) const;
    void invalidateIdNameCacheMaps() const;

    RefPtr<ContainerNode> m_ownerNode; // Cannot be null.
    const unsigned m_rootType : 2;
    const unsigned m_invalidationType : 4;
    const unsigned m_shouldOnlyIncludeDirectChildren : 1;
    const unsigned m_collectionType : 5;
};

ALWAYS_INLINE bool LiveNodeListBase::shouldInvalidateTypeOnAttributeChange(NodeListInvalidationType type, const QualifiedName& attrName)
{
    switch (type) {
    case InvalidateOnClassAttrChange:
        return attrName == HTMLNames::classAttr;
    case InvalidateOnNameAttrChange:
        return attrName == HTMLNames::nameAttr;
    case InvalidateOnIdNameAttrChange:
        return attrName == HTMLNames::idAttr || attrName == HTMLNames::nameAttr;
    case InvalidateOnForAttrChange:
        return attrName == HTMLNames::forAttr;
    case InvalidateForFormControls:
        return attrName == HTMLNames::nameAttr || attrName == HTMLNames::idAttr || attrName == HTMLNames::forAttr
            || attrName == HTMLNames::formAttr || attrName == HTMLNames::typeAttr;
    case InvalidateOnHRefAttrChange:
        return attrName == HTMLNames::hrefAttr;
    case InvalidateOnItemAttrChange:
    case DoNotInvalidateOnAttributeChanges:
        return false;
    case InvalidateOnAnyAttrChange:
        return true;
    }
    return false;
}

class LiveNodeList : public NodeList, public LiveNodeListBase {
public:
    LiveNodeList(PassRefPtr<ContainerNode> ownerNode, CollectionType collectionType, NodeListInvalidationType invalidationType, NodeListRootType rootType = NodeListIsRootedAtNode)
        : LiveNodeListBase(ownerNode.get(), rootType, invalidationType, collectionType == ChildNodeListType,
        collectionType)
    { }

    virtual unsigned length() const OVERRIDE FINAL { return m_collectionIndexCache.nodeCount(*this); }
    virtual Node* item(unsigned offset) const OVERRIDE FINAL { return m_collectionIndexCache.nodeAt(*this, offset); }
    virtual bool nodeMatches(const Element&) const = 0;
    // Avoid ambiguity since both NodeList and LiveNodeListBase have an ownerNode() method.
    using LiveNodeListBase::ownerNode;

    virtual void invalidateCache() const OVERRIDE FINAL;

    // Collection IndexCache API.
    bool canTraverseBackward() const { return true; }
    Node* traverseToFirstElement(const ContainerNode& root) const;
    Node* traverseForwardToOffset(unsigned offset, Node& currentNode, unsigned& currentOffset, const ContainerNode& root) const;

private:
    virtual bool isLiveNodeList() const OVERRIDE FINAL { return true; }

    mutable CollectionIndexCache<LiveNodeList> m_collectionIndexCache;
};

} // namespace WebCore

#endif // LiveNodeList_h
