/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef RangeBoundaryPoint_h
#define RangeBoundaryPoint_h

#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/editing/Position.h"

namespace blink {

class RangeBoundaryPoint {
    DISALLOW_NEW();
public:
    explicit RangeBoundaryPoint(Node* container);

    explicit RangeBoundaryPoint(const RangeBoundaryPoint&);

    bool inShadowIncludingDocument() const;
    const Position toPosition() const;

    Node* container() const;
    int offset() const;
    Node* childBefore() const;

    void clear();

    void set(Node* container, int offset, Node* childBefore);
    void setOffset(int);

    void setToBeforeChild(Node&);
    void setToStartOfNode(Node&);
    void setToEndOfNode(Node&);

    void childBeforeWillBeRemoved();
    void invalidateOffset() const;
    void ensureOffsetIsValid() const;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_containerNode);
        visitor->trace(m_childBeforeBoundary);
    }

private:
    static const int invalidOffset = -1;

    Member<Node> m_containerNode;
    mutable int m_offsetInContainer;
    Member<Node> m_childBeforeBoundary;
};

inline RangeBoundaryPoint::RangeBoundaryPoint(Node* container)
    : m_containerNode(container)
    , m_offsetInContainer(0)
    , m_childBeforeBoundary(nullptr)
{
    DCHECK(m_containerNode);
}

inline RangeBoundaryPoint::RangeBoundaryPoint(const RangeBoundaryPoint& other)
    : m_containerNode(other.container())
    , m_offsetInContainer(other.offset())
    , m_childBeforeBoundary(other.childBefore())
{
}

inline Node* RangeBoundaryPoint::container() const
{
    return m_containerNode.get();
}

inline Node* RangeBoundaryPoint::childBefore() const
{
    return m_childBeforeBoundary.get();
}

inline void RangeBoundaryPoint::ensureOffsetIsValid() const
{
    if (m_offsetInContainer >= 0)
        return;

    DCHECK(m_childBeforeBoundary);
    m_offsetInContainer = m_childBeforeBoundary->nodeIndex() + 1;
}

inline bool RangeBoundaryPoint::inShadowIncludingDocument() const
{
    return m_containerNode && m_containerNode->inShadowIncludingDocument();
}

inline const Position RangeBoundaryPoint::toPosition() const
{
    ensureOffsetIsValid();
    return Position::editingPositionOf(m_containerNode.get(), m_offsetInContainer);
}

inline int RangeBoundaryPoint::offset() const
{
    ensureOffsetIsValid();
    return m_offsetInContainer;
}

inline void RangeBoundaryPoint::clear()
{
    m_containerNode.clear();
    m_offsetInContainer = 0;
    m_childBeforeBoundary = nullptr;
}

inline void RangeBoundaryPoint::set(Node* container, int offset, Node* childBefore)
{
    DCHECK(container);
    DCHECK_GE(offset, 0);
    DCHECK_EQ(childBefore, offset ? NodeTraversal::childAt(*container, offset - 1) : 0);
    m_containerNode = container;
    m_offsetInContainer = offset;
    m_childBeforeBoundary = childBefore;
}

inline void RangeBoundaryPoint::setOffset(int offset)
{
    DCHECK(m_containerNode);
    DCHECK(m_containerNode->offsetInCharacters());
    DCHECK_GE(m_offsetInContainer, 0);
    DCHECK(!m_childBeforeBoundary);
    m_offsetInContainer = offset;
}

inline void RangeBoundaryPoint::setToBeforeChild(Node& child)
{
    DCHECK(child.parentNode());
    m_childBeforeBoundary = child.previousSibling();
    m_containerNode = child.parentNode();
    m_offsetInContainer = m_childBeforeBoundary ? invalidOffset : 0;
}

inline void RangeBoundaryPoint::setToStartOfNode(Node& container)
{
    m_containerNode = &container;
    m_offsetInContainer = 0;
    m_childBeforeBoundary = nullptr;
}

inline void RangeBoundaryPoint::setToEndOfNode(Node& container)
{
    m_containerNode = &container;
    if (m_containerNode->offsetInCharacters()) {
        m_offsetInContainer = m_containerNode->maxCharacterOffset();
        m_childBeforeBoundary = nullptr;
    } else {
        m_childBeforeBoundary = m_containerNode->lastChild();
        m_offsetInContainer = m_childBeforeBoundary ? invalidOffset : 0;
    }
}

inline void RangeBoundaryPoint::childBeforeWillBeRemoved()
{
    DCHECK(m_offsetInContainer);
    m_childBeforeBoundary = m_childBeforeBoundary->previousSibling();
    if (!m_childBeforeBoundary)
        m_offsetInContainer = 0;
    else if (m_offsetInContainer > 0)
        --m_offsetInContainer;
}

inline void RangeBoundaryPoint::invalidateOffset() const
{
    m_offsetInContainer = invalidOffset;
}

inline bool operator==(const RangeBoundaryPoint& a, const RangeBoundaryPoint& b)
{
    if (a.container() != b.container())
        return false;
    if (a.childBefore() || b.childBefore()) {
        if (a.childBefore() != b.childBefore())
            return false;
    } else {
        if (a.offset() != b.offset())
            return false;
    }
    return true;
}

} // namespace blink

#endif
