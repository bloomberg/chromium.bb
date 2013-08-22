/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
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
#include "core/html/HTMLTitleElement.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Text.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLTitleElement::HTMLTitleElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(titleTag));
    setHasCustomStyleCallbacks();
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLTitleElement> HTMLTitleElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLTitleElement(tagName, document));
}

Node::InsertionNotificationRequest HTMLTitleElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (inDocument() && !isInShadowTree())
        document()->setTitleElement(textWithDirection(), this);
    return InsertionDone;
}

void HTMLTitleElement::removedFrom(ContainerNode* insertionPoint)
{
    HTMLElement::removedFrom(insertionPoint);
    if (insertionPoint->inDocument() && !insertionPoint->isInShadowTree())
        document()->removeTitle(this);
}

void HTMLTitleElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (inDocument() && !isInShadowTree())
        document()->setTitleElement(textWithDirection(), this);
}

void HTMLTitleElement::attach(const AttachContext& context)
{
    HTMLElement::attach(context);
    // If after attaching nothing called styleForRenderer() on this node we
    // manually cache the value. This happens if our parent doesn't have a
    // renderer like <optgroup> or if it doesn't allow children like <select>.
    if (!m_style)
        updateNonRenderStyle();
}

void HTMLTitleElement::detach(const AttachContext& context)
{
    m_style.clear();
    HTMLElement::detach(context);
}

void HTMLTitleElement::updateNonRenderStyle()
{
    m_style = document()->styleForElementIgnoringPendingStylesheets(this);
}

RenderStyle* HTMLTitleElement::nonRendererStyle() const
{
    return m_style.get();
}

PassRefPtr<RenderStyle> HTMLTitleElement::customStyleForRenderer()
{
    // styleForRenderer is called whenever a new style should be associated
    // with an Element so now is a good time to update our cached style.
    updateNonRenderStyle();
    return m_style;
}

void HTMLTitleElement::didRecalcStyle(StyleChange)
{
    if (isInShadowTree())
        return;

    document()->setTitleElement(textWithDirection(), this);
}

String HTMLTitleElement::text() const
{
    StringBuilder result;

    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            result.append(toText(n)->data());
    }

    return result.toString();
}

StringWithDirection HTMLTitleElement::textWithDirection()
{
    TextDirection direction = LTR;
    if (m_style)
        direction = m_style->direction();
    return StringWithDirection(text(), direction);
}

void HTMLTitleElement::setText(const String &value)
{
    RefPtr<Node> protectFromMutationEvents(this);

    int numChildren = childNodeCount();

    if (numChildren == 1 && firstChild()->isTextNode())
        toText(firstChild())->setData(value);
    else {
        // We make a copy here because entity of "value" argument can be Document::m_title,
        // which goes empty during removeChildren() invocation below,
        // which causes HTMLTitleElement::childrenChanged(), which ends up Document::setTitle().
        String valueCopy(value);

        if (numChildren > 0)
            removeChildren();

        appendChild(document()->createTextNode(valueCopy.impl()), IGNORE_EXCEPTION);
    }
}

}
