/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. ALl rights reserved.
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

#ifndef HTMLStyleElement_h
#define HTMLStyleElement_h

#include "core/dom/StyleElement.h"
#include "core/html/HTMLElement.h"

namespace WebCore {

class HTMLStyleElement;
class StyleSheet;

template<typename T> class EventSender;
typedef EventSender<HTMLStyleElement> StyleEventSender;

class HTMLStyleElement FINAL : public HTMLElement, private StyleElement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLStyleElement);
public:
    static PassRefPtr<HTMLStyleElement> create(Document&, bool createdByParser);
    virtual ~HTMLStyleElement();

    void setType(const AtomicString&);

    bool scoped() const;
    void setScoped(bool);
    ContainerNode* scopingNode();
    bool isRegisteredAsScoped() const
    {
        // Note: We cannot rely on the 'scoped' attribute still being present when this method is invoked.
        // Therefore we cannot rely on scoped()!
        if (m_scopedStyleRegistrationState == NotRegistered)
            return false;
        return true;
    }

    bool isRegisteredInShadowRoot() const
    {
        return m_scopedStyleRegistrationState == RegisteredInShadowRoot;
    }

    using StyleElement::sheet;

    bool disabled() const;
    void setDisabled(bool);

    void dispatchPendingEvent(StyleEventSender*);
    static void dispatchPendingLoadEvents();

    virtual void trace(Visitor*) OVERRIDE;

private:
    HTMLStyleElement(Document&, bool createdByParser);

    // overload from HTMLElement
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void didNotifySubtreeInsertionsToDocument() OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    virtual void finishParsingChildren() OVERRIDE;

    virtual bool sheetLoaded() OVERRIDE { return StyleElement::sheetLoaded(document()); }
    virtual void notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred) OVERRIDE;
    virtual void startLoadingDynamicSheet() OVERRIDE { StyleElement::startLoadingDynamicSheet(document()); }

    virtual const AtomicString& media() const OVERRIDE;
    virtual const AtomicString& type() const OVERRIDE;

    void scopedAttributeChanged(bool);
    void registerWithScopingNode(bool);
    void unregisterWithScopingNode(ContainerNode*);

    bool m_firedLoad;
    bool m_loadedSheet;

    enum ScopedStyleRegistrationState {
        NotRegistered,
        RegisteredAsScoped,
        RegisteredInShadowRoot
    };
    ScopedStyleRegistrationState m_scopedStyleRegistrationState;
};

} //namespace

#endif
