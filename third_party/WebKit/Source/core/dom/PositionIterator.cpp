/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "core/dom/PositionIterator.h"

#include "core/dom/Position.h"

namespace blink {

template <typename Strategy>
PositionIteratorAlgorithm<Strategy>::PositionIteratorAlgorithm(Node* anchorNode, int offsetInAnchor)
    : m_anchorNode(anchorNode)
    , m_nodeAfterPositionInAnchor(Strategy::childAt(*anchorNode, offsetInAnchor))
    , m_offsetInAnchor(m_nodeAfterPositionInAnchor ? 0 : offsetInAnchor)
{
}

template <typename Strategy>
PositionIteratorAlgorithm<Strategy>::PositionIteratorAlgorithm(const PositionType& pos)
    : PositionIteratorAlgorithm(pos.anchorNode(), pos.deprecatedEditingOffset())
{
}

template <typename Strategy>
PositionIteratorAlgorithm<Strategy>::PositionIteratorAlgorithm()
    : m_anchorNode(nullptr)
    , m_nodeAfterPositionInAnchor(nullptr)
    , m_offsetInAnchor(0)
{
}

template <typename Strategy>
PositionIteratorAlgorithm<Strategy>::operator typename PositionIteratorAlgorithm<Strategy>::PositionType() const
{
    if (m_nodeAfterPositionInAnchor) {
        ASSERT(Strategy::parent(*m_nodeAfterPositionInAnchor) == m_anchorNode);
        // FIXME: This check is inadaquete because any ancestor could be ignored by editing
        if (Strategy::editingIgnoresContent(Strategy::parent(*m_nodeAfterPositionInAnchor)))
            return Strategy::positionBeforeNode(m_anchorNode);
        return Strategy::positionInParentBeforeNode(m_nodeAfterPositionInAnchor);
    }
    if (Strategy::hasChildren(*m_anchorNode))
        return Strategy::lastPositionInOrAfterNode(m_anchorNode);
    return Strategy::createLegacyEditingPosition(m_anchorNode, m_offsetInAnchor);
}

template <typename Strategy>
void PositionIteratorAlgorithm<Strategy>::increment()
{
    if (!m_anchorNode)
        return;

    if (m_nodeAfterPositionInAnchor) {
        m_anchorNode = m_nodeAfterPositionInAnchor;
        m_nodeAfterPositionInAnchor = Strategy::firstChild(*m_anchorNode);
        m_offsetInAnchor = 0;
        return;
    }

    if (!Strategy::hasChildren(*m_anchorNode) && m_offsetInAnchor < Strategy::lastOffsetForEditing(m_anchorNode)) {
        m_offsetInAnchor = Strategy::uncheckedNextOffset(m_anchorNode, m_offsetInAnchor);
    } else {
        m_nodeAfterPositionInAnchor = m_anchorNode;
        m_anchorNode = Strategy::parent(*m_nodeAfterPositionInAnchor);
        m_nodeAfterPositionInAnchor = Strategy::nextSibling(*m_nodeAfterPositionInAnchor);
        m_offsetInAnchor = 0;
    }
}

template <typename Strategy>
void PositionIteratorAlgorithm<Strategy>::decrement()
{
    if (!m_anchorNode)
        return;

    if (m_nodeAfterPositionInAnchor) {
        m_anchorNode = Strategy::previousSibling(*m_nodeAfterPositionInAnchor);
        if (m_anchorNode) {
            m_nodeAfterPositionInAnchor = nullptr;
            m_offsetInAnchor = Strategy::hasChildren(*m_anchorNode) ? 0 : Strategy::lastOffsetForEditing(m_anchorNode);
        } else {
            m_nodeAfterPositionInAnchor = Strategy::parent(*m_nodeAfterPositionInAnchor);
            m_anchorNode = Strategy::parent(*m_nodeAfterPositionInAnchor);
            m_offsetInAnchor = 0;
        }
        return;
    }

    if (Strategy::hasChildren(*m_anchorNode)) {
        m_anchorNode = Strategy::lastChild(*m_anchorNode);
        m_offsetInAnchor = Strategy::hasChildren(*m_anchorNode)? 0 : Strategy::lastOffsetForEditing(m_anchorNode);
    } else {
        if (m_offsetInAnchor) {
            m_offsetInAnchor = Strategy::uncheckedPreviousOffset(m_anchorNode, m_offsetInAnchor);
        } else {
            m_nodeAfterPositionInAnchor = m_anchorNode;
            m_anchorNode = Strategy::parent(*m_anchorNode);
        }
    }
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::atStart() const
{
    if (!m_anchorNode)
        return true;
    if (Strategy::parent(*m_anchorNode))
        return false;
    return (!Strategy::hasChildren(*m_anchorNode) && !m_offsetInAnchor) || (m_nodeAfterPositionInAnchor && !Strategy::previousSibling(*m_nodeAfterPositionInAnchor));
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::atEnd() const
{
    if (!m_anchorNode)
        return true;
    if (m_nodeAfterPositionInAnchor)
        return false;
    return !Strategy::parent(*m_anchorNode) && (Strategy::hasChildren(*m_anchorNode) || m_offsetInAnchor >= Strategy::lastOffsetForEditing(m_anchorNode));
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::atStartOfNode() const
{
    if (!m_anchorNode)
        return true;
    if (!m_nodeAfterPositionInAnchor)
        return !Strategy::hasChildren(*m_anchorNode) && !m_offsetInAnchor;
    return !Strategy::previousSibling(*m_nodeAfterPositionInAnchor);
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::atEndOfNode() const
{
    if (!m_anchorNode)
        return true;
    if (m_nodeAfterPositionInAnchor)
        return false;
    return Strategy::hasChildren(*m_anchorNode) || m_offsetInAnchor >= Strategy::lastOffsetForEditing(m_anchorNode);
}

template <typename Strategy>
bool PositionIteratorAlgorithm<Strategy>::isCandidate() const
{
    if (!m_anchorNode)
        return false;

    LayoutObject* renderer = m_anchorNode->layoutObject();
    if (!renderer)
        return false;

    if (renderer->style()->visibility() != VISIBLE)
        return false;

    if (renderer->isBR()) {
        // For br element, the condition
        // |(!Strategy::hasChildren(*m_anchorNode) || m_nodeAfterPositionInAnchor)|
        // corresponds to the condition
        // |m_anchorType != PositionIsAfterAnchor| in Position.isCandaite.
        // Both conditions say this position is not in tail of the element.
        // If conditions lose consitency, VisiblePosition::canonicalPosition
        // will fail on |canonicalizeCandidate(previousCandidate(position))|,
        // because previousCandidate returns a Position converted from
        // a "Candidate" PositionIterator and cannonicalizeCandidate(Position)
        // assumes the Position is "Candidate".
        return !m_offsetInAnchor && (!Strategy::hasChildren(*m_anchorNode) || m_nodeAfterPositionInAnchor) && !Strategy::nodeIsUserSelectNone(Strategy::parent(*m_anchorNode));
    }
    if (renderer->isText())
        return !Strategy::nodeIsUserSelectNone(m_anchorNode) && Strategy::inRenderedText(PositionType(*this));

    if (renderer->isSVG()) {
        // We don't consider SVG elements are contenteditable except for
        // associated renderer returns isText() true, e.g. LayoutSVGInlineText.
        return false;
    }

    if (isRenderedHTMLTableElement(m_anchorNode) || editingIgnoresContent(m_anchorNode))
        return (atStartOfNode() || atEndOfNode()) && !Strategy::nodeIsUserSelectNone(Strategy::parent(*m_anchorNode));

    if (!isHTMLHtmlElement(*m_anchorNode) && renderer->isLayoutBlockFlow()) {
        if (toLayoutBlock(renderer)->logicalHeight() || isHTMLBodyElement(*m_anchorNode)) {
            if (!PositionType::hasRenderedNonAnonymousDescendantsWithHeight(renderer))
                return atStartOfNode() && !Strategy::nodeIsUserSelectNone(m_anchorNode);
            return m_anchorNode->hasEditableStyle() && !Strategy::nodeIsUserSelectNone(m_anchorNode) && Strategy::atEditingBoundary(PositionType(*this));
        }
    }

    return false;
}

// ---

bool PositionIteratorStrategy::atEditingBoundary(const PositionType& pos)
{
    return pos.atEditingBoundary();
}

Position PositionIteratorStrategy::createLegacyEditingPosition(Node* node, int offset)
{
    return ::blink::createLegacyEditingPosition(node, offset);
}

int PositionIteratorStrategy::editingOffset(const Position& position)
{
    return position.deprecatedEditingOffset();
}

bool PositionIteratorStrategy::editingIgnoresContent(Node* node)
{
    return ::blink::editingIgnoresContent(node);
}

bool PositionIteratorStrategy::inRenderedText(const Position& pos)
{
    return pos.inRenderedText();
}

int PositionIteratorStrategy::lastOffsetForEditing(Node* node)
{
    return ::blink::lastOffsetForEditing(node);
}

Position PositionIteratorStrategy::lastPositionInOrAfterNode(Node* node)
{
    return ::blink::lastPositionInOrAfterNode(node);
}

bool PositionIteratorStrategy::nodeIsUserSelectNone(Node* node)
{
    return Position::nodeIsUserSelectNone(node);
}

Position PositionIteratorStrategy::positionBeforeNode(Node* node)
{
    return ::blink::positionBeforeNode(node);
}

Position PositionIteratorStrategy::positionInParentBeforeNode(Node* node)
{
    return ::blink::positionInParentBeforeNode(*node);
}

int PositionIteratorStrategy::uncheckedNextOffset(const Node* node, int offset)
{
    return Position::uncheckedNextOffset(node, offset);
}

int PositionIteratorStrategy::uncheckedPreviousOffset(const Node* node, int offset)
{
    return Position::uncheckedPreviousOffset(node, offset);
}

// ---

template class PositionIteratorAlgorithm<PositionIteratorStrategy>;

} // namespace blink
