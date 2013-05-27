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
#include "core/loader/LinkLoader.h"
#include "core/loader/LinkLoaderClient.h"
#include "core/loader/cache/CachedResourceHandle.h"
#include "core/loader/cache/CachedStyleSheetClient.h"
#include "core/platform/Timer.h"

namespace WebCore {

class HTMLLinkElement;
class KURL;

template<typename T> class EventSender;
typedef EventSender<HTMLLinkElement> LinkEventSender;

class LinkStyle FINAL : public CachedStyleSheetClient {
public:
    explicit LinkStyle(HTMLLinkElement* owner);
    ~LinkStyle();

    void startLoadingDynamicSheet();
    void notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred);
    bool sheetLoaded();

    void setDisabledState(bool);
    void process();
    void ownerRemoved();
    void setSheetTitle(const String&);

    bool styleSheetIsLoading() const;
    bool hasSheet() const { return m_sheet; }
    bool hasLoadedSheet() const { return m_loadedSheet; }
    bool isDisabled() const { return m_disabledState == Disabled; }
    bool isEnabledViaScript() const { return m_disabledState == EnabledViaScript; }
    bool isUnset() const { return m_disabledState == Unset; }

    CSSStyleSheet* sheet() const { return m_sheet.get(); }

private:
    // From CachedResourceClient
    virtual void setCSSStyleSheet(const String& href, const KURL& baseURL, const String& charset, const CachedCSSStyleSheet*);

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

    CachedResourceHandle<CachedCSSStyleSheet> m_cachedSheet;
    RefPtr<CSSStyleSheet> m_sheet;
    DisabledState m_disabledState;
    PendingSheetType m_pendingSheetType;
    bool m_loading;
    bool m_firedLoad;
    bool m_loadedSheet;
    HTMLLinkElement* m_owner;
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

    CSSStyleSheet* sheet() const { return m_link ? m_link->sheet() : 0; }

    bool styleSheetIsLoading() const;

    bool isDisabled() const { return m_link->isDisabled(); }
    bool isEnabledViaScript() const { return m_link->isEnabledViaScript(); }
    void setSizes(const String&);
    DOMSettableTokenList* sizes() const;

    void dispatchPendingEvent(LinkEventSender*);
    static void dispatchPendingLoadEvents();

    // From LinkLoaderClient
    virtual bool shouldLoadLink() OVERRIDE;

    // For LinkStyle
    bool loadLink(const String& type, const KURL& url) { return m_linkLoader.loadLink(m_relAttribute, type, url, document()); }
    bool isAlternate() const { return m_link->isUnset() && m_relAttribute.isAlternate(); }
    bool shouldProcessStyle() { return linkStyleToProcess(); }

private:
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    LinkStyle* linkStyle() const { return m_link.get(); }
    LinkStyle* linkStyleToProcess();

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

    OwnPtr<LinkStyle> m_link;
    LinkLoader m_linkLoader;

    String m_type;
    String m_media;
    RefPtr<DOMSettableTokenList> m_sizes;
    LinkRelAttribute m_relAttribute;

    bool m_createdByParser;
    bool m_isInShadowTree;
    int m_beforeLoadRecurseCount;
};

} //namespace

#endif
