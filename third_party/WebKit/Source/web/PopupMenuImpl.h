// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PopupMenuImpl_h
#define PopupMenuImpl_h

#include "core/html/forms/PopupMenuClient.h"
#include "core/page/PagePopupClient.h"
#include "platform/PopupMenu.h"

namespace blink {

class ChromeClientImpl;
class PagePopup;
class HTMLElement;
class HTMLHRElement;
class HTMLOptGroupElement;
class HTMLOptionElement;

class PopupMenuImpl final : public PopupMenu, public PagePopupClient {
public:
    static PassRefPtrWillBeRawPtr<PopupMenuImpl> create(ChromeClientImpl*, PopupMenuClient*);
    virtual ~PopupMenuImpl();

    void dispose();

private:
    PopupMenuImpl(ChromeClientImpl*, PopupMenuClient*);

    void addOption(HTMLOptionElement&, SharedBuffer*);
    void addOptGroup(HTMLOptGroupElement&, SharedBuffer*);
    void addSeparator(HTMLHRElement&, SharedBuffer*);
    void addElementStyle(HTMLElement&, SharedBuffer*);

    // PopupMenu functions:
    void show(const FloatQuad& controlPosition, const IntSize& controlSize, int index) override;
    void hide() override;
    void updateFromElement() override;
    void disconnectClient() override;

    // PagePopupClient functions:
    IntSize contentSize() override;
    void writeDocument(SharedBuffer*) override;
    void didWriteDocument(Document&) override;
    void setValueAndClosePopup(int, const String&) override;
    void setValue(const String&) override;
    void closePopup() override;
    Element& ownerElement() override;
    Locale& locale() override;
    void didClosePopup() override;

    ChromeClientImpl* m_chromeClient;
    PopupMenuClient* m_client;
    PagePopup* m_popup;
    // If >= 0, this is the index we should accept when the popup closes.
    // This is used for keyboard navigation, where we want the
    // text to change immediately but set the value on close.
    int m_indexToSetOnClose;
};

}

#endif // PopupMenuImpl_h
