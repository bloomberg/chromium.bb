/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/parser/HTMLTreeBuilderSimulator.h"

#include "core/HTMLNames.h"
#include "core/MathMLNames.h"
#include "core/SVGNames.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "core/html/parser/HTMLTreeBuilder.h"

namespace blink {

using namespace HTMLNames;

static bool tokenExitsForeignContent(const CompactHTMLToken& token)
{
    // FIXME: This is copied from HTMLTreeBuilder::processTokenInForeignContent and changed to use threadSafeHTMLNamesMatch.
    const String& tagName = token.data();
    return threadSafeMatch(tagName, bTag)
        || threadSafeMatch(tagName, bigTag)
        || threadSafeMatch(tagName, blockquoteTag)
        || threadSafeMatch(tagName, bodyTag)
        || threadSafeMatch(tagName, brTag)
        || threadSafeMatch(tagName, centerTag)
        || threadSafeMatch(tagName, codeTag)
        || threadSafeMatch(tagName, ddTag)
        || threadSafeMatch(tagName, divTag)
        || threadSafeMatch(tagName, dlTag)
        || threadSafeMatch(tagName, dtTag)
        || threadSafeMatch(tagName, emTag)
        || threadSafeMatch(tagName, embedTag)
        || threadSafeMatch(tagName, h1Tag)
        || threadSafeMatch(tagName, h2Tag)
        || threadSafeMatch(tagName, h3Tag)
        || threadSafeMatch(tagName, h4Tag)
        || threadSafeMatch(tagName, h5Tag)
        || threadSafeMatch(tagName, h6Tag)
        || threadSafeMatch(tagName, headTag)
        || threadSafeMatch(tagName, hrTag)
        || threadSafeMatch(tagName, iTag)
        || threadSafeMatch(tagName, imgTag)
        || threadSafeMatch(tagName, liTag)
        || threadSafeMatch(tagName, listingTag)
        || threadSafeMatch(tagName, menuTag)
        || threadSafeMatch(tagName, metaTag)
        || threadSafeMatch(tagName, nobrTag)
        || threadSafeMatch(tagName, olTag)
        || threadSafeMatch(tagName, pTag)
        || threadSafeMatch(tagName, preTag)
        || threadSafeMatch(tagName, rubyTag)
        || threadSafeMatch(tagName, sTag)
        || threadSafeMatch(tagName, smallTag)
        || threadSafeMatch(tagName, spanTag)
        || threadSafeMatch(tagName, strongTag)
        || threadSafeMatch(tagName, strikeTag)
        || threadSafeMatch(tagName, subTag)
        || threadSafeMatch(tagName, supTag)
        || threadSafeMatch(tagName, tableTag)
        || threadSafeMatch(tagName, ttTag)
        || threadSafeMatch(tagName, uTag)
        || threadSafeMatch(tagName, ulTag)
        || threadSafeMatch(tagName, varTag)
        || (threadSafeMatch(tagName, fontTag) && (token.getAttributeItem(colorAttr) || token.getAttributeItem(faceAttr) || token.getAttributeItem(sizeAttr)));
}

static bool tokenExitsSVG(const CompactHTMLToken& token)
{
    // FIXME: It's very fragile that we special case foreignObject here to be case-insensitive.
    return equalIgnoringCase(token.data(), SVGNames::foreignObjectTag.localName());
}

static bool tokenExitsMath(const CompactHTMLToken& token)
{
    // FIXME: This is copied from HTMLElementStack::isMathMLTextIntegrationPoint and changed to use threadSafeMatch.
    const String& tagName = token.data();
    return threadSafeMatch(tagName, MathMLNames::miTag)
        || threadSafeMatch(tagName, MathMLNames::moTag)
        || threadSafeMatch(tagName, MathMLNames::mnTag)
        || threadSafeMatch(tagName, MathMLNames::msTag)
        || threadSafeMatch(tagName, MathMLNames::mtextTag);
}

// We always push tokens which may be related to elements which are
// HTML integration points. elementMayBeHTMLIntegrationPoint gives
// conservative false positives. Specifically, annotation-xml end tags
// may not be related to HTML integration points; it depends on the
// opening tags' attributes. But elementMayBeHTMLIntegrationPoint
// returns true for these elements.
static bool elementMayBeHTMLIntegrationPoint(const String& tagName)
{
    return threadSafeMatch(tagName, MathMLNames::annotation_xmlTag)
        || threadSafeMatch(tagName, SVGNames::descTag)
        || threadSafeMatch(tagName, SVGNames::foreignObjectTag)
        || threadSafeMatch(tagName, titleTag);
}

// https://html.spec.whatwg.org/#html-integration-point
// See also HTMLElementStack::isHTMLIntegrationPoint
static bool tokenStartsHTMLIntegrationPoint(const CompactHTMLToken& token)
{
    if (token.type() != HTMLToken::StartTag)
        return false;

    const String& tagName = token.data();
    if (threadSafeMatch(tagName, MathMLNames::annotation_xmlTag)) {
        if (const CompactHTMLToken::Attribute* encoding = token.getAttributeItem(MathMLNames::encodingAttr)) {
            return equalIgnoringCase(encoding->value(), "text/html")
                || equalIgnoringCase(encoding->value(), "application/xhtml+xml");
        }
        return false;
    }

    return threadSafeMatch(tagName, SVGNames::descTag)
        || threadSafeMatch(tagName, SVGNames::foreignObjectTag)
        || threadSafeMatch(tagName, titleTag);
}

HTMLTreeBuilderSimulator::HTMLTreeBuilderSimulator(const HTMLParserOptions& options)
    : m_options(options)
{
    m_stack.append(StateFlags {HTML, false});
}

HTMLTreeBuilderSimulator::State HTMLTreeBuilderSimulator::stateFor(HTMLTreeBuilder* treeBuilder)
{
    ASSERT(isMainThread());
    State stack;
    for (HTMLElementStack::ElementRecord* record = treeBuilder->openElements()->topRecord(); record; record = record->next()) {
        Namespace recordNamespace = HTML;
        if (record->namespaceURI() == SVGNames::svgNamespaceURI)
            recordNamespace = SVG;
        else if (record->namespaceURI() == MathMLNames::mathmlNamespaceURI)
            recordNamespace = MathML;

        if (stack.isEmpty() || static_cast<Namespace>(stack.last().ns) != recordNamespace || elementMayBeHTMLIntegrationPoint(record->stackItem()->localName()))
            stack.append(StateFlags {static_cast<unsigned>(recordNamespace), HTMLElementStack::isHTMLIntegrationPoint(record->stackItem())});
    }
    stack.reverse();
    return stack;
}

HTMLTreeBuilderSimulator::SimulatedToken HTMLTreeBuilderSimulator::simulate(const CompactHTMLToken& token, HTMLTokenizer* tokenizer)
{
    SimulatedToken simulatedToken = OtherToken;

    if (token.type() == HTMLToken::StartTag) {
        const String& tagName = token.data();
        bool currentNodeIsHTMLIntegrationPoint = m_stack.last().isHTMLIntegrationPoint;
        Namespace currentNodeNamespace = currentNamespace();

        if (inForeignContent() && tokenExitsForeignContent(token))
            m_stack.removeLast();

        if (threadSafeMatch(tagName, SVGNames::svgTag))
            m_stack.append(StateFlags {SVG, tokenStartsHTMLIntegrationPoint(token)});
        else if (threadSafeMatch(tagName, MathMLNames::mathTag))
            m_stack.append(StateFlags {MathML, tokenStartsHTMLIntegrationPoint(token)});
        else if ((currentNodeNamespace == SVG && tokenExitsSVG(token))
            || (currentNodeNamespace == MathML && tokenExitsMath(token)))
            m_stack.append(StateFlags {HTML, tokenStartsHTMLIntegrationPoint(token)});
        else if (elementMayBeHTMLIntegrationPoint(token.data()) != currentNodeIsHTMLIntegrationPoint)
            m_stack.append(StateFlags {static_cast<unsigned>(currentNodeNamespace), tokenStartsHTMLIntegrationPoint(token)});

        if (!inForeignContent() || currentNodeIsHTMLIntegrationPoint) {
            // FIXME: This is just a copy of Tokenizer::updateStateFor which uses threadSafeMatches.
            if (threadSafeMatch(tagName, textareaTag) || threadSafeMatch(tagName, titleTag)) {
                tokenizer->setState(HTMLTokenizer::RCDATAState);
            } else if (threadSafeMatch(tagName, plaintextTag)) {
                tokenizer->setState(HTMLTokenizer::PLAINTEXTState);
            } else if (threadSafeMatch(tagName, scriptTag)) {
                tokenizer->setState(HTMLTokenizer::ScriptDataState);
                simulatedToken = ScriptStart;
            } else if (threadSafeMatch(tagName, styleTag)
                || threadSafeMatch(tagName, iframeTag)
                || threadSafeMatch(tagName, xmpTag)
                || (threadSafeMatch(tagName, noembedTag) && m_options.pluginsEnabled)
                || threadSafeMatch(tagName, noframesTag)
                || (threadSafeMatch(tagName, noscriptTag) && m_options.scriptEnabled)) {
                tokenizer->setState(HTMLTokenizer::RAWTEXTState);
            }
        }
    }

    if (token.type() == HTMLToken::EndTag) {
        const String& tagName = token.data();
        if ((currentNamespace() == SVG && threadSafeMatch(tagName, SVGNames::svgTag))
            || (currentNamespace() == MathML && threadSafeMatch(tagName, MathMLNames::mathTag))
            || (stackContainsNamespace(SVG) && currentNamespace() == HTML && tokenExitsSVG(token))
            || (stackContainsNamespace(MathML) && currentNamespace() == HTML && tokenExitsMath(token))
            // By checking the namespace, the above tests subtly avoid
            // popping the base stack entry which is 'HTML'. Because
            // HTML title is an integration point, we must explicitly
            // check we are not popping the base entry when presented
            // malformed input like </title> with no opening tag.
            || (m_stack.size() > 1 && elementMayBeHTMLIntegrationPoint(token.data()) != static_cast<bool>(m_stack.last().isHTMLIntegrationPoint)))
            m_stack.removeLast();

        if (threadSafeMatch(tagName, scriptTag)) {
            if (!inForeignContent())
                tokenizer->setState(HTMLTokenizer::DataState);
            return ScriptEnd;
        }
    }

    // FIXME: Also setForceNullCharacterReplacement when in text mode.
    tokenizer->setForceNullCharacterReplacement(inForeignContent());
    tokenizer->setShouldAllowCDATA(inForeignContent());
    return simulatedToken;
}

} // namespace blink
