/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "core/html/HTMLLabelElement.h"

#include "HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/events/Event.h"
#include "core/events/EventNames.h"
#include "core/html/FormAssociatedElement.h"

namespace WebCore {

using namespace HTMLNames;

static bool supportsLabels(Element* element)
{
    if (!element || !element->isHTMLElement())
        return false;
    if (!toHTMLElement(element)->isLabelable())
        return false;
    return toLabelableElement(element)->supportLabels();
}

inline HTMLLabelElement::HTMLLabelElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(labelTag));
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLLabelElement> HTMLLabelElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLLabelElement(tagName, document));
}

bool HTMLLabelElement::rendererIsFocusable() const
{
    HTMLLabelElement* that = const_cast<HTMLLabelElement*>(this);
    return that->isContentEditable();
}

LabelableElement* HTMLLabelElement::control()
{
    const AtomicString& controlId = getAttribute(forAttr);
    if (controlId.isNull()) {
        // Search the children and descendants of the label element for a form element.
        // per http://dev.w3.org/html5/spec/Overview.html#the-label-element
        // the form element must be "labelable form-associated element".
        Element* element = this;
        while ((element = ElementTraversal::next(element, this))) {
            if (!supportsLabels(element))
                continue;
            return toLabelableElement(element);
        }
        return 0;
    }

    if (Element* element = treeScope().getElementById(controlId)) {
        if (supportsLabels(element))
            return toLabelableElement(element);
    }

    return 0;
}

HTMLFormElement* HTMLLabelElement::form() const
{
    return FormAssociatedElement::findAssociatedForm(this, 0);
}

void HTMLLabelElement::setActive(bool down, bool pause)
{
    if (down == active())
        return;

    // Update our status first.
    HTMLElement::setActive(down, pause);

    // Also update our corresponding control.
    if (HTMLElement* element = control())
        element->setActive(down, pause);
}

void HTMLLabelElement::setHovered(bool over)
{
    if (over == hovered())
        return;

    // Update our status first.
    HTMLElement::setHovered(over);

    // Also update our corresponding control.
    if (HTMLElement* element = control())
        element->setHovered(over);
}

void HTMLLabelElement::defaultEventHandler(Event* evt)
{
    static bool processingClick = false;

    if (evt->type() == eventNames().clickEvent && !processingClick) {
        RefPtr<HTMLElement> element = control();

        // If we can't find a control or if the control received the click
        // event, then there's no need for us to do anything.
        if (!element || (evt->target() && element->containsIncludingShadowDOM(evt->target()->toNode())))
            return;

        processingClick = true;

        document().updateLayoutIgnorePendingStylesheets();
        if (element->isMouseFocusable())
            element->focus(true, FocusDirectionMouse);

        // Click the corresponding control.
        element->dispatchSimulatedClick(evt);

        processingClick = false;

        evt->setDefaultHandled();
    }

    HTMLElement::defaultEventHandler(evt);
}

bool HTMLLabelElement::willRespondToMouseClickEvents()
{
    if (control() && control()->willRespondToMouseClickEvents())
        return true;

    return HTMLElement::willRespondToMouseClickEvents();
}

void HTMLLabelElement::focus(bool, FocusDirection direction)
{
    // to match other browsers, always restore previous selection
    if (HTMLElement* element = control())
        element->focus(true, direction);
    if (isFocusable())
        HTMLElement::focus(true, direction);
}

void HTMLLabelElement::accessKeyAction(bool sendMouseEvents)
{
    if (HTMLElement* element = control())
        element->accessKeyAction(sendMouseEvents);
    else
        HTMLElement::accessKeyAction(sendMouseEvents);
}

} // namespace
