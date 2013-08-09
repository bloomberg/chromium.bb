/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef HTMLLinkElement_h
#define HTMLLinkElement_h

#include "core/css/CSSStyleSheet.h"
#include "core/dom/IconURL.h"
#include "core/html/DOMSettableTokenList.h"
#include "core/html/HTMLElement.h"
#include "core/html/LinkRelAttribute.h"
#include "core/html/LinkResource.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/LinkLoaderClient.h"
#include "core/loader/cache/ResourcePtr.h"
#include "core/loader/cache/StyleSheetResourceClient.h"

namespace WebCore {

class DocumentFragment;
class HTMLLinkElement;
class KURL;
class LinkImport;

template<typename T> class EventSender;
typedef EventSender<HTMLLinkElement> LinkEventSender;

//
// LinkStyle handles dynaically change-able link resources, which is
// typically @rel="stylesheet".
//
// It could be @rel="shortcut icon" or soething else though. Each of
// types might better be handled by a separate class, but dynamically
// changing @rel makes it harder to move such a design so we are
// sticking current way so far.
//
class LinkStyle FINAL : public LinkResource, StyleSheetResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<LinkStyle> create(HTMLLinkElement* owner);

    explicit LinkStyle(HTMLLinkElement* owner);
    virtual ~LinkStyle();

    virtual Type type() const OVERRIDE { return Style; }
    virtual void process() OVERRIDE;
    virtual void ownerRemoved() OVERRIDE;
    virtual bool hasLoaded() const OVERRIDE { return m_loadedSheet; }

    void startLoadingDynamicSheet();
    void notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred);
    bool sheetLoaded();

    void setDisabledState(bool);
    void setSheetTitle(const String&);

    bool styleSheetIsLoading() const;
    bool hasSheet() const { return m_sheet; }
    bool isDisabled() const { return m_disabledState == Disabled; }
    bool isEnabledViaScript() const { return m_disabledState == EnabledViaScript; }
    bool isUnset() const { return m_disabledState == Unset; }

    CSSStyleSheet* sheet() const { return m_sheet.get(); }

private:
    // From ResourceClient
    virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CSSStyleSheetResource*);

    enum DisabledState {
        Unset,
        EnabledViaScript,
        Disabled
    };

    enum PendingSheetType {
        None,
        NonBlocking,
        Blocking
    };

    enum RemovePendingSheetNotificationType {
        RemovePendingSheetNotifyImmediately,
        RemovePendingSheetNotifyLater
    };

    void clearSheet();
    void addPendingSheet(PendingSheetType);
    void removePendingSheet(RemovePendingSheetNotificationType = RemovePendingSheetNotifyImmediately);
    Document* document();

    ResourcePtr<CSSStyleSheetResource> m_resource;
    RefPtr<CSSStyleSheet> m_sheet;
    DisabledState m_disabledState;
    PendingSheetType m_pendingSheetType;
    bool m_loading;
    bool m_firedLoad;
    bool m_loadedSheet;
};


class HTMLLinkElement FINAL : public HTMLElement, public LinkLoaderClient {
public:
    static PassRefPtr<HTMLLinkElement> create(const QualifiedName&, Document*, bool createdByParser);
    virtual ~HTMLLinkElement();

    KURL href() const;
    String rel() const;
    String media() const { return m_media; }
    String typeValue() const { return m_type; }
    const LinkRelAttribute& relAttribute() const { return m_relAttribute; }

    virtual String target() const;

    String type() const;

    IconType iconType() const;

    // the icon size string as parsed from the HTML attribute
    String iconSizes() const;

    CSSStyleSheet* sheet() const { return linkStyle() ? linkStyle()->sheet() : 0; }
    Document* import() const;

    bool styleSheetIsLoading() const;

    bool isDisabled() const { return linkStyle() && linkStyle()->isDisabled(); }
    bool isEnabledViaScript() const { return linkStyle() && linkStyle()->isEnabledViaScript(); }
    void setSizes(const String&);
    DOMSettableTokenList* sizes() const;

    void dispatchPendingEvent(LinkEventSender*);
    void scheduleEvent();
    static void dispatchPendingLoadEvents();

    // From LinkLoaderClient
    virtual bool shouldLoadLink() OVERRIDE;

    // For LinkStyle
    bool loadLink(const String& type, const KURL& url) { return m_linkLoader.loadLink(m_relAttribute, type, url, document()); }
    bool isAlternate() const { return linkStyle()->isUnset() && m_relAttribute.isAlternate(); }
    bool shouldProcessStyle() { return linkResourceToProcess() && linkStyle(); }

private:
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    LinkStyle* linkStyle() const;
    LinkImport* linkImport() const;
    LinkResource* linkResourceToProcess();

    void process();
    static void processCallback(Node*);

    // From Node and subclassses
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual bool sheetLoaded() OVERRIDE;
    virtual void notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred) OVERRIDE;
    virtual void startLoadingDynamicSheet() OVERRIDE;
    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const OVERRIDE;
    virtual void finishParsingChildren();

    // From LinkLoaderClient
    virtual void linkLoaded() OVERRIDE;
    virtual void linkLoadingErrored() OVERRIDE;
    virtual void didStartLinkPrerender() OVERRIDE;
    virtual void didStopLinkPrerender() OVERRIDE;
    virtual void didSendLoadForLinkPrerender() OVERRIDE;
    virtual void didSendDOMContentLoadedForLinkPrerender() OVERRIDE;

private:
    HTMLLinkElement(const QualifiedName&, Document*, bool createdByParser);

    RefPtr<LinkResource> m_link;
    LinkLoader m_linkLoader;

    String m_type;
    String m_media;
    RefPtr<DOMSettableTokenList> m_sizes;
    LinkRelAttribute m_relAttribute;

    bool m_createdByParser;
    bool m_isInShadowTree;
    int m_beforeLoadRecurseCount;
};

inline HTMLLinkElement* toHTMLLinkElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->hasTagName(HTMLNames::linkTag));
    return static_cast<HTMLLinkElement*>(node);
}

} //namespace

#endif
