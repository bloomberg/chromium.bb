/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009, 2010, 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/editing/StyledMarkupSerializer.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/htmlediting.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/markup.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLElement.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

const String& styleNodeCloseTag(bool isBlock)
{
    DEFINE_STATIC_LOCAL(const String, divClose, ("</div>"));
    DEFINE_STATIC_LOCAL(const String, styleSpanClose, ("</span>"));
    return isBlock ? divClose : styleSpanClose;
}

template<typename PositionType>
TextOffset toTextOffset(const PositionType& position)
{
    if (position.isNull())
        return TextOffset();

    if (!position.containerNode()->isTextNode())
        return TextOffset();

    return TextOffset(toText(position.containerNode()), position.offsetInContainerNode());
}

} // namespace

using namespace HTMLNames;

static Position toPositionInDOMTree(const Position& position)
{
    return position;
}

template<typename Strategy>
StyledMarkupSerializer<Strategy>::StyledMarkupSerializer(EAbsoluteURLs shouldResolveURLs, EAnnotateForInterchange shouldAnnotate, const PositionType& start, const PositionType& end, Node* highestNodeToBeSerialized)
    : m_markupAccumulator(shouldResolveURLs, toTextOffset(start.parentAnchoredEquivalent()), toTextOffset(end.parentAnchoredEquivalent()), start.document(), shouldAnnotate, highestNodeToBeSerialized)
    , m_start(start)
    , m_end(end)
{
}

static bool needInterchangeNewlineAfter(const VisiblePosition& v)
{
    VisiblePosition next = v.next();
    Node* upstreamNode = next.deepEquivalent().upstream().deprecatedNode();
    Node* downstreamNode = v.deepEquivalent().downstream().deprecatedNode();
    // Add an interchange newline if a paragraph break is selected and a br won't already be added to the markup to represent it.
    return isEndOfParagraph(v) && isStartOfParagraph(next) && !(isHTMLBRElement(*upstreamNode) && upstreamNode == downstreamNode);
}

static bool needInterchangeNewlineAt(const VisiblePosition& v)
{
    // FIXME: |v.previous()| works on a DOM tree. We need to fix this to work on
    // a composed tree.
    return needInterchangeNewlineAfter(v.previous());
}

template<typename PositionType>
static bool areSameRanges(Node* node, const PositionType& startPosition, const PositionType& endPosition)
{
    ASSERT(node);
    Position otherStartPosition;
    Position otherEndPosition;
    VisibleSelection::selectionFromContentsOfNode(node).toNormalizedPositions(otherStartPosition, otherEndPosition);
    return toPositionInDOMTree(startPosition) == otherStartPosition && toPositionInDOMTree(endPosition) == otherEndPosition;
}

static PassRefPtrWillBeRawPtr<EditingStyle> styleFromMatchedRulesAndInlineDecl(const HTMLElement* element)
{
    RefPtrWillBeRawPtr<EditingStyle> style = EditingStyle::create(element->inlineStyle());
    // FIXME: Having to const_cast here is ugly, but it is quite a bit of work to untangle
    // the non-const-ness of styleFromMatchedRulesForElement.
    style->mergeStyleFromRules(const_cast<HTMLElement*>(element));
    return style.release();
}

template<typename Strategy>
String StyledMarkupSerializer<Strategy>::createMarkup(bool convertBlocksToInlines, Node* specialCommonAncestor)
{
    DEFINE_STATIC_LOCAL(const String, interchangeNewlineString, ("<br class=\"" AppleInterchangeNewline "\">"));

    Node* pastEnd = m_end.nodeAsRangePastLastNode();

    Node* firstNode = m_start.nodeAsRangeFirstNode();
    VisiblePosition visibleStart(toPositionInDOMTree(m_start), VP_DEFAULT_AFFINITY);
    VisiblePosition visibleEnd(toPositionInDOMTree(m_end), VP_DEFAULT_AFFINITY);
    if (m_markupAccumulator.shouldAnnotateForInterchange() && needInterchangeNewlineAfter(visibleStart)) {
        if (visibleStart == visibleEnd.previous())
            return interchangeNewlineString;

        m_markupAccumulator.appendString(interchangeNewlineString);
        firstNode = visibleStart.next().deepEquivalent().deprecatedNode();

        if (pastEnd && Strategy::PositionType::beforeNode(firstNode).compareTo(Strategy::PositionType::beforeNode(pastEnd)) >= 0) {
            // This condition hits in editing/pasteboard/copy-display-none.html.
            return interchangeNewlineString;
        }
    }

    Node* lastClosed = serializeNodes(firstNode, pastEnd);

    if (specialCommonAncestor && lastClosed) {
        // TODO(hajimehoshi): This is calculated at createMarkupInternal too.
        Node* commonAncestor = Strategy::commonAncestor(*m_start.containerNode(), *m_end.containerNode());
        ASSERT(commonAncestor);
        HTMLBodyElement* body = toHTMLBodyElement(enclosingElementWithTag(firstPositionInNode(commonAncestor), bodyTag));
        HTMLBodyElement* fullySelectedRoot = nullptr;
        // FIXME: Do this for all fully selected blocks, not just the body.
        if (body && areSameRanges(body, m_start, m_end))
            fullySelectedRoot = body;

        // Also include all of the ancestors of lastClosed up to this special ancestor.
        // FIXME: What is ancestor?
        for (ContainerNode* ancestor = Strategy::parent(*lastClosed); ancestor; ancestor = Strategy::parent(*ancestor)) {
            if (ancestor == fullySelectedRoot && !convertBlocksToInlines) {
                RefPtrWillBeRawPtr<EditingStyle> fullySelectedRootStyle = styleFromMatchedRulesAndInlineDecl(fullySelectedRoot);

                // Bring the background attribute over, but not as an attribute because a background attribute on a div
                // appears to have no effect.
                if ((!fullySelectedRootStyle || !fullySelectedRootStyle->style() || !fullySelectedRootStyle->style()->getPropertyCSSValue(CSSPropertyBackgroundImage))
                    && fullySelectedRoot->hasAttribute(backgroundAttr))
                    fullySelectedRootStyle->style()->setProperty(CSSPropertyBackgroundImage, "url('" + fullySelectedRoot->getAttribute(backgroundAttr) + "')");

                if (fullySelectedRootStyle->style()) {
                    // Reset the CSS properties to avoid an assertion error in addStyleMarkup().
                    // This assertion is caused at least when we select all text of a <body> element whose
                    // 'text-decoration' property is "inherit", and copy it.
                    if (!propertyMissingOrEqualToNone(fullySelectedRootStyle->style(), CSSPropertyTextDecoration))
                        fullySelectedRootStyle->style()->setProperty(CSSPropertyTextDecoration, CSSValueNone);
                    if (!propertyMissingOrEqualToNone(fullySelectedRootStyle->style(), CSSPropertyWebkitTextDecorationsInEffect))
                        fullySelectedRootStyle->style()->setProperty(CSSPropertyWebkitTextDecorationsInEffect, CSSValueNone);
                    wrapWithStyleNode(fullySelectedRootStyle->style(), true);
                }
            } else {
                // Since this node and all the other ancestors are not in the selection we want to set RangeFullySelectsNode to DoesNotFullySelectNode
                // so that styles that affect the exterior of the node are not included.
                wrapWithNode(*ancestor, convertBlocksToInlines, StyledMarkupAccumulator::DoesNotFullySelectNode);
            }

            if (ancestor == specialCommonAncestor)
                break;
        }
    }

    // FIXME: The interchange newline should be placed in the block that it's in, not after all of the content, unconditionally.
    if (m_markupAccumulator.shouldAnnotateForInterchange() && needInterchangeNewlineAt(visibleEnd))
        m_markupAccumulator.appendString(interchangeNewlineString);

    return takeResults();
}

template<typename Strategy>
void StyledMarkupSerializer<Strategy>::wrapWithNode(ContainerNode& node, bool convertBlocksToInlines, typename StyledMarkupAccumulator::RangeFullySelectsNode rangeFullySelectsNode)
{
    StringBuilder markup;
    if (node.isElementNode())
        m_markupAccumulator.appendElement(markup, toElement(node), convertBlocksToInlines && isBlock(&node), rangeFullySelectsNode);
    else
        m_markupAccumulator.appendStartMarkup(markup, node, 0);
    m_reversedPrecedingMarkup.append(markup.toString());
    if (node.isElementNode())
        m_markupAccumulator.appendEndTag(toElement(node));
}

template<typename Strategy>
void StyledMarkupSerializer<Strategy>::wrapWithStyleNode(StylePropertySet* style, bool isBlock)
{
    StringBuilder openTag;
    m_markupAccumulator.appendStyleNodeOpenTag(openTag, style, isBlock);
    m_reversedPrecedingMarkup.append(openTag.toString());
    m_markupAccumulator.appendString(styleNodeCloseTag(isBlock));
}

template<typename Strategy>
String StyledMarkupSerializer<Strategy>::takeResults()
{
    StringBuilder result;
    result.reserveCapacity(MarkupAccumulator::totalLength(m_reversedPrecedingMarkup) + m_markupAccumulator.length());

    for (size_t i = m_reversedPrecedingMarkup.size(); i > 0; --i)
        result.append(m_reversedPrecedingMarkup[i - 1]);

    m_markupAccumulator.concatenateMarkup(result);

    // We remove '\0' characters because they are not visibly rendered to the user.
    return result.toString().replace(0, "");
}

template<typename Strategy>
Node* StyledMarkupSerializer<Strategy>::serializeNodes(Node* startNode, Node* pastEnd)
{
    if (!m_markupAccumulator.highestNodeToBeSerialized()) {
        Node* lastClosed = traverseNodesForSerialization(startNode, pastEnd, DoNotEmitString);
        m_markupAccumulator.setHighestNodeToBeSerialized(lastClosed);
    }

    Node* highestNodeToBeSerialized = m_markupAccumulator.highestNodeToBeSerialized();
    if (highestNodeToBeSerialized && Strategy::parent(*highestNodeToBeSerialized)) {
        RefPtrWillBeRawPtr<EditingStyle> wrappingStyle = EditingStyle::wrappingStyleForSerialization(Strategy::parent(*highestNodeToBeSerialized), m_markupAccumulator.shouldAnnotate());
        if (m_markupAccumulator.shouldAnnotateForNavigationTransition()) {
            wrappingStyle->style()->removeProperty(CSSPropertyBackgroundColor);
            wrappingStyle->style()->removeProperty(CSSPropertyBackgroundImage);
        }
        m_markupAccumulator.setWrappingStyle(wrappingStyle.release());
    }
    return traverseNodesForSerialization(startNode, pastEnd, EmitString);
}

template<typename Strategy>
Node* StyledMarkupSerializer<Strategy>::traverseNodesForSerialization(Node* startNode, Node* pastEnd, NodeTraversalMode traversalMode)
{
    const bool shouldEmit = traversalMode == EmitString;
    WillBeHeapVector<RawPtrWillBeMember<ContainerNode>> ancestorsToClose;
    Node* next;
    Node* lastClosed = nullptr;
    for (Node* n = startNode; n != pastEnd; n = next) {
        // According to <rdar://problem/5730668>, it is possible for n to blow
        // past pastEnd and become null here. This shouldn't be possible.
        // This null check will prevent crashes (but create too much markup)
        // and the ASSERT will hopefully lead us to understanding the problem.
        ASSERT(n);
        if (!n)
            break;

        next = Strategy::next(*n);
        bool openedTag = false;

        if (isBlock(n) && canHaveChildrenForEditing(n) && next == pastEnd) {
            // Don't write out empty block containers that aren't fully selected.
            continue;
        }

        if (!n->layoutObject() && !enclosingElementWithTag(firstPositionInOrBeforeNode(n), selectTag) && !m_markupAccumulator.shouldAnnotateForNavigationTransition()) {
            next = Strategy::nextSkippingChildren(*n);
            // Don't skip over pastEnd.
            if (pastEnd && Strategy::isDescendantOf(*pastEnd, *n))
                next = pastEnd;
        } else {
            // Add the node to the markup if we're not skipping the descendants
            if (shouldEmit)
                m_markupAccumulator.appendStartTag(*n);

            // If node has no children, close the tag now.
            if (Strategy::hasChildren(*n)) {
                openedTag = true;
                ancestorsToClose.append(toContainerNode(n));
            } else {
                if (shouldEmit && n->isElementNode())
                    m_markupAccumulator.appendEndTag(toElement(*n));
                lastClosed = n;
            }
        }

        // If we didn't insert open tag and there's no more siblings or we're at the end of the traversal, take care of ancestors.
        // FIXME: What happens if we just inserted open tag and reached the end?
        if (!openedTag && (!Strategy::nextSibling(*n) || next == pastEnd)) {
            // Close up the ancestors.
            while (!ancestorsToClose.isEmpty()) {
                ContainerNode* ancestor = ancestorsToClose.last();
                ASSERT(ancestor);
                if (next != pastEnd && Strategy::isDescendantOf(*next, *ancestor))
                    break;
                // Not at the end of the range, close ancestors up to sibling of next node.
                if (shouldEmit && ancestor->isElementNode())
                    m_markupAccumulator.appendEndTag(toElement(*ancestor));
                lastClosed = ancestor;
                ancestorsToClose.removeLast();
            }

            // Surround the currently accumulated markup with markup for ancestors we never opened as we leave the subtree(s) rooted at those ancestors.
            ContainerNode* nextParent = next ? Strategy::parent(*next) : nullptr;
            if (next != pastEnd && n != nextParent) {
                ASSERT(n);
                Node* lastAncestorClosedOrSelf = Strategy::isDescendantOf(*n, *lastClosed) ? lastClosed : n;
                for (ContainerNode* parent = Strategy::parent(*lastAncestorClosedOrSelf); parent && parent != nextParent; parent = Strategy::parent(*parent)) {
                    // All ancestors that aren't in the ancestorsToClose list should either be a) unrendered:
                    if (!parent->layoutObject())
                        continue;
                    // or b) ancestors that we never encountered during a pre-order traversal starting at startNode:
                    ASSERT(startNode);
                    ASSERT(Strategy::isDescendantOf(*startNode, *parent));
                    if (shouldEmit)
                        wrapWithNode(*parent);
                    lastClosed = parent;
                }
            }
        }
    }

    return lastClosed;
}

template class StyledMarkupSerializer<EditingStrategy>;

} // namespace blink
