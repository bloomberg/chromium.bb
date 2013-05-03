/*
 * Copyright (C) 2008, 2011, 2012 Apple Inc. All rights reserved.
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
#include "core/html/HTMLPlugInImageElement.h"

#include "bindings/v8/ScriptController.h"
#include "core/css/StyleResolver.h"
#include "core/dom/MouseEvent.h"
#include "core/dom/NodeList.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeRenderingContext.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/Text.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/PlugInClient.h"
#include "core/page/SecurityOrigin.h"
#include "core/page/Settings.h"
#include "core/platform/LocalizedStrings.h"
#include "core/platform/Logging.h"
#include "core/platform/SchemeRegistry.h"
#include "core/platform/graphics/Image.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderImage.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

using namespace HTMLNames;

typedef Vector<RefPtr<HTMLPlugInImageElement> > HTMLPlugInImageElementList;

static const int sizingTinyDimensionThreshold = 40;
static const int sizingSmallWidthThreshold = 250;
static const int sizingMediumWidthThreshold = 450;
static const int sizingMediumHeightThreshold = 300;
static const float sizingFullPageAreaRatioThreshold = 0.96;
static const float autostartSoonAfterUserGestureThreshold = 5.0;

HTMLPlugInImageElement::HTMLPlugInImageElement(const QualifiedName& tagName, Document* document, bool createdByParser, PreferPlugInsForImagesOption preferPlugInsForImagesOption)
    : HTMLPlugInElement(tagName, document)
    // m_needsWidgetUpdate(!createdByParser) allows HTMLObjectElement to delay
    // widget updates until after all children are parsed.  For HTMLEmbedElement
    // this delay is unnecessary, but it is simpler to make both classes share
    // the same codepath in this class.
    , m_needsWidgetUpdate(!createdByParser)
    , m_shouldPreferPlugInsForImages(preferPlugInsForImagesOption == ShouldPreferPlugInsForImages)
    , m_createdDuringUserGesture(ScriptController::processingUserGesture())
{
    setHasCustomStyleCallbacks();
}

HTMLPlugInImageElement::~HTMLPlugInImageElement()
{
}

void HTMLPlugInImageElement::setDisplayState(DisplayState state)
{
    HTMLPlugInElement::setDisplayState(state);
}

RenderEmbeddedObject* HTMLPlugInImageElement::renderEmbeddedObject() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers
    // when using fallback content.
    if (!renderer() || !renderer()->isEmbeddedObject())
        return 0;
    return toRenderEmbeddedObject(renderer());
}

bool HTMLPlugInImageElement::isImageType()
{
    if (m_serviceType.isEmpty() && protocolIs(m_url, "data"))
        m_serviceType = mimeTypeFromDataURL(m_url);

    if (Frame* frame = document()->frame()) {
        KURL completedURL = document()->completeURL(m_url);
        return frame->loader()->client()->objectContentType(completedURL, m_serviceType, shouldPreferPlugInsForImages()) == ObjectContentImage;
    }

    return Image::supportsType(m_serviceType);
}

// We don't use m_url, as it may not be the final URL that the object loads,
// depending on <param> values. 
bool HTMLPlugInImageElement::allowedToLoadFrameURL(const String& url)
{
    KURL completeURL = document()->completeURL(url);

    if (contentFrame() && protocolIsJavaScript(completeURL)
        && !document()->securityOrigin()->canAccess(contentDocument()->securityOrigin()))
        return false;

    return document()->frame()->isURLAllowed(completeURL);
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values. 
bool HTMLPlugInImageElement::wouldLoadAsNetscapePlugin(const String& url, const String& serviceType)
{
    ASSERT(document());
    ASSERT(document()->frame());
    KURL completedURL;
    if (!url.isEmpty())
        completedURL = document()->completeURL(url);

    FrameLoader* frameLoader = document()->frame()->loader();
    ASSERT(frameLoader);
    if (frameLoader->client()->objectContentType(completedURL, serviceType, shouldPreferPlugInsForImages()) == ObjectContentNetscapePlugin)
        return true;
    return false;
}

RenderObject* HTMLPlugInImageElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    // Fallback content breaks the DOM->Renderer class relationship of this
    // class and all superclasses because createObject won't necessarily
    // return a RenderEmbeddedObject, RenderPart or even RenderWidget.
    if (useFallbackContent())
        return RenderObject::createObject(this, style);

    if (isImageType()) {
        RenderImage* image = new (arena) RenderImage(this);
        image->setImageResource(RenderImageResource::create());
        return image;
    }

    return new (arena) RenderEmbeddedObject(this);
}

void HTMLPlugInImageElement::willRecalcStyle(StyleChange)
{
    // FIXME: Why is this necessary?  Manual re-attach is almost always wrong.
    if (!useFallbackContent() && needsWidgetUpdate() && renderer() && !isImageType())
        reattach();
}

void HTMLPlugInImageElement::attach()
{
    PostAttachCallbackDisabler disabler(this);

    bool isImage = isImageType();
    
    if (!isImage)
        queuePostAttachCallback(&HTMLPlugInImageElement::updateWidgetCallback, this);

    HTMLPlugInElement::attach();

    if (isImage && renderer() && !useFallbackContent()) {
        if (!m_imageLoader)
            m_imageLoader = adoptPtr(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
    }
}

void HTMLPlugInImageElement::detach()
{
    // FIXME: Because of the insanity that is HTMLPlugInImageElement::recalcStyle,
    // we can end up detaching during an attach() call, before we even have a
    // renderer.  In that case, don't mark the widget for update.
    if (attached() && renderer() && !useFallbackContent())
        // Update the widget the next time we attach (detaching destroys the plugin).
        setNeedsWidgetUpdate(true);
    HTMLPlugInElement::detach();
}

void HTMLPlugInImageElement::updateWidgetIfNecessary()
{
    document()->updateStyleIfNeeded();

    if (!needsWidgetUpdate() || useFallbackContent() || isImageType())
        return;

    if (!renderEmbeddedObject() || renderEmbeddedObject()->showsUnavailablePluginIndicator())
        return;

    updateWidget(CreateOnlyNonNetscapePlugins);
}

void HTMLPlugInImageElement::finishParsingChildren()
{
    HTMLPlugInElement::finishParsingChildren();
    if (useFallbackContent())
        return;
    
    setNeedsWidgetUpdate(true);
    if (inDocument())
        setNeedsStyleRecalc();    
}

void HTMLPlugInImageElement::didMoveToNewDocument(Document* oldDocument)
{
    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument();
    HTMLPlugInElement::didMoveToNewDocument(oldDocument);
}

void HTMLPlugInImageElement::updateWidgetCallback(Node* n, unsigned)
{
    static_cast<HTMLPlugInImageElement*>(n)->updateWidgetIfNecessary();
}

void HTMLPlugInImageElement::subframeLoaderWillCreatePlugIn(const KURL& url)
{
    LOG(Plugins, "%p Plug-in URL: %s", this, m_url.utf8().data());
    LOG(Plugins, "   Loaded URL: %s", url.string().utf8().data());

    m_loadedUrl = url;
}

} // namespace WebCore
