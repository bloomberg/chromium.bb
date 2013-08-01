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

#include "core/html/HTMLFrameOwnerElement.h"

#include "bindings/v8/ScriptInstance.h"

struct NPObject;

namespace WebCore {

class RenderEmbeddedObject;
class RenderWidget;
class Widget;

class HTMLPlugInElement : public HTMLFrameOwnerElement {
public:
    virtual ~HTMLPlugInElement();

    void resetInstance();

    PassScriptInstance getInstance();

    Widget* pluginWidget() const;

    enum DisplayState {
        Restarting,
        RestartingWithPendingMouseClick,
        Playing
    };
    DisplayState displayState() const { return m_displayState; }
    virtual void setDisplayState(DisplayState state) { m_displayState = state; }

    NPObject* getNPObject();

    bool isCapturingMouseEvents() const { return m_isCapturingMouseEvents; }
    void setIsCapturingMouseEvents(bool capturing) { m_isCapturingMouseEvents = capturing; }

    bool canContainRangeEndPoint() const { return false; }

    bool canProcessDrag() const;

    virtual bool willRespondToMouseClickEvents() OVERRIDE;

    virtual bool isPlugInImageElement() const { return false; }

protected:
    HTMLPlugInElement(const QualifiedName& tagName, Document*);

    virtual void detach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;

    virtual bool useFallbackContent() const { return false; }

    virtual bool dispatchBeforeLoadEvent(const String& sourceURL) OVERRIDE;

private:
    virtual bool areAuthorShadowsAllowed() const OVERRIDE { return false; }

    virtual void defaultEventHandler(Event*);

    virtual RenderWidget* renderWidgetForJSBindings() const = 0;

    virtual bool supportsFocus() const OVERRIDE { return true; };
    virtual bool rendererIsFocusable() const OVERRIDE;

    virtual bool isKeyboardFocusable() const OVERRIDE;
    virtual bool isPluginElement() const;

    mutable ScriptInstance m_instance;
    NPObject* m_NPObject;
    bool m_isCapturingMouseEvents;
    bool m_inBeforeLoadEventHandler;
    DisplayState m_displayState;
};

inline HTMLPlugInElement* toHTMLPlugInElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isPluginElement());
    return static_cast<HTMLPlugInElement*>(node);
}

inline const HTMLPlugInElement* toHTMLPlugInElement(const Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isPluginElement());
    return static_cast<const HTMLPlugInElement*>(node);
}

// This will catch anyone doing an unnecessary cast.
void toHTMLPlugInElement(const HTMLPlugInElement*);

} // namespace WebCore

#endif // HTMLPlugInElement_h
