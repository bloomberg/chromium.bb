/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
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

#ifndef HTMLPlugInElement_h
#define HTMLPlugInElement_h

#include "bindings/core/v8/SharedPersistent.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include <v8.h>

struct NPObject;

namespace blink {

class HTMLImageLoader;
class PluginPlaceholder;
class RenderEmbeddedObject;
class RenderPart;
class Widget;

enum PreferPlugInsForImagesOption {
    ShouldPreferPlugInsForImages,
    ShouldNotPreferPlugInsForImages
};

class HTMLPlugInElement : public HTMLFrameOwnerElement {
public:
    virtual ~HTMLPlugInElement();
    virtual void trace(Visitor*) override;

    void resetInstance();
    SharedPersistent<v8::Object>* pluginWrapper();
    Widget* pluginWidget() const;
    NPObject* getNPObject();
    bool canProcessDrag() const;
    const String& url() const { return m_url; }

    // Public for FrameView::addPartToUpdate()
    bool needsWidgetUpdate() const { return m_needsWidgetUpdate; }
    void setNeedsWidgetUpdate(bool needsWidgetUpdate) { m_needsWidgetUpdate = needsWidgetUpdate; }
    void updateWidget();

    bool shouldAccelerate() const;

    void requestPluginCreationWithoutRendererIfPossible();
    void createPluginWithoutRenderer();

    // Public for Internals::forcePluginPlaceholder.
    bool usePlaceholderContent() const { return m_placeholder; }
    void setPlaceholder(PassOwnPtrWillBeRawPtr<PluginPlaceholder>);

protected:
    HTMLPlugInElement(const QualifiedName& tagName, Document&, bool createdByParser, PreferPlugInsForImagesOption);

    // Node functions:
    virtual void didMoveToNewDocument(Document& oldDocument) override;

    // Element functions:
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) override;

    virtual bool hasFallbackContent() const;
    virtual bool useFallbackContent() const;
    // Create or update the RenderPart and return it, triggering layout if
    // necessary.
    virtual RenderPart* renderPartForJSBindings() const;

    bool isImageType();
    bool shouldPreferPlugInsForImages() const { return m_shouldPreferPlugInsForImages; }
    RenderEmbeddedObject* renderEmbeddedObject() const;
    bool allowedToLoadFrameURL(const String& url);
    bool requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues);
    bool shouldUsePlugin(const KURL&, const String& mimeType, bool hasFallback, bool& useFallback);

    void dispatchErrorEvent();
    void lazyReattachIfNeeded();

    String m_serviceType;
    String m_url;
    KURL m_loadedUrl;
    OwnPtrWillBeMember<HTMLImageLoader> m_imageLoader;
    bool m_isDelayingLoadEvent;

private:
    // EventTarget functions:
    virtual void removeAllEventListeners() override final;

    // Node functions:
    virtual bool canContainRangeEndPoint() const override { return false; }
    virtual bool willRespondToMouseClickEvents() override final;
    virtual void defaultEventHandler(Event*) override final;
    virtual void attach(const AttachContext& = AttachContext()) override final;
    virtual void detach(const AttachContext& = AttachContext()) override final;
    virtual void finishParsingChildren() override final;

    // Element functions:
    virtual RenderObject* createRenderer(RenderStyle*) override;
    virtual bool supportsFocus() const override final { return true; }
    virtual bool rendererIsFocusable() const override final;
    virtual bool isKeyboardFocusable() const override final;
    virtual void didAddUserAgentShadowRoot(ShadowRoot&) override final;
    virtual void willAddFirstAuthorShadowRoot() override final;

    // HTMLElement function:
    virtual bool hasCustomFocusLogic() const override;
    virtual bool isPluginElement() const override final;

    // Return any existing RenderPart without triggering relayout, or 0 if it
    // doesn't yet exist.
    virtual RenderPart* existingRenderPart() const = 0;
    virtual void updateWidgetInternal() = 0;

    bool loadPlugin(const KURL&, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback, bool requireRenderer);
    bool pluginIsLoadable(const KURL&, const String& mimeType);
    bool wouldLoadAsNetscapePlugin(const String& url, const String& serviceType);

    mutable RefPtr<SharedPersistent<v8::Object> > m_pluginWrapper;
    NPObject* m_NPObject;
    bool m_isCapturingMouseEvents;
    bool m_needsWidgetUpdate;
    bool m_shouldPreferPlugInsForImages;

    OwnPtrWillBeMember<PluginPlaceholder> m_placeholder;

    // Normally the Widget is stored in HTMLFrameOwnerElement::m_widget.
    // However, plugins can persist even when not rendered. In order to
    // prevent confusing code which may assume that widget() != null
    // means the frame is active, we save off m_widget here while
    // the plugin is persisting but not being displayed.
    RefPtr<Widget> m_persistedPluginWidget;
};

inline bool isHTMLPlugInElement(const HTMLElement& element)
{
    return element.isPluginElement();
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLPlugInElement);

} // namespace blink

#endif // HTMLPlugInElement_h
