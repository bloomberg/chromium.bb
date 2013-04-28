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
#include "core/dom/MouseEvent.h"
#include "core/dom/NodeRenderingContext.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/Text.h"
#include "core/html/HTMLSummaryElement.h"
#include "core/html/shadow/HTMLContentElement.h"
#include "core/platform/LocalizedStrings.h"
#include "core/rendering/RenderBlock.h"

namespace WebCore {

using namespace HTMLNames;

static const AtomicString& summaryQuerySelector()
{
    DEFINE_STATIC_LOCAL(AtomicString, selector, ("summary:first-of-type", AtomicString::ConstructFromLiteral));
    return selector;
};

PassRefPtr<HTMLDetailsElement> HTMLDetailsElement::create(const QualifiedName& tagName, Document* document)
{
    RefPtr<HTMLDetailsElement> details = adoptRef(new HTMLDetailsElement(tagName, document));
    details->ensureUserAgentShadowRoot();
    return details.release();
}

HTMLDetailsElement::HTMLDetailsElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
    , m_isOpen(false)
{
    ASSERT(hasTagName(detailsTag));
    ScriptWrappable::init(this);
}

RenderObject* HTMLDetailsElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderBlock(this);
}

void HTMLDetailsElement::didAddUserAgentShadowRoot(ShadowRoot* root)
{
    RefPtr<HTMLSummaryElement> defaultSummary = HTMLSummaryElement::create(summaryTag, document());
    defaultSummary->appendChild(Text::create(document(), defaultDetailsSummaryText()), ASSERT_NO_EXCEPTION);

    RefPtr<HTMLContentElement> content = HTMLContentElement::create(document());
    content->setSelect(summaryQuerySelector());
    content->appendChild(defaultSummary);

    root->appendChild(content, ASSERT_NO_EXCEPTION, AttachLazily);
    root->appendChild(HTMLContentElement::create(document()), ASSERT_NO_EXCEPTION, AttachLazily);
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
            reattachIfAttached();
    } else
        HTMLElement::parseAttribute(name, value);
}

bool HTMLDetailsElement::childShouldCreateRenderer(const NodeRenderingContext& childContext) const
{
    if (!childContext.isOnEncapsulationBoundary())
        return false;

    if (m_isOpen)
        return HTMLElement::childShouldCreateRenderer(childContext);

    if (!childContext.node()->hasTagName(summaryTag))
        return false;

    return childContext.node() == findMainSummary() && HTMLElement::childShouldCreateRenderer(childContext);
}

void HTMLDetailsElement::toggleOpen()
{
    setAttribute(openAttr, m_isOpen ? nullAtom : emptyAtom);
}

}
