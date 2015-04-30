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
#include "core/editing/StyledMarkupAccumulator.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/Text.h"
#include "core/editing/htmlediting.h"
#include "core/editing/iterators/TextIterator.h"
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

TextOffset::TextOffset()
    : m_offset(0)
{
}

TextOffset::TextOffset(PassRefPtrWillBeRawPtr<Text> text, int offset)
    : m_text(text)
    , m_offset(offset)
{
}

TextOffset::TextOffset(const TextOffset& other)
    : m_text(other.m_text)
    , m_offset(other.m_offset)
{
}

bool TextOffset::isNull() const
{
    return !m_text;
}

bool TextOffset::isNotNull() const
{
    return m_text;
}

StyledMarkupAccumulator::StyledMarkupAccumulator(EAbsoluteURLs shouldResolveURLs, const TextOffset& start, const TextOffset& end, const PassRefPtrWillBeRawPtr<Document> document, EAnnotateForInterchange shouldAnnotate, Node* highestNodeToBeSerialized)
    : m_accumulator(shouldResolveURLs)
    , m_start(start)
    , m_end(end)
    , m_document(document)
    , m_shouldAnnotate(shouldAnnotate)
    , m_highestNodeToBeSerialized(highestNodeToBeSerialized)
{
}

void StyledMarkupAccumulator::appendString(const String& str)
{
    m_accumulator.appendString(str);
}

void StyledMarkupAccumulator::appendStartTag(Node& node, Namespaces* namespaces)
{
    StringBuilder out;
    appendStartMarkup(out, node, namespaces);
    appendString(out.toString());
}

void StyledMarkupAccumulator::appendEndTag(const Element& element)
{
    StringBuilder out;
    appendEndMarkup(out, element);
    appendString(out.toString());
}

void StyledMarkupAccumulator::appendStartMarkup(StringBuilder& result, Node& node, Namespaces* namespaces)
{
    switch (node.nodeType()) {
    case Node::TEXT_NODE:
        appendText(result, toText(node));
        break;
    case Node::ELEMENT_NODE:
        appendElement(result, toElement(node), namespaces);
        break;
    default:
        m_accumulator.appendStartMarkup(result, node, namespaces);
        break;
    }
}

void StyledMarkupAccumulator::appendEndMarkup(StringBuilder& result, const Element& element)
{
    m_accumulator.appendEndMarkup(result, element);
}

void StyledMarkupAccumulator::appendText(StringBuilder& out, Text& text)
{
    const bool parentIsTextarea = text.parentElement() && text.parentElement()->tagQName() == textareaTag;
    const bool wrappingSpan = shouldApplyWrappingStyle(text) && !parentIsTextarea;
    if (wrappingSpan) {
        RefPtrWillBeRawPtr<EditingStyle> wrappingStyle = m_wrappingStyle->copy();
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
        if (m_end.isNotNull()) {
            if (text == m_end.text())
                length = m_end.offset();
        }
        if (m_start.isNotNull()) {
            if (text == m_start.text()) {
                start = m_start.offset();
                length -= start;
            }
        }
        m_accumulator.appendCharactersReplacingEntities(out, str, start, length, m_accumulator.entityMaskForText(text));
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
    m_accumulator.appendOpenTag(out, element, 0);

    const bool shouldAnnotateOrForceInline = element.isHTMLElement() && (shouldAnnotate() || addDisplayInline);
    const bool shouldOverrideStyleAttr = shouldAnnotateOrForceInline || shouldApplyWrappingStyle(element);

    AttributeCollection attributes = element.attributes();
    for (const auto& attribute : attributes) {
        // We'll handle the style attribute separately, below.
        if (attribute.name() == styleAttr && shouldOverrideStyleAttr)
            continue;
        m_accumulator.appendAttribute(out, element, attribute, 0);
    }

    if (shouldOverrideStyleAttr) {
        RefPtrWillBeRawPtr<EditingStyle> newInlineStyle = nullptr;

        if (shouldApplyWrappingStyle(element)) {
            newInlineStyle = m_wrappingStyle->copy();
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
            m_accumulator.appendAttributeValue(out, newInlineStyle->style()->asText(), documentIsHTML);
            out.append('\"');
        }
    }

    m_accumulator.appendCloseTag(out, element);
}

void StyledMarkupAccumulator::appendStyleNodeOpenTag(StringBuilder& out, StylePropertySet* style, bool isBlock)
{
    // wrappingStyleForSerialization should have removed -webkit-text-decorations-in-effect
    ASSERT(propertyMissingOrEqualToNone(style, CSSPropertyWebkitTextDecorationsInEffect));
    if (isBlock)
        out.appendLiteral("<div style=\"");
    else
        out.appendLiteral("<span style=\"");
    ASSERT(m_document);
    m_accumulator.appendAttributeValue(out, style->asText(), m_document->isHTMLDocument());
    out.appendLiteral("\">");
}

void StyledMarkupAccumulator::concatenateMarkup(StringBuilder& result) const
{
    m_accumulator.concatenateMarkup(result);
}

String StyledMarkupAccumulator::renderedText(Text& textNode)
{
    int startOffset = 0;
    int endOffset = textNode.length();
    if (m_start.text() == textNode)
        startOffset = m_start.offset();
    if (m_end.text() == textNode)
        endOffset = m_end.offset();
    return plainText(Position(&textNode, startOffset), Position(&textNode, endOffset));
}

String StyledMarkupAccumulator::stringValueForRange(const Text& node)
{
    if (m_start.isNull())
        return node.data();

    String str = node.data();
    if (m_start.text() == node)
        str.truncate(m_end.offset());
    if (m_end.text() == node)
        str.remove(0, m_start.offset());
    return str;
}

bool StyledMarkupAccumulator::shouldApplyWrappingStyle(const Node& node) const
{
    // TODO(hajimehoshi): Use Strategy
    return m_highestNodeToBeSerialized && m_highestNodeToBeSerialized->parentNode() == node.parentNode()
        && m_wrappingStyle && m_wrappingStyle->style();
}

} // namespace blink
