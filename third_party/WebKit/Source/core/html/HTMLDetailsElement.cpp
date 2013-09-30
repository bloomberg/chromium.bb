/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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
 *
 */

#include "config.h"
#include "core/html/HTMLDetailsElement.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLSummaryElement.h"
#include "core/html/shadow/HTMLContentElement.h"
#include "core/platform/text/PlatformLocale.h"
#include "core/rendering/RenderBlockFlow.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<HTMLDetailsElement> HTMLDetailsElement::create(const QualifiedName& tagName, Document& document)
{
    RefPtr<HTMLDetailsElement> details = adoptRef(new HTMLDetailsElement(tagName, document));
    details->ensureUserAgentShadowRoot();
    return details.release();
}

HTMLDetailsElement::HTMLDetailsElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_isOpen(false)
{
    ASSERT(hasTagName(detailsTag));
    ScriptWrappable::init(this);
}

RenderObject* HTMLDetailsElement::createRenderer(RenderStyle*)
{
    return new RenderBlockFlow(this);
}

void HTMLDetailsElement::didAddUserAgentShadowRoot(ShadowRoot* root)
{
    DEFINE_STATIC_LOCAL(AtomicString, summarySelector, ("summary:first-of-type", AtomicString::ConstructFromLiteral));

    RefPtr<HTMLSummaryElement> defaultSummary = HTMLSummaryElement::create(summaryTag, document());
    defaultSummary->appendChild(Text::create(document(), locale().queryString(WebKit::WebLocalizedString::DetailsLabel)));

    RefPtr<HTMLContentElement> content = HTMLContentElement::create(document());
    content->setAttribute(selectAttr, summarySelector);
    content->appendChild(defaultSummary);

    root->appendChild(content);
    root->appendChild(HTMLContentElement::create(document()));
}

Element* HTMLDetailsElement::findMainSummary() const
{
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(summaryTag))
            return toElement(child);
    }

    HTMLContentElement* content = toHTMLContentElement(userAgentShadowRoot()->firstChild());
    ASSERT(content->firstChild() && content->firstChild()->hasTagName(summaryTag));
    return toElement(content->firstChild());
}

void HTMLDetailsElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == openAttr) {
        bool oldValue = m_isOpen;
        m_isOpen = !value.isNull();
        if (oldValue != m_isOpen)
            lazyReattachIfAttached();
    } else
        HTMLElement::parseAttribute(name, value);
}

bool HTMLDetailsElement::childShouldCreateRenderer(const Node& child) const
{
    // FIXME: These checks do not look correct, we should just use insertion points instead.
    if (m_isOpen)
        return HTMLElement::childShouldCreateRenderer(child);
    if (!child.hasTagName(summaryTag))
        return false;
    return &child == findMainSummary() && HTMLElement::childShouldCreateRenderer(child);
}

void HTMLDetailsElement::toggleOpen()
{
    setAttribute(openAttr, m_isOpen ? nullAtom : emptyAtom);
}

}
