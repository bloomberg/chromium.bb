
/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PopupContainer_h
#define PopupContainer_h

#include "platform/PopupMenuStyle.h"
#include "platform/geometry/FloatQuad.h"
#include "web/PopupListBox.h"

namespace blink {

class ChromeClient;
class FrameView;
class PopupContainerClient;
class PopupMenuClient;
struct WebPopupMenuInfo;

// This class wraps a PopupListBox. It positions the popup, paints the border
// around it, and forwards input events.
// FIXME(skobes): This class can probably be combined with PopupListBox.
class PopupContainer final : public Widget {
public:
    static PassRefPtr<PopupContainer> create(PopupMenuClient*, bool deviceSupportsTouch);

    // Whether a key event should be sent to this popup.
    bool isInterestedInEventForKey(int keyCode);

    // Widget
    virtual void paint(GraphicsContext*, const IntRect&) override;
    virtual void hide() override;
    virtual HostWindow* hostWindow() const override;
    virtual void invalidateRect(const IntRect&) override;
    virtual IntPoint convertChildToSelf(const Widget* child, const IntPoint&) const override;
    virtual IntPoint convertSelfToChild(const Widget* child, const IntPoint&) const override;

    // PopupContainer methods

    bool handleMouseDownEvent(const PlatformMouseEvent&);
    bool handleMouseMoveEvent(const PlatformMouseEvent&);
    bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    bool handleWheelEvent(const PlatformWheelEvent&);
    bool handleKeyEvent(const PlatformKeyboardEvent&);
    bool handleTouchEvent(const PlatformTouchEvent&);
    bool handleGestureEvent(const PlatformGestureEvent&);

    PopupContainerClient* client() const { return m_client; }
    void setClient(PopupContainerClient* client) { m_client = client; }

    // Show the popup
    void showPopup(FrameView*);

    // Show the popup in the specified rect for the specified frame.
    // Note: this code was somehow arbitrarily factored-out of the Popup class
    // so WebViewImpl can create a PopupContainer. This method is used for
    // displaying auto complete popup menus on Mac Chromium, and for all
    // popups on other platforms.
    void showInRect(const FloatQuad& controlPosition, const IntSize& controlSize, FrameView*, int index);

    // Hides the popup.
    void hidePopup();

    // The popup was hidden.
    void notifyPopupHidden();

    PopupListBox* listBox() const { return m_listBox.get(); }

    bool isRTL() const;

    // Gets the index of the item that the user is currently moused-over or
    // has selected with the keyboard up/down arrows.
    int selectedIndex() const;

    // Refresh the popup values from the PopupMenuClient.
    IntRect refresh(const IntRect& targetControlRect);

    // The menu per-item data.
    const Vector<PopupItem*>& popupData() const;

    // The height of a row in the menu.
    int menuItemHeight() const;

    // The size of the font being used.
    int menuItemFontSize() const;

    // The style of the menu being used.
    PopupMenuStyle menuStyle() const;

    // While hovering popup menu window, we want to show tool tip message.
    String getSelectedItemToolTip();

    // This is public for testing.
    static IntRect layoutAndCalculateWidgetRectInternal(IntRect widgetRectInScreen, int targetControlHeight, const FloatRect& windowRect, const FloatRect& screen, bool isRTL, const int rtlOffset, const int verticalOffset, const IntSize& transformOffset, PopupContent*, bool& needToResizeView);

    void disconnectClient() { m_listBox->disconnectClient(); }

    void updateFromElement() { m_listBox->updateFromElement(); }

private:
    friend class WTF::RefCounted<PopupContainer>;

    PopupContainer(PopupMenuClient*, bool deviceSupportsTouch);
    virtual ~PopupContainer();

    // Paint the border.
    void paintBorder(GraphicsContext*, const IntRect&);

    // Layout and calculate popup widget size and location and returns it as IntRect.
    IntRect layoutAndCalculateWidgetRect(int targetControlHeight, const IntSize& transformOffset, const IntPoint& popupInitialCoordinate);

    void fitToListBox();

    void popupOpened(const IntRect& bounds);
    void getPopupMenuInfo(WebPopupMenuInfo*);

    // Returns the ChromeClient of the page this popup is associated with.
    ChromeClient& chromeClient();

    RefPtr<PopupListBox> m_listBox;
    RefPtr<FrameView> m_frameView;

    // m_controlPosition contains the transformed position of the
    // <select>/<input> associated with this popup. m_controlSize is the size
    // of the <select>/<input> without transform.
    // The popup menu will be positioned as follows:
    // LTR : If the popup is positioned down it will align with the bottom left
    //       of m_controlPosition (p4)
    //       If the popup is positioned up it will align with the top left of
    //       m_controlPosition (p1)
    // RTL : If the popup is positioned down it will align with the bottom right
    //       of m_controlPosition (p3)
    //       If the popup is positioned up it will align with the top right of
    //       m_controlPosition (p2)
    FloatQuad m_controlPosition;
    IntSize m_controlSize;

    // Whether the popup is currently open.
    bool m_popupOpen;

    PopupContainerClient* m_client;
};

} // namespace blink

#endif
