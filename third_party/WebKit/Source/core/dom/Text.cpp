/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "core/dom/Text.h"

#include "SVGNames.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeRenderingContext.h"
#include "core/dom/ScopedEventQueue.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/rendering/RenderCombineText.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/svg/RenderSVGInlineText.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"

using namespace std;

namespace WebCore {

PassRefPtr<Text> Text::create(Document& document, const String& data)
{
    return adoptRef(new Text(document, data, CreateText));
}

PassRefPtr<Text> Text::createEditingText(Document& document, const String& data)
{
    return adoptRef(new Text(document, data, CreateEditingText));
}

PassRefPtr<Text> Text::splitText(unsigned offset, ExceptionState& es)
{
    // IndexSizeError: Raised if the specified offset is negative or greater than
    // the number of 16-bit units in data.
    if (offset > length()) {
        es.throwDOMException(IndexSizeError, ExceptionMessages::failedToExecute("splitText", "Text", "The offset " + String::number(offset) + " is larger than the Text node's length."));
        return 0;
    }

    EventQueueScope scope;
    String oldStr = data();
    RefPtr<Text> newText = cloneWithData(oldStr.substring(offset));
    setDataWithoutUpdate(oldStr.substring(0, offset));

    didModifyData(oldStr);

    if (parentNode())
        parentNode()->insertBefore(newText.get(), nextSibling(), es);
    if (es.hadException())
        return 0;

    if (renderer())
        toRenderText(renderer())->setTextWithOffset(dataImpl(), 0, oldStr.length());

    if (parentNode())
        document().didSplitTextNode(this);

    return newText.release();
}

static const Text* earliestLogicallyAdjacentTextNode(const Text* t)
{
    const Node* n = t;
    while ((n = n->previousSibling())) {
        Node::NodeType type = n->nodeType();
        if (type == Node::TEXT_NODE || type == Node::CDATA_SECTION_NODE) {
            t = toText(n);
            continue;
        }

        break;
    }
    return t;
}

static const Text* latestLogicallyAdjacentTextNode(const Text* t)
{
    const Node* n = t;
    while ((n = n->nextSibling())) {
        Node::NodeType type = n->nodeType();
        if (type == Node::TEXT_NODE || type == Node::CDATA_SECTION_NODE) {
            t = toText(n);
            continue;
        }

        break;
    }
    return t;
}

String Text::wholeText() const
{
    const Text* startText = earliestLogicallyAdjacentTextNode(this);
    const Text* endText = latestLogicallyAdjacentTextNode(this);

    Node* onePastEndText = endText->nextSibling();
    unsigned resultLength = 0;
    for (const Node* n = startText; n != onePastEndText; n = n->nextSibling()) {
        if (!n->isTextNode())
            continue;
        const String& data = toText(n)->data();
        if (std::numeric_limits<unsigned>::max() - data.length() < resultLength)
            CRASH();
        resultLength += data.length();
    }
    StringBuilder result;
    result.reserveCapacity(resultLength);
    for (const Node* n = startText; n != onePastEndText; n = n->nextSibling()) {
        if (!n->isTextNode())
            continue;
        result.append(toText(n)->data());
    }
    ASSERT(result.length() == resultLength);

    return result.toString();
}

PassRefPtr<Text> Text::replaceWholeText(const String& newText)
{
    // Remove all adjacent text nodes, and replace the contents of this one.

    // Protect startText and endText against mutation event handlers removing the last ref
    RefPtr<Text> startText = const_cast<Text*>(earliestLogicallyAdjacentTextNode(this));
    RefPtr<Text> endText = const_cast<Text*>(latestLogicallyAdjacentTextNode(this));

    RefPtr<Text> protectedThis(this); // Mutation event handlers could cause our last ref to go away
    RefPtr<ContainerNode> parent = parentNode(); // Protect against mutation handlers moving this node during traversal
    for (RefPtr<Node> n = startText; n && n != this && n->isTextNode() && n->parentNode() == parent;) {
        RefPtr<Node> nodeToRemove(n.release());
        n = nodeToRemove->nextSibling();
        parent->removeChild(nodeToRemove.get(), IGNORE_EXCEPTION);
    }

    if (this != endText) {
        Node* onePastEndText = endText->nextSibling();
        for (RefPtr<Node> n = nextSibling(); n && n != onePastEndText && n->isTextNode() && n->parentNode() == parent;) {
            RefPtr<Node> nodeToRemove(n.release());
            n = nodeToRemove->nextSibling();
            parent->removeChild(nodeToRemove.get(), IGNORE_EXCEPTION);
        }
    }

    if (newText.isEmpty()) {
        if (parent && parentNode() == parent)
            parent->removeChild(this, IGNORE_EXCEPTION);
        return 0;
    }

    setData(newText);
    return protectedThis.release();
}

String Text::nodeName() const
{
    return textAtom.string();
}

Node::NodeType Text::nodeType() const
{
    return TEXT_NODE;
}

PassRefPtr<Node> Text::cloneNode(bool /*deep*/)
{
    return cloneWithData(data());
}

bool Text::textRendererIsNeeded(const NodeRenderingContext& context)
{
    if (isEditingText())
        return true;

    if (!length())
        return false;

    if (context.style()->display() == NONE)
        return false;

    if (!containsOnlyWhitespace())
        return true;

    RenderObject* parent = context.parentRenderer();
    if (!parent->canHaveWhitespaceChildren())
        return false;

    if (context.style()->preserveNewline()) // pre/pre-wrap/pre-line always make renderers.
        return true;

    RenderObject* prev = context.previousRenderer();
    if (prev && prev->isBR()) // <span><br/> <br/></span>
        return false;

    if (parent->isRenderInline()) {
        // <span><div/> <div/></span>
        if (prev && !prev->isInline())
            return false;
    } else {
        if (parent->isRenderBlock() && !parent->childrenInline() && (!prev || !prev->isInline()))
            return false;

        // Avoiding creation of a Renderer for the text node is a non-essential memory optimization.
        // So to avoid blowing up on very wide DOMs, we limit the number of siblings to visit.
        unsigned maxSiblingsToVisit = 50;

        RenderObject* first = parent->firstChild();
        while (first && first->isFloatingOrOutOfFlowPositioned() && maxSiblingsToVisit--)
            first = first->nextSibling();
        if (!first || context.nextRenderer() == first)
            // Whitespace at the start of a block just goes away.  Don't even
            // make a render object for this text.
            return false;
    }
    return true;
}

static bool isSVGText(Text* text)
{
    Node* parentOrShadowHostNode = text->parentOrShadowHostNode();
    return parentOrShadowHostNode->isSVGElement() && !parentOrShadowHostNode->hasTagName(SVGNames::foreignObjectTag);
}

RenderText* Text::createTextRenderer(RenderStyle* style)
{
    if (isSVGText(this))
        return new RenderSVGInlineText(this, dataImpl());

    if (style->hasTextCombine())
        return new RenderCombineText(this, dataImpl());

    return new RenderText(this, dataImpl());
}

void Text::attach(const AttachContext& context)
{
    NodeRenderingContext(this, context.resolvedStyle).createRendererForTextIfNeeded();
    CharacterData::attach(context);
}

bool Text::recalcTextStyle(StyleRecalcChange change)
{
    if (RenderText* renderer = toRenderText(this->renderer())) {
        if (change != NoChange || needsStyleRecalc())
            renderer->setStyle(document().styleResolver()->styleForText(this));
        if (needsStyleRecalc())
            renderer->setText(dataImpl());
        clearNeedsStyleRecalc();
    } else if (needsStyleRecalc() || needsWhitespaceRenderer()) {
        reattach();
        return true;
    }
    return false;
}

// If a whitespace node had no renderer and goes through a recalcStyle it may
// need to create one if the parent style now has white-space: pre.
bool Text::needsWhitespaceRenderer()
{
    ASSERT(!renderer());
    if (RenderStyle* style = parentRenderStyle())
        return style->preserveNewline();
    return false;
}

void Text::updateTextRenderer(unsigned offsetOfReplacedData, unsigned lengthOfReplacedData, RecalcStyleBehavior recalcStyleBehavior)
{
    if (!attached())
        return;
    RenderText* textRenderer = toRenderText(renderer());
    if (!textRenderer || !textRendererIsNeeded(NodeRenderingContext(this, textRenderer->style()))) {
        lazyReattach();
        // FIXME: Editing should be updated so this is not neccesary.
        if (recalcStyleBehavior == DeprecatedRecalcStyleImmediatlelyForEditing)
            document().updateStyleIfNeeded();
        return;
    }
    textRenderer->setTextWithOffset(dataImpl(), offsetOfReplacedData, lengthOfReplacedData);
}

bool Text::childTypeAllowed(NodeType) const
{
    return false;
}

PassRefPtr<Text> Text::cloneWithData(const String& data)
{
    return create(document(), data);
}

PassRefPtr<Text> Text::createWithLengthLimit(Document& document, const String& data, unsigned start, unsigned lengthLimit)
{
    unsigned dataLength = data.length();

    if (!start && dataLength <= lengthLimit)
        return create(document, data);

    RefPtr<Text> result = Text::create(document, String());
    result->parserAppendData(data, start, lengthLimit);

    return result;
}

#ifndef NDEBUG
void Text::formatForDebugger(char *buffer, unsigned length) const
{
    StringBuilder result;
    String s;

    result.append(nodeName());

    s = data();
    if (s.length() > 0) {
        if (result.length())
            result.appendLiteral("; ");
        result.appendLiteral("value=");
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}
#endif

} // namespace WebCore
