/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2012 Apple Inc. All rights reserved.
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
#include "core/html/HTMLAppletElement.h"

#include "HTMLNames.h"
#include "core/html/HTMLParamElement.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/Frame.h"
#include "core/page/Settings.h"
#include "core/platform/Widget.h"
#include "core/rendering/RenderApplet.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

using namespace HTMLNames;

HTMLAppletElement::HTMLAppletElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLPlugInImageElement(tagName, document, createdByParser, ShouldNotPreferPlugInsForImages)
{
    ASSERT(hasTagName(appletTag));
    ScriptWrappable::init(this);

    m_serviceType = "application/x-java-applet";
}

PassRefPtr<HTMLAppletElement> HTMLAppletElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    return adoptRef(new HTMLAppletElement(tagName, document, createdByParser));
}

void HTMLAppletElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == altAttr
        || name == archiveAttr
        || name == codeAttr
        || name == codebaseAttr
        || name == mayscriptAttr
        || name == objectAttr) {
        // Do nothing.
        return;
    }

    HTMLPlugInImageElement::parseAttribute(name, value);
}

bool HTMLAppletElement::rendererIsNeeded(const RenderStyle& style)
{
    if (!fastHasAttribute(codeAttr))
        return false;
    return HTMLPlugInImageElement::rendererIsNeeded(style);
}

RenderObject* HTMLAppletElement::createRenderer(RenderStyle* style)
{
    if (!canEmbedJava())
        return RenderObject::createObject(this, style);

    return new RenderApplet(this);
}

RenderWidget* HTMLAppletElement::renderWidgetForJSBindings() const
{
    if (!canEmbedJava())
        return 0;
    return HTMLPlugInImageElement::renderWidgetForJSBindings();
}

RenderWidget* HTMLAppletElement::existingRenderWidget() const
{
    return renderPart();
}

void HTMLAppletElement::updateWidget(PluginCreationOption)
{
    setNeedsWidgetUpdate(false);
    // FIXME: This should ASSERT isFinishedParsingChildren() instead.
    if (!isFinishedParsingChildren())
        return;

    RenderEmbeddedObject* renderer = renderEmbeddedObject();

    Frame* frame = document().frame();
    ASSERT(frame);

    LayoutUnit contentWidth = renderer->style()->width().isFixed() ? LayoutUnit(renderer->style()->width().value()) :
        renderer->width() - renderer->borderAndPaddingWidth();
    LayoutUnit contentHeight = renderer->style()->height().isFixed() ? LayoutUnit(renderer->style()->height().value()) :
        renderer->height() - renderer->borderAndPaddingHeight();

    Vector<String> paramNames;
    Vector<String> paramValues;

    paramNames.append("code");
    paramValues.append(getAttribute(codeAttr).string());

    const AtomicString& codeBase = getAttribute(codebaseAttr);
    if (!codeBase.isNull()) {
        KURL codeBaseURL = document().completeURL(codeBase);
        if (!document().securityOrigin()->canDisplay(codeBaseURL)) {
            FrameLoader::reportLocalLoadFailed(frame, codeBaseURL.string());
            return;
        }
        const char javaAppletMimeType[] = "application/x-java-applet";
        if (!document().contentSecurityPolicy()->allowObjectFromSource(codeBaseURL)
            || !document().contentSecurityPolicy()->allowPluginType(javaAppletMimeType, javaAppletMimeType, codeBaseURL))
            return;
        paramNames.append("codeBase");
        paramValues.append(codeBase.string());
    }

    const AtomicString& name = document().isHTMLDocument() ? getNameAttribute() : getIdAttribute();
    if (!name.isNull()) {
        paramNames.append("name");
        paramValues.append(name.string());
    }

    const AtomicString& archive = getAttribute(archiveAttr);
    if (!archive.isNull()) {
        paramNames.append("archive");
        paramValues.append(archive.string());
    }

    paramNames.append("baseURL");
    KURL baseURL = document().baseURL();
    paramValues.append(baseURL.string());

    const AtomicString& mayScript = getAttribute(mayscriptAttr);
    if (!mayScript.isNull()) {
        paramNames.append("mayScript");
        paramValues.append(mayScript.string());
    }

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->hasTagName(paramTag))
            continue;

        HTMLParamElement* param = toHTMLParamElement(child);
        if (param->name().isEmpty())
            continue;

        paramNames.append(param->name());
        paramValues.append(param->value());
    }

    RefPtr<Widget> widget;
    if (frame->loader()->allowPlugins(AboutToInstantiatePlugin))
        widget = frame->loader()->client()->createJavaAppletWidget(roundedIntSize(LayoutSize(contentWidth, contentHeight)), this, baseURL, paramNames, paramValues);

    if (!widget) {
        if (!renderer->showsUnavailablePluginIndicator())
            renderer->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginMissing);
        return;
    }
    frame->loader()->setContainsPlugins();
    renderer->setWidget(widget);
}

bool HTMLAppletElement::canEmbedJava() const
{
    if (document().isSandboxed(SandboxPlugins))
        return false;

    Settings* settings = document().settings();
    if (!settings)
        return false;

    if (!settings->isJavaEnabled())
        return false;

    return true;
}

}
