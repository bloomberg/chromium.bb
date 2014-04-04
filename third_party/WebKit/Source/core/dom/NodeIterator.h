/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#ifndef NodeIterator_h
#define NodeIterator_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/NodeFilter.h"
#include "core/dom/NodeIteratorBase.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExceptionState;

class NodeIterator : public ScriptWrappable, public RefCounted<NodeIterator>, public NodeIteratorBase {
public:
    static PassRefPtr<NodeIterator> create(PassRefPtr<Node> rootNode, unsigned whatToShow, PassRefPtr<NodeFilter> filter)
    {
        return adoptRef(new NodeIterator(rootNode, whatToShow, filter));
    }
    ~NodeIterator();

    PassRefPtr<Node> nextNode(ExceptionState&);
    PassRefPtr<Node> previousNode(ExceptionState&);
    void detach();

    Node* referenceNode() const { return m_referenceNode.node.get(); }
    bool pointerBeforeReferenceNode() const { return m_referenceNode.isPointerBeforeNode; }

    // This function is called before any node is removed from the document tree.
    void nodeWillBeRemoved(Node&);

private:
    NodeIterator(PassRefPtr<Node>, unsigned whatToShow, PassRefPtr<NodeFilter>);

    struct NodePointer {
        RefPtr<Node> node;
        bool isPointerBeforeNode;
        NodePointer();
        NodePointer(PassRefPtr<Node>, bool);
        void clear();
        bool moveToNext(Node* root);
        bool moveToPrevious(Node* root);
    };

    void updateForNodeRemoval(Node& nodeToBeRemoved, NodePointer&) const;

    NodePointer m_referenceNode;
    NodePointer m_candidateNode;
    bool m_detached;
};

} // namespace WebCore

#endif // NodeIterator_h
