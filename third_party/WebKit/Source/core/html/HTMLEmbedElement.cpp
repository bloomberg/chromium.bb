/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#include "core/html/HTMLEmbedElement.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/PluginDocument.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderWidget.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLEmbedElement::HTMLEmbedElement(Document& document, bool createdByParser)
    : HTMLPlugInElement(embedTag, document, createdByParser, ShouldPreferPlugInsForImages)
{
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLEmbedElement> HTMLEmbedElement::create(Document& document, bool createdByParser)
{
    RefPtr<HTMLEmbedElement> element = adoptRef(new HTMLEmbedElement(document, createdByParser));
    element->ensureUserAgentShadowRoot();
    return element.release();
}

static inline RenderWidget* findWidgetRenderer(const Node* n)
{
    if (!n->renderer())
        do
            n = n->parentNode();
        while (n && !n->hasTagName(objectTag));

    if (n && n->renderer() && n->renderer()->isWidget())
        return toRenderWidget(n->renderer());

    return 0;
}

RenderWidget* HTMLEmbedElement::existingRenderWidget() const
{
    return findWidgetRenderer(this);
}

bool HTMLEmbedElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == hiddenAttr)
        return true;
    return HTMLPlugInElement::isPresentationAttribute(name);
}

void HTMLEmbedElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == hiddenAttr) {
        if (equalIgnoringCase(value, "yes") || equalIgnoringCase(value, "true")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWidth, 0, CSSPrimitiveValue::CSS_PX);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyHeight, 0, CSSPrimitiveValue::CSS_PX);
        }
    } else {
        HTMLPlugInElement::collectStyleForPresentationAttribute(name, value, style);
    }
}

void HTMLEmbedElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == typeAttr) {
        m_serviceType = value.string().lower();
        size_t pos = m_serviceType.find(";");
        if (pos != kNotFound)
            m_serviceType = m_serviceType.left(pos);
    } else if (name == codeAttr) {
        m_url = stripLeadingAndTrailingHTMLSpaces(value);
    } else if (name == srcAttr) {
        m_url = stripLeadingAndTrailingHTMLSpaces(value);
        if (renderer() && isImageType()) {
            if (!m_imageLoader)
                m_imageLoader = adoptPtr(new HTMLImageLoader(this));
            m_imageLoader->updateFromElementIgnoringPreviousError();
        }
    } else {
        HTMLPlugInElement::parseAttribute(name, value);
    }
}

void HTMLEmbedElement::parametersForPlugin(Vector<String>& paramNames, Vector<String>& paramValues)
{
    if (!hasAttributes())
        return;

    unsigned attributeCount = this->attributeCount();
    for (unsigned i = 0; i < attributeCount; ++i) {
        const Attribute* attribute = attributeItem(i);
        paramNames.append(attribute->localName().string());
        paramValues.append(attribute->value().string());
    }
}

// FIXME: This should be unified with HTMLObjectElement::updateWidget and
// moved down into HTMLPluginElement.cpp
void HTMLEmbedElement::updateWidgetInternal()
{
    ASSERT(!renderEmbeddedObject()->showsUnavailablePluginIndicator());
    ASSERT(needsWidgetUpdate());
    setNeedsWidgetUpdate(false);

    if (m_url.isEmpty() && m_serviceType.isEmpty())
        return;

    // Note these pass m_url and m_serviceType to allow better code sharing with
    // <object> which modifies url and serviceType before calling these.
    if (!allowedToLoadFrameURL(m_url))
        return;

    // FIXME: These should be joined into a PluginParameters class.
    Vector<String> paramNames;
    Vector<String> paramValues;
    parametersForPlugin(paramNames, paramValues);

    RefPtr<HTMLEmbedElement> protect(this); // Loading the plugin might remove us from the document.
    bool beforeLoadAllowedLoad = dispatchBeforeLoadEvent(m_url);
    if (!beforeLoadAllowedLoad) {
        if (document().isPluginDocument()) {
            // Plugins inside plugin documents load differently than other plugins. By the time
            // we are here in a plugin document, the load of the plugin (which is the plugin document's
            // main resource) has already started. We need to explicitly cancel the main resource load here.
            toPluginDocument(document()).cancelManualPluginLoad();
        }
        return;
    }
    if (!renderer()) // Do not load the plugin if beforeload removed this element or its renderer.
        return;

    // FIXME: beforeLoad could have detached the renderer!  Just like in the <object> case above.
    requestObject(m_url, m_serviceType, paramNames, paramValues);
}

bool HTMLEmbedElement::rendererIsNeeded(const RenderStyle& style)
{
    if (isImageType())
        return HTMLPlugInElement::rendererIsNeeded(style);

    Frame* frame = document().frame();
    if (!frame)
        return false;

    // If my parent is an <object> and is not set to use fallback content, I
    // should be ignored and not get a renderer.
    ContainerNode* p = parentNode();
    if (p && p->hasTagName(objectTag)) {
        ASSERT(p->renderer());
        if (!toHTMLObjectElement(p)->useFallbackContent()) {
            ASSERT(!p->renderer()->isEmbeddedObject());
            return false;
        }
    }
    return HTMLPlugInElement::rendererIsNeeded(style);
}

bool HTMLEmbedElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLPlugInElement::isURLAttribute(attribute);
}

const AtomicString HTMLEmbedElement::imageSourceURL() const
{
    return getAttribute(srcAttr);
}

bool HTMLEmbedElement::isInteractiveContent() const
{
    return true;
}

bool HTMLEmbedElement::isExposed() const
{
    // http://www.whatwg.org/specs/web-apps/current-work/#exposed
    for (Node* ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        if (ancestor->hasTagName(objectTag) && toHTMLObjectElement(ancestor)->isExposed())
            return false;
    }
    return true;
}

}
