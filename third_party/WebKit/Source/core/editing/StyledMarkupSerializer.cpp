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
#include "core/editing/htmlediting.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/markup.h"
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

} // namespace

using namespace HTMLNames;

StyledMarkupAccumulator::StyledMarkupAccumulator(StyledMarkupSerializer* serializer, EAbsoluteURLs shouldResolveURLs, const Position& start, const Position& end, EAnnotateForInterchange shouldAnnotate, Node* highestNodeToBeSerialized)
    : MarkupAccumulator(shouldResolveURLs)
    , m_serializer(serializer)
    , m_start(start.parentAnchoredEquivalent())
    , m_end(end.parentAnchoredEquivalent())
    , m_shouldAnnotate(shouldAnnotate)
    , m_highestNodeToBeSerialized(highestNodeToBeSerialized)
{
    ASSERT(m_start.isNull() == m_end.isNull());
}

void StyledMarkupAccumulator::appendText(StringBuilder& out, Text& text)
{
    const bool parentIsTextarea = text.parentElement() && text.parentElement()->tagQName() == textareaTag;
    const bool wrappingSpan = shouldApplyWrappingStyle(text) && !parentIsTextarea;
    if (wrappingSpan) {
        RefPtrWillBeRawPtr<EditingStyle> wrappingStyle = m_serializer->wrappingStyle()->copy();
        // FIXME: <rdar://problem/5371536> Style rules that match pasted content can change it's appearance
        // Make sure spans are inline style in paste side e.g. span { display: block }.
        wrappingStyle->forceInline();
        // FIXME: Should this be included in forceInline?
        wrappingStyle->style()->setProperty(CSSPropertyFloat, CSSValueNone);

        appendStyleNodeOpenTag(out, wrappingStyle->style());
    }

    if (!shouldAnnotate() || parentIsTextarea) {
        const String& str = text.data();
        unsigned length = str.length();
        unsigned start = 0;
        if (m_start.isNotNull() && m_end.isNotNull()) {
            if (text == m_end.containerNode())
                length = m_end.offsetInContainerNode();
            if (text == m_start.containerNode()) {
                start = m_start.offsetInContainerNode();
                length -= start;
            }
        }
        appendCharactersReplacingEntities(out, str, start, length, entityMaskForText(text));
    } else {
        const bool useRenderedText = !enclosingElementWithTag(firstPositionInNode(&text), selectTag);
        String content = useRenderedText ? renderedText(text) : stringValueForRange(text);
        StringBuilder buffer;
        MarkupAccumulator::appendCharactersReplacingEntities(buffer, content, 0, content.length(), EntityMaskInPCDATA);
        out.append(convertHTMLTextToInterchangeFormat(buffer.toString(), text));
    }

    if (wrappingSpan)
        out.append(styleNodeCloseTag(false));
}

void StyledMarkupAccumulator::appendElement(StringBuilder& out, Element& element, Namespaces*)
{
    appendElement(out, element, false, DoesFullySelectNode);
}

void StyledMarkupAccumulator::appendElement(StringBuilder& out, Element& element, bool addDisplayInline, StyledMarkupAccumulator::RangeFullySelectsNode rangeFullySelectsNode)
{
    const bool documentIsHTML = element.document().isHTMLDocument();
    appendOpenTag(out, element, 0);

    const bool shouldAnnotateOrForceInline = element.isHTMLElement() && (shouldAnnotate() || addDisplayInline);
    const bool shouldOverrideStyleAttr = shouldAnnotateOrForceInline || shouldApplyWrappingStyle(element);

    AttributeCollection attributes = element.attributes();
    for (const auto& attribute : attributes) {
        // We'll handle the style attribute separately, below.
        if (attribute.name() == styleAttr && shouldOverrideStyleAttr)
            continue;
        appendAttribute(out, element, attribute, 0);
    }

    if (shouldOverrideStyleAttr) {
        RefPtrWillBeRawPtr<EditingStyle> newInlineStyle = nullptr;

        if (shouldApplyWrappingStyle(element)) {
            newInlineStyle = m_serializer->wrappingStyle()->copy();
            newInlineStyle->removePropertiesInElementDefaultStyle(&element);
            newInlineStyle->removeStyleConflictingWithStyleOfElement(&element);
        } else {
            newInlineStyle = EditingStyle::create();
        }

        if (element.isStyledElement() && element.inlineStyle())
            newInlineStyle->overrideWithStyle(element.inlineStyle());

        if (shouldAnnotateOrForceInline) {
            if (shouldAnnotate())
                newInlineStyle->mergeStyleFromRulesForSerialization(&toHTMLElement(element));

            if (&element == m_highestNodeToBeSerialized && m_shouldAnnotate == AnnotateForNavigationTransition)
                newInlineStyle->addAbsolutePositioningFromElement(element);

            if (addDisplayInline)
                newInlineStyle->forceInline();

            // If the node is not fully selected by the range, then we don't want to keep styles that affect its relationship to the nodes around it
            // only the ones that affect it and the nodes within it.
            if (rangeFullySelectsNode == DoesNotFullySelectNode && newInlineStyle->style())
                newInlineStyle->style()->removeProperty(CSSPropertyFloat);
        }

        if (!newInlineStyle->isEmpty()) {
            out.appendLiteral(" style=\"");
            appendAttributeValue(out, newInlineStyle->style()->asText(), documentIsHTML);
            out.append('\"');
        }
    }

    appendCloseTag(out, element);
}

void StyledMarkupAccumulator::appendStyleNodeOpenTag(StringBuilder& out, StylePropertySet* style, bool isBlock)
{
    // wrappingStyleForSerialization should have removed -webkit-text-decorations-in-effect
    ASSERT(propertyMissingOrEqualToNone(style, CSSPropertyWebkitTextDecorationsInEffect));
    if (isBlock)
        out.appendLiteral("<div style=\"");
    else
        out.appendLiteral("<span style=\"");
    ASSERT(m_start.isNotNull());
    appendAttributeValue(out, style->asText(), m_start.document()->isHTMLDocument());
    out.appendLiteral("\">");
}

String StyledMarkupAccumulator::renderedText(Text& textNode)
{
    int startOffset = 0;
    int endOffset = textNode.length();
    if (m_start.containerNode() == textNode)
        startOffset = m_start.offsetInContainerNode();
    if (m_end.containerNode() == textNode)
        endOffset = m_end.offsetInContainerNode();
    return plainText(Position(&textNode, startOffset), Position(&textNode, endOffset));
}

String StyledMarkupAccumulator::stringValueForRange(const Node& node)
{
    if (m_start.isNull())
        return node.nodeValue();

    String str = node.nodeValue();
    if (m_start.containerNode() == node)
        str.truncate(m_end.offsetInContainerNode());
    if (m_end.containerNode() == node)
        str.remove(0, m_start.offsetInContainerNode());
    return str;
}

bool StyledMarkupAccumulator::shouldApplyWrappingStyle(const Node& node) const
{
    return m_highestNodeToBeSerialized && m_highestNodeToBeSerialized->parentNode() == node.parentNode()
        && m_serializer->wrappingStyle() && m_serializer->wrappingStyle()->style();
}

StyledMarkupSerializer::StyledMarkupSerializer(EAbsoluteURLs shouldResolveURLs, EAnnotateForInterchange shouldAnnotate, const Position& start, const Position& end, Node* highestNodeToBeSerialized)
    : m_markupAccumulator(this, shouldResolveURLs, start, end, shouldAnnotate, highestNodeToBeSerialized)
{
}

void StyledMarkupSerializer::wrapWithNode(ContainerNode& node, bool convertBlocksToInlines, StyledMarkupAccumulator::RangeFullySelectsNode rangeFullySelectsNode)
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

void StyledMarkupSerializer::wrapWithStyleNode(StylePropertySet* style, bool isBlock)
{
    StringBuilder openTag;
    m_markupAccumulator.appendStyleNodeOpenTag(openTag, style, isBlock);
    m_reversedPrecedingMarkup.append(openTag.toString());
    appendString(styleNodeCloseTag(isBlock));
}

String StyledMarkupSerializer::takeResults()
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
Node* StyledMarkupSerializer::serializeNodes(Node* startNode, Node* pastEnd)
{
    if (!m_markupAccumulator.highestNodeToBeSerialized()) {
        Node* lastClosed = traverseNodesForSerialization<Strategy>(startNode, pastEnd, DoNotEmitString);
        m_markupAccumulator.setHighestNodeToBeSerialized(lastClosed);
    }

    if (m_markupAccumulator.highestNodeToBeSerialized() && Strategy::parent(*m_markupAccumulator.highestNodeToBeSerialized())) {
        m_wrappingStyle = EditingStyle::wrappingStyleForSerialization(m_markupAccumulator.highestNodeToBeSerialized()->parentNode(), m_markupAccumulator.shouldAnnotate());
        if (m_markupAccumulator.shouldAnnotateForNavigationTransition()) {
            m_wrappingStyle->style()->removeProperty(CSSPropertyBackgroundColor);
            m_wrappingStyle->style()->removeProperty(CSSPropertyBackgroundImage);
        }
    }

    return traverseNodesForSerialization<Strategy>(startNode, pastEnd, EmitString);
}

template<typename Strategy>
Node* StyledMarkupSerializer::traverseNodesForSerialization(Node* startNode, Node* pastEnd, NodeTraversalMode traversalMode)
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

template Node* StyledMarkupSerializer::serializeNodes<EditingStrategy>(Node* startNode, Node* endNode);

} // namespace blink
