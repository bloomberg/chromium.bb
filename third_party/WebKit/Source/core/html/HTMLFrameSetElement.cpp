/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
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
#include "core/html/HTMLFrameSetElement.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "bindings/v8/ScriptEventListener.h"
#include "core/dom/Document.h"
#include "core/dom/Event.h"
#include "core/dom/EventNames.h"
#include "core/dom/MouseEvent.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDimension.h"
#include "core/html/HTMLFrameElement.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Frame.h"
#include "core/rendering/RenderFrameSet.h"

namespace WebCore {

using namespace HTMLNames;

HTMLFrameSetElement::HTMLFrameSetElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_border(6)
    , m_borderSet(false)
    , m_borderColorSet(false)
    , m_frameborder(true)
    , m_frameborderSet(false)
    , m_noresize(false)
{
    ASSERT(hasTagName(framesetTag));
    ScriptWrappable::init(this);
    setHasCustomStyleCallbacks();
}

PassRefPtr<HTMLFrameSetElement> HTMLFrameSetElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLFrameSetElement(tagName, document));
}

bool HTMLFrameSetElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == bordercolorAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLFrameSetElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == bordercolorAttr)
        addHTMLColorToStyle(style, CSSPropertyBorderColor, value);
    else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLFrameSetElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == rowsAttr) {
        if (!value.isNull()) {
            m_rowLengths = parseListOfDimensions(value.string());
            setNeedsStyleRecalc();
        }
    } else if (name == colsAttr) {
        if (!value.isNull()) {
            m_colLengths = parseListOfDimensions(value.string());
            setNeedsStyleRecalc();
        }
    } else if (name == frameborderAttr) {
        if (!value.isNull()) {
            if (equalIgnoringCase(value, "no") || equalIgnoringCase(value, "0")) {
                m_frameborder = false;
                m_frameborderSet = true;
            } else if (equalIgnoringCase(value, "yes") || equalIgnoringCase(value, "1")) {
                m_frameborderSet = true;
            }
        } else {
            m_frameborder = false;
            m_frameborderSet = false;
        }
    } else if (name == noresizeAttr) {
        m_noresize = true;
    } else if (name == borderAttr) {
        if (!value.isNull()) {
            m_border = value.toInt();
            m_borderSet = true;
        } else
            m_borderSet = false;
    } else if (name == bordercolorAttr)
        m_borderColorSet = !value.isEmpty();
    else if (name == onloadAttr)
        document().setWindowAttributeEventListener(eventNames().loadEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onbeforeunloadAttr)
        document().setWindowAttributeEventListener(eventNames().beforeunloadEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onunloadAttr)
        document().setWindowAttributeEventListener(eventNames().unloadEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onblurAttr)
        document().setWindowAttributeEventListener(eventNames().blurEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onfocusAttr)
        document().setWindowAttributeEventListener(eventNames().focusEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onfocusinAttr)
        document().setWindowAttributeEventListener(eventNames().focusinEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onfocusoutAttr)
        document().setWindowAttributeEventListener(eventNames().focusoutEvent, createAttributeEventListener(document().frame(), name, value));
#if ENABLE(ORIENTATION_EVENTS)
    else if (name == onorientationchangeAttr)
        document().setWindowAttributeEventListener(eventNames().orientationchangeEvent, createAttributeEventListener(document().frame(), name, value));
#endif
    else if (name == onhashchangeAttr)
        document().setWindowAttributeEventListener(eventNames().hashchangeEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onresizeAttr)
        document().setWindowAttributeEventListener(eventNames().resizeEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onscrollAttr)
        document().setWindowAttributeEventListener(eventNames().scrollEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onstorageAttr)
        document().setWindowAttributeEventListener(eventNames().storageEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == ononlineAttr)
        document().setWindowAttributeEventListener(eventNames().onlineEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onofflineAttr)
        document().setWindowAttributeEventListener(eventNames().offlineEvent, createAttributeEventListener(document().frame(), name, value));
    else if (name == onpopstateAttr)
        document().setWindowAttributeEventListener(eventNames().popstateEvent, createAttributeEventListener(document().frame(), name, value));
    else
        HTMLElement::parseAttribute(name, value);
}

bool HTMLFrameSetElement::rendererIsNeeded(const RenderStyle& style)
{
    // For compatibility, frames render even when display: none is set.
    // However, we delay creating a renderer until stylesheets have loaded.
    return style.isStyleAvailable();
}

RenderObject* HTMLFrameSetElement::createRenderer(RenderStyle *style)
{
    if (style->hasContent())
        return RenderObject::createObject(this, style);

    return new RenderFrameSet(this);
}

void HTMLFrameSetElement::attach(const AttachContext& context)
{
    // Inherit default settings from parent frameset
    // FIXME: This is not dynamic.
    for (ContainerNode* node = parentNode(); node; node = node->parentNode()) {
        if (node->hasTagName(framesetTag)) {
            HTMLFrameSetElement* frameset = toHTMLFrameSetElement(node);
            if (!m_frameborderSet)
                m_frameborder = frameset->hasFrameBorder();
            if (m_frameborder) {
                if (!m_borderSet)
                    m_border = frameset->border();
                if (!m_borderColorSet)
                    m_borderColorSet = frameset->hasBorderColor();
            }
            if (!m_noresize)
                m_noresize = frameset->noResize();
            break;
        }
    }

    HTMLElement::attach(context);
}

void HTMLFrameSetElement::defaultEventHandler(Event* evt)
{
    if (evt->isMouseEvent() && !m_noresize && renderer() && renderer()->isFrameSet()) {
        if (toRenderFrameSet(renderer())->userResize(toMouseEvent(evt))) {
            evt->setDefaultHandled();
            return;
        }
    }
    HTMLElement::defaultEventHandler(evt);
}

Node::InsertionNotificationRequest HTMLFrameSetElement::insertedInto(ContainerNode* insertionPoint)
{
    if (insertionPoint->inDocument() && document().frame()) {
        // A document using <frameset> likely won't literally have a body, but as far as the client is concerned, the frameset is effectively the body.
        document().frame()->loader()->client()->dispatchWillInsertBody();
    }
    return HTMLElement::insertedInto(insertionPoint);
}

void HTMLFrameSetElement::willRecalcStyle(StyleRecalcChange)
{
    if (needsStyleRecalc() && renderer()) {
        renderer()->setNeedsLayout();
        clearNeedsStyleRecalc();
    }
}

DOMWindow* HTMLFrameSetElement::anonymousNamedGetter(const AtomicString& name)
{
    Node* frameNode = children()->namedItem(name);
    if (!frameNode || !frameNode->hasTagName(HTMLNames::frameTag))
        return 0;
    Document* document = toHTMLFrameElement(frameNode)->contentDocument();
    if (!document || !document->frame())
        return 0;
    return document->domWindow();
}

} // namespace WebCore
