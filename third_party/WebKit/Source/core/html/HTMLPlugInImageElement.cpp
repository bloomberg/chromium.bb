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

#include "core/html/PluginDocument.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/frame/ContentSecurityPolicy.h"
#include "core/frame/Frame.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "platform/Logging.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/plugins/PluginData.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "platform/UserGestureIndicator.h"
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
    : HTMLPlugInElement(tagName, document, createdByParser, preferPlugInsForImagesOption)
    , m_createdDuringUserGesture(UserGestureIndicator::processingUserGesture())
{
}

HTMLPlugInImageElement::~HTMLPlugInImageElement()
{
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

    if (!frame->loader().allowPlugins(AboutToInstantiatePlugin))
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
    bool loadManually = document().isPluginDocument() && !frame->loader().containsPlugins() && toPluginDocument(document()).shouldLoadPluginManually();
    RefPtr<Widget> widget = frame->loader().client()->createPlugin(contentSize, this, url, paramNames, paramValues, mimeType, loadManually);

    if (!widget) {
        if (!renderer->showsUnavailablePluginIndicator())
            renderer->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginMissing);
        return false;
    }

    renderer->setWidget(widget);
    frame->loader().setContainsPlugins();
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

    ObjectContentType objectType = document().frame()->loader().client()->objectContentType(url, mimeType, shouldPreferPlugInsForImages());
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

    if (!frame->loader().mixedContentChecker()->canRunInsecureContent(document().securityOrigin(), url))
        return false;
    return true;
}

} // namespace WebCore
