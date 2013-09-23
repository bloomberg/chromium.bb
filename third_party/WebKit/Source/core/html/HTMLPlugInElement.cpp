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
#include "core/events/Event.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/platform/Widget.h"
#include "core/plugins/PluginView.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderWidget.h"
#include "wtf/UnusedParam.h"


namespace WebCore {

using namespace HTMLNames;

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tagName, Document& doc)
    : HTMLFrameOwnerElement(tagName, doc)
    , m_NPObject(0)
    , m_isCapturingMouseEvents(false)
    , m_inBeforeLoadEventHandler(false)
    , m_displayState(Playing)
{
}

HTMLPlugInElement::~HTMLPlugInElement()
{
    ASSERT(!m_instance); // cleared in detach()

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

void HTMLPlugInElement::detach(const AttachContext& context)
{
    m_instance.clear();

    if (m_isCapturingMouseEvents) {
        if (Frame* frame = document().frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);
        m_isCapturingMouseEvents = false;
    }

    if (m_NPObject) {
        _NPN_ReleaseObject(m_NPObject);
        m_NPObject = 0;
    }

    HTMLFrameOwnerElement::detach(context);
}

void HTMLPlugInElement::resetInstance()
{
    m_instance.clear();
}

PassScriptInstance HTMLPlugInElement::getInstance()
{
    Frame* frame = document().frame();
    if (!frame)
        return 0;

    // If the host dynamically turns off JavaScript (or Java) we will still return
    // the cached allocated Bindings::Instance.  Not supporting this edge-case is OK.
    if (m_instance)
        return m_instance;

    if (Widget* widget = pluginWidget())
        m_instance = frame->script()->createScriptInstanceForWidget(widget);

    return m_instance;
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
        m_NPObject = document().frame()->script()->createScriptObjectForPluginElement(this);
    return m_NPObject;
}

}
