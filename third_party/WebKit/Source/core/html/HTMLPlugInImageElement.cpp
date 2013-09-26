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

#include "core/dom/PostAttachCallbacks.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/PluginDocument.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/Logging.h"
#include "core/platform/MIMETypeFromURL.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/graphics/Image.h"
#include "core/plugins/PluginData.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderImage.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

using namespace HTMLNames;

typedef Vector<RefPtr<HTMLPlugInImageElement> > HTMLPlugInImageElementList;

static const int sizingTinyDimensionThreshold = 40;
static const int sizingSmallWidthThreshold = 250;
static const int sizingMediumWidthThreshold = 450;
static const int sizingMediumHeightThreshold = 300;
static const float sizingFullPageAreaRatioThreshold = 0.96;
static const float autostartSoonAfterUserGestureThreshold = 5.0;

HTMLPlugInImageElement::HTMLPlugInImageElement(const QualifiedName& tagName, Document& document, bool createdByParser, PreferPlugInsForImagesOption preferPlugInsForImagesOption)
    : HTMLPlugInElement(tagName, document)
    // m_needsWidgetUpdate(!createdByParser) allows HTMLObjectElement to delay
    // widget updates until after all children are parsed.  For HTMLEmbedElement
    // this delay is unnecessary, but it is simpler to make both classes share
    // the same codepath in this class.
    , m_needsWidgetUpdate(!createdByParser)
    , m_shouldPreferPlugInsForImages(preferPlugInsForImagesOption == ShouldPreferPlugInsForImages)
    , m_createdDuringUserGesture(UserGestureIndicator::processingUserGesture())
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

    if (Frame* frame = document().frame()) {
        KURL completedURL = document().completeURL(m_url);
        return frame->loader()->client()->objectContentType(completedURL, m_serviceType, shouldPreferPlugInsForImages()) == ObjectContentImage;
    }

    return Image::supportsType(m_serviceType);
}

// We don't use m_url, as it may not be the final URL that the object loads,
// depending on <param> values.
bool HTMLPlugInImageElement::allowedToLoadFrameURL(const String& url)
{
    KURL completeURL = document().completeURL(url);

    if (contentFrame() && protocolIsJavaScript(completeURL)
        && !document().securityOrigin()->canAccess(contentDocument()->securityOrigin()))
        return false;

    return document().frame()->isURLAllowed(completeURL);
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values.
bool HTMLPlugInImageElement::wouldLoadAsNetscapePlugin(const String& url, const String& serviceType)
{
    ASSERT(document().frame());
    KURL completedURL;
    if (!url.isEmpty())
        completedURL = document().completeURL(url);

    FrameLoader* frameLoader = document().frame()->loader();
    ASSERT(frameLoader);
    if (frameLoader->client()->objectContentType(completedURL, serviceType, shouldPreferPlugInsForImages()) == ObjectContentNetscapePlugin)
        return true;
    return false;
}

RenderObject* HTMLPlugInImageElement::createRenderer(RenderStyle* style)
{
    // Fallback content breaks the DOM->Renderer class relationship of this
    // class and all superclasses because createObject won't necessarily
    // return a RenderEmbeddedObject, RenderPart or even RenderWidget.
    if (useFallbackContent())
        return RenderObject::createObject(this, style);

    if (isImageType()) {
        RenderImage* image = new RenderImage(this);
        image->setImageResource(RenderImageResource::create());
        return image;
    }

    return new RenderEmbeddedObject(this);
}

void HTMLPlugInImageElement::willRecalcStyle(StyleRecalcChange)
{
    // FIXME: Why is this necessary?  Manual re-attach is almost always wrong.
    if (!useFallbackContent() && needsWidgetUpdate() && renderer() && !isImageType())
        reattach();
}

void HTMLPlugInImageElement::attach(const AttachContext& context)
{
    bool isImage = isImageType();

    if (!isImage)
        PostAttachCallbacks::queueCallback(HTMLPlugInImageElement::updateWidgetCallback, this);

    HTMLPlugInElement::attach(context);

    if (isImage && renderer() && !useFallbackContent()) {
        if (!m_imageLoader)
            m_imageLoader = adoptPtr(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
    }
}

void HTMLPlugInImageElement::detach(const AttachContext& context)
{
    // FIXME: Because of the insanity that is HTMLPlugInImageElement::recalcStyle,
    // we can end up detaching during an attach() call, before we even have a
    // renderer.  In that case, don't mark the widget for update.
    if (confusingAndOftenMisusedAttached() && renderer() && !useFallbackContent())
        // Update the widget the next time we attach (detaching destroys the plugin).
        setNeedsWidgetUpdate(true);
    HTMLPlugInElement::detach(context);
}

void HTMLPlugInImageElement::updateWidgetIfNecessary()
{
    document().updateStyleIfNeeded();

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

void HTMLPlugInImageElement::updateWidgetCallback(Node* n)
{
    toHTMLPlugInImageElement(n)->updateWidgetIfNecessary();
}

bool HTMLPlugInImageElement::requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    if (url.isEmpty() && mimeType.isEmpty())
        return false;

    // FIXME: None of this code should use renderers!
    RenderEmbeddedObject* renderer = renderEmbeddedObject();
    ASSERT(renderer);
    if (!renderer)
        return false;

    KURL completedURL = document().completeURL(url);

    bool useFallback;
    if (shouldUsePlugin(completedURL, mimeType, renderer->hasFallbackContent(), useFallback)) {
        bool success = loadPlugin(completedURL, mimeType, paramNames, paramValues, useFallback);
        return success;
    }

    // If the plug-in element already contains a subframe, loadOrRedirectSubframe will re-use it. Otherwise,
    // it will create a new frame and set it as the RenderPart's widget, causing what was previously
    // in the widget to be torn down.
    return loadOrRedirectSubframe(completedURL, getNameAttribute(), true);
}

bool HTMLPlugInImageElement::loadPlugin(const KURL& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback)
{
    Frame* frame = document().frame();

    if (!frame->loader()->allowPlugins(AboutToInstantiatePlugin))
        return false;

    if (!pluginIsLoadable(url, mimeType))
        return false;

    RenderEmbeddedObject* renderer = renderEmbeddedObject();

    // FIXME: This code should not depend on renderer!
    if (!renderer || useFallback)
        return false;

    LOG(Plugins, "%p Plug-in URL: %s", this, m_url.utf8().data());
    LOG(Plugins, "   Loaded URL: %s", url.string().utf8().data());
    m_loadedUrl = url;

    IntSize contentSize = roundedIntSize(LayoutSize(renderer->contentWidth(), renderer->contentHeight()));
    bool loadManually = document().isPluginDocument() && !frame->loader()->containsPlugins() && toPluginDocument(document()).shouldLoadPluginManually();
    RefPtr<Widget> widget = frame->loader()->client()->createPlugin(contentSize, this, url, paramNames, paramValues, mimeType, loadManually);

    if (!widget) {
        if (!renderer->showsUnavailablePluginIndicator())
            renderer->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginMissing);
        return false;
    }

    renderer->setWidget(widget);
    frame->loader()->setContainsPlugins();
    setNeedsStyleRecalc(LocalStyleChange, StyleChangeFromRenderer);
    return true;
}

bool HTMLPlugInImageElement::shouldUsePlugin(const KURL& url, const String& mimeType, bool hasFallback, bool& useFallback)
{
    // Allow other plug-ins to win over QuickTime because if the user has installed a plug-in that
    // can handle TIFF (which QuickTime can also handle) they probably intended to override QT.
    if (document().frame()->page() && (mimeType == "image/tiff" || mimeType == "image/tif" || mimeType == "image/x-tiff")) {
        const PluginData* pluginData = document().frame()->page()->pluginData();
        String pluginName = pluginData ? pluginData->pluginNameForMimeType(mimeType) : String();
        if (!pluginName.isEmpty() && !pluginName.contains("QuickTime", false))
            return true;
    }

    ObjectContentType objectType = document().frame()->loader()->client()->objectContentType(url, mimeType, shouldPreferPlugInsForImages());
    // If an object's content can't be handled and it has no fallback, let
    // it be handled as a plugin to show the broken plugin icon.
    useFallback = objectType == ObjectContentNone && hasFallback;
    return objectType == ObjectContentNone || objectType == ObjectContentNetscapePlugin || objectType == ObjectContentOtherPlugin;

}

bool HTMLPlugInImageElement::pluginIsLoadable(const KURL& url, const String& mimeType)
{
    Frame* frame = document().frame();
    Settings* settings = frame->settings();
    if (!settings)
        return false;

    if (MIMETypeRegistry::isJavaAppletMIMEType(mimeType)) {
        if (!settings->isJavaEnabled())
            return false;
    }

    if (document().isSandboxed(SandboxPlugins))
        return false;

    if (!document().securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(frame, url.string());
        return false;
    }

    String declaredMimeType = document().isPluginDocument() && document().ownerElement() ?
        document().ownerElement()->fastGetAttribute(HTMLNames::typeAttr) :
        fastGetAttribute(HTMLNames::typeAttr);
    if (!document().contentSecurityPolicy()->allowObjectFromSource(url)
        || !document().contentSecurityPolicy()->allowPluginType(mimeType, declaredMimeType, url)) {
        renderEmbeddedObject()->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginBlockedByContentSecurityPolicy);
        return false;
    }

    if (frame->loader() && !frame->loader()->mixedContentChecker()->canRunInsecureContent(document().securityOrigin(), url))
        return false;
    return true;
}

} // namespace WebCore
