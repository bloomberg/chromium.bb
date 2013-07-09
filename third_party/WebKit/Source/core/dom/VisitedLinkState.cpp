/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/dom/VisitedLinkState.h"

#include "HTMLNames.h"
#include "core/dom/NodeTraversal.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/page/Page.h"
#include "public/platform/Platform.h"

namespace WebCore {

using namespace HTMLNames;

inline static const AtomicString& linkAttribute(Element* element)
{
    ASSERT(element->isLink());
    if (element->isHTMLElement())
        return element->fastGetAttribute(HTMLNames::hrefAttr);
    ASSERT(element->isSVGElement());
    return element->getAttribute(XLinkNames::hrefAttr);
}

inline static LinkHash linkHashForElement(Document* document, Element* element)
{
    if (isHTMLAnchorElement(element))
        return toHTMLAnchorElement(element)->visitedLinkHash();
    return visitedLinkHash(document->baseURL(), linkAttribute(element));
}

inline static LinkHash linkHashForElementWithAttribute(Document* document, Element* element, const AtomicString& attribute)
{
    ASSERT(linkAttribute(element) == attribute);
    if (isHTMLAnchorElement(element))
        return toHTMLAnchorElement(element)->visitedLinkHash();
    return visitedLinkHash(document->baseURL(), attribute);
}

PassOwnPtr<VisitedLinkState> VisitedLinkState::create(Document* document)
{
    return adoptPtr(new VisitedLinkState(document));
}

VisitedLinkState::VisitedLinkState(Document* document)
    : m_document(document)
{
}

void VisitedLinkState::invalidateStyleForAllLinks()
{
    if (m_linksCheckedForVisitedState.isEmpty())
        return;
    for (Element* element = ElementTraversal::firstWithin(m_document); element; element = ElementTraversal::next(element)) {
        if (element->isLink())
            element->setNeedsStyleRecalc();
    }
}

void VisitedLinkState::invalidateStyleForLink(LinkHash linkHash)
{
    if (!m_linksCheckedForVisitedState.contains(linkHash))
        return;
    for (Element* element = ElementTraversal::firstWithin(m_document); element; element = ElementTraversal::next(element)) {
        if (element->isLink() && linkHashForElement(m_document, element) == linkHash)
            element->setNeedsStyleRecalc();
    }
}

EInsideLink VisitedLinkState::determineLinkStateSlowCase(Element* element)
{
    ASSERT(element->isLink());

    const AtomicString& attribute = linkAttribute(element);

    if (attribute.isNull())
        return NotInsideLink; // This can happen for <img usemap>

    // An empty attribute refers to the document itself which is always
    // visited. It is useful to check this explicitly so that visited
    // links can be tested in platform independent manner, without
    // explicit support in the test harness.
    if (attribute.isEmpty())
        return InsideVisitedLink;

    // We null check the Frame here to avoid canonicalizing and hashing
    // URLs in documents that aren't attached to Frames (like documents
    // from XMLHttpRequest).
    if (!m_document->frame())
        return InsideUnvisitedLink;

    LinkHash hash = linkHashForElementWithAttribute(m_document, element, attribute);
    if (!hash)
        return InsideUnvisitedLink;

    m_linksCheckedForVisitedState.add(hash);
    return WebKit::Platform::current()->isLinkVisited(hash) ? InsideVisitedLink : InsideUnvisitedLink;
}

}
