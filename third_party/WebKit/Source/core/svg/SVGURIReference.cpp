/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGURIReference.h"

#include "core/XLinkNames.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/svg/SVGElement.h"
#include "platform/weborigin/KURL.h"

namespace blink {

SVGURIReference::SVGURIReference(SVGElement* element)
    : m_href(SVGAnimatedHref::create(element))
{
    ASSERT(element);
    m_href->addToPropertyMap(element);
}

DEFINE_TRACE(SVGURIReference)
{
    visitor->trace(m_href);
}

bool SVGURIReference::isKnownAttribute(const QualifiedName& attrName)
{
    return SVGAnimatedHref::isKnownAttribute(attrName);
}

const AtomicString& SVGURIReference::legacyHrefString(const SVGElement& element)
{
    if (element.hasAttribute(SVGNames::hrefAttr))
        return element.getAttribute(SVGNames::hrefAttr);
    return element.getAttribute(XLinkNames::hrefAttr);
}

KURL SVGURIReference::legacyHrefURL(const Document& document) const
{
    return document.completeURL(stripLeadingAndTrailingHTMLSpaces(hrefString()));
}

AtomicString SVGURIReference::fragmentIdentifierFromIRIString(const String& urlString, const TreeScope& treeScope)
{
    const Document& document = treeScope.document();

    KURL url = document.completeURL(urlString);
    if (!url.hasFragmentIdentifier() || !equalIgnoringFragmentIdentifier(url, document.url()))
        return emptyAtom;
    return AtomicString(url.fragmentIdentifier());
}

Element* SVGURIReference::targetElementFromIRIString(const String& urlString, const TreeScope& treeScope, AtomicString* fragmentIdentifier, Document* externalDocument)
{
    const Document& document = treeScope.document();

    KURL url = document.completeURL(urlString);
    if (!url.hasFragmentIdentifier())
        return nullptr;

    AtomicString id(url.fragmentIdentifier());
    if (fragmentIdentifier)
        *fragmentIdentifier = id;

    if (externalDocument) {
        // Enforce that the referenced url matches the url of the document that we've loaded for it!
        ASSERT(equalIgnoringFragmentIdentifier(url, externalDocument->url()));
        return externalDocument->getElementById(id);
    }

    // Exit early if the referenced url is external, and we have no externalDocument given.
    if (!equalIgnoringFragmentIdentifier(url, document.url()))
        return nullptr;

    return treeScope.getElementById(id);
}

} // namespace blink
