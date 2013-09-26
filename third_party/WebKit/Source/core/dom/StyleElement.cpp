/*
 * Copyright (C) 2006, 2007 Rob Buis
 * Copyright (C) 2008 Apple, Inc. All rights reserved.
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
#include "core/dom/StyleElement.h"

#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/StyleEngine.h"
#include "core/html/HTMLStyleElement.h"
#include "core/page/ContentSecurityPolicy.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"

namespace WebCore {

static bool isCSS(Element* element, const AtomicString& type)
{
    return type.isEmpty() || (element->isHTMLElement() ? equalIgnoringCase(type, "text/css") : (type == "text/css"));
}

StyleElement::StyleElement(Document* document, bool createdByParser)
    : m_createdByParser(createdByParser)
    , m_loading(false)
    , m_startPosition(TextPosition::belowRangePosition())
{
    if (createdByParser && document && document->scriptableDocumentParser() && !document->isInDocumentWrite())
        m_startPosition = document->scriptableDocumentParser()->textPosition();
}

StyleElement::~StyleElement()
{
    if (m_sheet)
        clearSheet();
}

void StyleElement::processStyleSheet(Document& document, Element* element)
{
    ASSERT(element);
    document.styleEngine()->addStyleSheetCandidateNode(element, m_createdByParser);
    if (m_createdByParser)
        return;

    process(element);
}

void StyleElement::removedFromDocument(Document& document, Element* element, ContainerNode* scopingNode)
{
    ASSERT(element);
    document.styleEngine()->removeStyleSheetCandidateNode(element, scopingNode);

    RefPtr<StyleSheet> removedSheet = m_sheet;

    if (m_sheet)
        clearSheet();

    // If we're in document teardown, then we don't need to do any notification of our sheet's removal.
    if (document.renderer())
        document.removedStyleSheet(removedSheet.get());
}

void StyleElement::clearDocumentData(Document& document, Element* element)
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (element->inDocument())
        document.styleEngine()->removeStyleSheetCandidateNode(element, isHTMLStyleElement(element) ? toHTMLStyleElement(element)->scopingNode() :  0);
}

void StyleElement::childrenChanged(Element* element)
{
    ASSERT(element);
    if (m_createdByParser)
        return;

    process(element);
}

void StyleElement::finishParsingChildren(Element* element)
{
    ASSERT(element);
    process(element);
    m_createdByParser = false;
}

void StyleElement::process(Element* element)
{
    if (!element || !element->inDocument())
        return;
    createSheet(element, element->textFromChildren());
}

void StyleElement::clearSheet()
{
    ASSERT(m_sheet);
    m_sheet.release()->clearOwnerNode();
}

void StyleElement::createSheet(Element* e, const String& text)
{
    ASSERT(e);
    ASSERT(e->inDocument());
    Document& document = e->document();
    if (m_sheet) {
        if (m_sheet->isLoading())
            document.styleEngine()->removePendingSheet(e);
        clearSheet();
    }

    // If type is empty or CSS, this is a CSS style sheet.
    const AtomicString& type = this->type();
    bool passesContentSecurityPolicyChecks = document.contentSecurityPolicy()->allowStyleNonce(e->fastGetAttribute(HTMLNames::nonceAttr)) || document.contentSecurityPolicy()->allowInlineStyle(e->document().url(), m_startPosition.m_line);
    if (isCSS(e, type) && passesContentSecurityPolicyChecks) {
        RefPtr<MediaQuerySet> mediaQueries = MediaQuerySet::create(media());

        MediaQueryEvaluator screenEval("screen", true);
        MediaQueryEvaluator printEval("print", true);
        if (screenEval.eval(mediaQueries.get()) || printEval.eval(mediaQueries.get())) {
            document.styleEngine()->addPendingSheet();
            m_loading = true;

            TextPosition startPosition = m_startPosition == TextPosition::belowRangePosition() ? TextPosition::minimumPosition() : m_startPosition;
            m_sheet = CSSStyleSheet::createInline(e, KURL(), startPosition, document.inputEncoding());
            m_sheet->setMediaQueries(mediaQueries.release());
            m_sheet->setTitle(e->title());
            m_sheet->contents()->parseStringAtPosition(text, startPosition, m_createdByParser);

            m_loading = false;
        }
    }

    if (m_sheet)
        m_sheet->contents()->checkLoaded();
}

bool StyleElement::isLoading() const
{
    if (m_loading)
        return true;
    return m_sheet ? m_sheet->isLoading() : false;
}

bool StyleElement::sheetLoaded(Document& document)
{
    if (isLoading())
        return false;

    document.styleEngine()->removePendingSheet(m_sheet->ownerNode());
    return true;
}

void StyleElement::startLoadingDynamicSheet(Document& document)
{
    document.styleEngine()->addPendingSheet();
}

}
