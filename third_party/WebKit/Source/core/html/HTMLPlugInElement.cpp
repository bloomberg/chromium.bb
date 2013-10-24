/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "core/html/HTMLPlugInElement.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/npruntime_impl.h"
#include "core/dom/Document.h"
#include "core/dom/PostAttachCallbacks.h"
#include "core/events/Event.h"
#include "core/html/HTMLImageLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/EventHandler.h"
#include "core/frame/Frame.h"
#include "core/platform/MIMETypeFromURL.h"
#include "core/plugins/PluginView.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderWidget.h"
#include "platform/Widget.h"
#include "wtf/UnusedParam.h"


namespace WebCore {

using namespace HTMLNames;

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tagName, Document& doc, bool createdByParser, PreferPlugInsForImagesOption preferPlugInsForImagesOption)
    : HTMLFrameOwnerElement(tagName, doc)
    , m_NPObject(0)
    , m_isCapturingMouseEvents(false)
    , m_inBeforeLoadEventHandler(false)
    // m_needsWidgetUpdate(!createdByParser) allows HTMLObjectElement to delay
    // widget updates until after all children are parsed. For HTMLEmbedElement
    // this delay is unnecessary, but it is simpler to make both classes share
    // the same codepath in this class.
    , m_needsWidgetUpdate(!createdByParser)
    , m_shouldPreferPlugInsForImages(preferPlugInsForImagesOption == ShouldPreferPlugInsForImages)
    , m_displayState(Playing)
{
    setHasCustomStyleCallbacks();
}

HTMLPlugInElement::~HTMLPlugInElement()
{
    ASSERT(!m_pluginWrapper); // cleared in detach()

    if (m_NPObject) {
        _NPN_ReleaseObject(m_NPObject);
        m_NPObject = 0;
    }
}

bool HTMLPlugInElement::canProcessDrag() const
{
    const PluginView* plugin = pluginWidget() && pluginWidget()->isPluginView() ? toPluginView(pluginWidget()) : 0;
    return plugin ? plugin->canProcessDrag() : false;
}

bool HTMLPlugInElement::willRespondToMouseClickEvents()
{
    if (isDisabledFormControl())
        return false;
    RenderObject* r = renderer();
    if (!r)
        return false;
    if (!r->isEmbeddedObject() && !r->isWidget())
        return false;
    return true;
}

void HTMLPlugInElement::removeAllEventListeners()
{
    HTMLFrameOwnerElement::removeAllEventListeners();
    if (RenderWidget* renderer = existingRenderWidget()) {
        if (Widget* widget = renderer->widget())
            widget->eventListenersRemoved();
    }
}

void HTMLPlugInElement::didMoveToNewDocument(Document& oldDocument)
{
    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument();
    HTMLFrameOwnerElement::didMoveToNewDocument(oldDocument);
}

void HTMLPlugInElement::attach(const AttachContext& context)
{
    bool isImage = isImageType();

    if (!isImage)
        PostAttachCallbacks::queueCallback(HTMLPlugInElement::updateWidgetCallback, this);

    HTMLFrameOwnerElement::attach(context);

    if (isImage && renderer() && !useFallbackContent()) {
        if (!m_imageLoader)
            m_imageLoader = adoptPtr(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
    }
}

void HTMLPlugInElement::updateWidgetCallback(Node* n)
{
    toHTMLPlugInElement(n)->updateWidgetIfNecessary();
}

void HTMLPlugInElement::updateWidgetIfNecessary()
{
    document().updateStyleIfNeeded();

    if (!needsWidgetUpdate() || useFallbackContent() || isImageType())
        return;
    if (!renderEmbeddedObject() || renderEmbeddedObject()->showsUnavailablePluginIndicator())
        return;

    updateWidget(CreateOnlyNonNetscapePlugins);
}

void HTMLPlugInElement::detach(const AttachContext& context)
{
    // FIXME: Because of the insanity that is HTMLPlugInImageElement::
    // recalcStyle, we can end up detaching during an attach() call, before we
    // even have a renderer. In that case, don't mark the widget for update.
    if (confusingAndOftenMisusedAttached() && renderer() && !useFallbackContent()) {
        // Update the widget the next time we attach (detaching destroys the
        // plugin).
        setNeedsWidgetUpdate(true);
    }

    resetInstance();

    if (m_isCapturingMouseEvents) {
        if (Frame* frame = document().frame())
            frame->eventHandler().setCapturingMouseEventsNode(0);
        m_isCapturingMouseEvents = false;
    }

    if (m_NPObject) {
        _NPN_ReleaseObject(m_NPObject);
        m_NPObject = 0;
    }

    HTMLFrameOwnerElement::detach(context);
}

RenderObject* HTMLPlugInElement::createRenderer(RenderStyle* style)
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

void HTMLPlugInElement::willRecalcStyle(StyleRecalcChange)
{
    // FIXME: Why is this necessary? Manual re-attach is almost always wrong.
    if (!useFallbackContent() && needsWidgetUpdate() && renderer() && !isImageType())
        reattach();
}

void HTMLPlugInElement::finishParsingChildren()
{
    HTMLFrameOwnerElement::finishParsingChildren();
    if (useFallbackContent())
        return;

    setNeedsWidgetUpdate(true);
    if (inDocument())
        setNeedsStyleRecalc();
}

void HTMLPlugInElement::resetInstance()
{
    m_pluginWrapper.clear();
}

SharedPersistent<v8::Object>* HTMLPlugInElement::pluginWrapper()
{
    Frame* frame = document().frame();
    if (!frame)
        return 0;

    // If the host dynamically turns off JavaScript (or Java) we will still return
    // the cached allocated Bindings::Instance.  Not supporting this edge-case is OK.
    if (!m_pluginWrapper) {
        if (Widget* widget = pluginWidget())
            m_pluginWrapper = frame->script().createPluginWrapper(widget);
    }
    return m_pluginWrapper.get();
}

bool HTMLPlugInElement::dispatchBeforeLoadEvent(const String& sourceURL)
{
    // FIXME: Our current plug-in loading design can't guarantee the following
    // assertion is true, since plug-in loading can be initiated during layout,
    // and synchronous layout can be initiated in a beforeload event handler!
    // See <http://webkit.org/b/71264>.
    // ASSERT(!m_inBeforeLoadEventHandler);
    m_inBeforeLoadEventHandler = true;
    bool beforeLoadAllowedLoad = HTMLFrameOwnerElement::dispatchBeforeLoadEvent(sourceURL);
    m_inBeforeLoadEventHandler = false;
    return beforeLoadAllowedLoad;
}

Widget* HTMLPlugInElement::pluginWidget() const
{
    if (m_inBeforeLoadEventHandler) {
        // The plug-in hasn't loaded yet, and it makes no sense to try to load if beforeload handler happened to touch the plug-in element.
        // That would recursively call beforeload for the same element.
        return 0;
    }

    RenderWidget* renderWidget = renderWidgetForJSBindings();
    if (!renderWidget)
        return 0;

    return renderWidget->widget();
}

bool HTMLPlugInElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr)
        return true;
    return HTMLFrameOwnerElement::isPresentationAttribute(name);
}

void HTMLPlugInElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr)
        applyAlignmentAttributeToStyle(value, style);
    else
        HTMLFrameOwnerElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLPlugInElement::defaultEventHandler(Event* event)
{
    // Firefox seems to use a fake event listener to dispatch events to plug-in (tested with mouse events only).
    // This is observable via different order of events - in Firefox, event listeners specified in HTML attributes fires first, then an event
    // gets dispatched to plug-in, and only then other event listeners fire. Hopefully, this difference does not matter in practice.

    // FIXME: Mouse down and scroll events are passed down to plug-in via custom code in EventHandler; these code paths should be united.

    RenderObject* r = renderer();
    if (r && r->isEmbeddedObject()) {
        if (toRenderEmbeddedObject(r)->showsUnavailablePluginIndicator())
            return;

        if (displayState() < Playing)
            return;
    }

    if (!r || !r->isWidget())
        return;
    RefPtr<Widget> widget = toRenderWidget(r)->widget();
    if (!widget)
        return;
    widget->handleEvent(event);
    if (event->defaultHandled())
        return;
    HTMLFrameOwnerElement::defaultEventHandler(event);
}

RenderWidget* HTMLPlugInElement::renderWidgetForJSBindings() const
{
    document().updateLayoutIgnorePendingStylesheets();
    return existingRenderWidget();
}

bool HTMLPlugInElement::isKeyboardFocusable() const
{
    if (!document().page())
        return false;

    const PluginView* plugin = pluginWidget() && pluginWidget()->isPluginView() ? toPluginView(pluginWidget()) : 0;
    if (plugin)
        return plugin->supportsKeyboardFocus();

    return false;
}

bool HTMLPlugInElement::isPluginElement() const
{
    return true;
}

bool HTMLPlugInElement::rendererIsFocusable() const
{
    if (HTMLFrameOwnerElement::supportsFocus() && HTMLFrameOwnerElement::rendererIsFocusable())
        return true;

    if (useFallbackContent() || !renderer() || !renderer()->isEmbeddedObject())
        return false;
    return !toRenderEmbeddedObject(renderer())->showsUnavailablePluginIndicator();
}

NPObject* HTMLPlugInElement::getNPObject()
{
    ASSERT(document().frame());
    if (!m_NPObject)
        m_NPObject = document().frame()->script().createScriptObjectForPluginElement(this);
    return m_NPObject;
}

bool HTMLPlugInElement::isImageType()
{
    if (m_serviceType.isEmpty() && protocolIs(m_url, "data"))
        m_serviceType = mimeTypeFromDataURL(m_url);

    if (Frame* frame = document().frame()) {
        KURL completedURL = document().completeURL(m_url);
        return frame->loader().client()->objectContentType(completedURL, m_serviceType, shouldPreferPlugInsForImages()) == ObjectContentImage;
    }

    return Image::supportsType(m_serviceType);
}

const String HTMLPlugInElement::loadedMimeType() const
{
    String mimeType = serviceType();
    if (mimeType.isEmpty())
        mimeType = mimeTypeFromURL(m_loadedUrl);
    return mimeType;
}

RenderEmbeddedObject* HTMLPlugInElement::renderEmbeddedObject() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers
    // when using fallback content.
    if (!renderer() || !renderer()->isEmbeddedObject())
        return 0;
    return toRenderEmbeddedObject(renderer());
}

// We don't use m_url, as it may not be the final URL that the object loads,
// depending on <param> values.
bool HTMLPlugInElement::allowedToLoadFrameURL(const String& url)
{
    KURL completeURL = document().completeURL(url);
    if (contentFrame() && protocolIsJavaScript(completeURL)
        && !document().securityOrigin()->canAccess(contentDocument()->securityOrigin()))
        return false;
    return document().frame()->isURLAllowed(completeURL);
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values.
bool HTMLPlugInElement::wouldLoadAsNetscapePlugin(const String& url, const String& serviceType)
{
    ASSERT(document().frame());
    KURL completedURL;
    if (!url.isEmpty())
        completedURL = document().completeURL(url);
    return document().frame()->loader().client()->objectContentType(completedURL, serviceType, shouldPreferPlugInsForImages()) == ObjectContentNetscapePlugin;
}

}
