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

#ifndef PopupListBox_h
#define PopupListBox_h

#include "core/dom/Element.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/ScrollView.h"
#include "platform/text/TextDirection.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Font;
class GraphicsContext;
class IntRect;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformGestureEvent;
class PlatformTouchEvent;
class PlatformWheelEvent;
class PopupContainer;
class PopupMenuClient;
typedef unsigned long long TimeStamp;

class PopupContent {
public:
    virtual void layout() = 0;
    virtual void setMaxHeight(int) = 0;
    virtual void setMaxWidthAndLayout(int) = 0;
    virtual int popupContentHeight() const = 0;
    virtual ~PopupContent() { };
};

// A container for the data for each menu item (e.g. represented by <option>
// or <optgroup> in a <select> widget) and is used by PopupListBox.
struct PopupItem {
    enum Type {
        TypeOption,
        TypeGroup,
        TypeSeparator
    };

    PopupItem(const String& label, Type type)
        : label(label)
        , type(type)
        , yOffset(0)
    {
    }
    String label;
    Type type;
    int yOffset; // y offset of this item, relative to the top of the popup.
    TextDirection textDirection;
    bool hasTextDirectionOverride;
    bool enabled;
    bool displayNone;
};

// This class manages the scrollable content inside a <select> popup.
class PopupListBox final : public Widget, public ScrollableArea, public PopupContent {
public:
    static PassRefPtr<PopupListBox> create(PopupMenuClient* client, bool deviceSupportsTouch, PopupContainer* container)
    {
        return adoptRef(new PopupListBox(client, deviceSupportsTouch, container));
    }

    // Widget
    virtual void invalidateRect(const IntRect&) override;
    virtual void paint(GraphicsContext*, const IntRect&) override;
    virtual HostWindow* hostWindow() const override;
    virtual void setFrameRect(const IntRect&) override;
    virtual IntPoint convertChildToSelf(const Widget* child, const IntPoint&) const override;
    virtual IntPoint convertSelfToChild(const Widget* child, const IntPoint&) const override;

    // ScrollableArea
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) override;
    virtual bool isActive() const override;
    virtual bool scrollbarsCanBeActive() const override;
    virtual IntRect scrollableAreaBoundingBox() const override;
    virtual bool shouldPlaceVerticalScrollbarOnLeft() const override;
    virtual int scrollSize(ScrollbarOrientation) const override;
    virtual void setScrollOffset(const IntPoint&) override;
    virtual bool isScrollCornerVisible() const override { return false; }
    virtual bool userInputScrollable(ScrollbarOrientation orientation) const override { return orientation == VerticalScrollbar; }
    virtual Scrollbar* verticalScrollbar() const override { return m_verticalScrollbar.get(); }
    virtual IntRect visibleContentRect(IncludeScrollbarsInRect = ExcludeScrollbars) const override;
    virtual IntSize contentsSize() const override { return m_contentsSize; }
    virtual IntPoint scrollPosition() const override { return visibleContentRect().location(); }
    virtual IntPoint maximumScrollPosition() const override; // The maximum position we can be scrolled to.
    virtual IntPoint minimumScrollPosition() const override; // The minimum position we can be scrolled to.
    virtual IntRect scrollCornerRect() const override { return IntRect(); }

    // PopupListBox methods

    bool handleMouseDownEvent(const PlatformMouseEvent&);
    bool handleMouseMoveEvent(const PlatformMouseEvent&);
    bool handleMouseReleaseEvent(const PlatformMouseEvent&);
    bool handleWheelEvent(const PlatformWheelEvent&);
    bool handleKeyEvent(const PlatformKeyboardEvent&);
    bool handleTouchEvent(const PlatformTouchEvent&);
    bool handleGestureEvent(const PlatformGestureEvent&);

    // Closes the popup
    void abandon();

    // Updates our internal list to match the client.
    void updateFromElement();

    // Frees any allocated resources used in a particular popup session.
    void clear();

    // Sets the index of the option that is displayed in the <select> widget in the page
    void setOriginalIndex(int);

    // Gets the index of the item that the user is currently moused over or has
    // selected with the keyboard. This is not the same as the original index,
    // since the user has not yet accepted this input.
    int selectedIndex() const { return m_selectedIndex; }

    // Moves selection down/up the given number of items, scrolling if necessary.
    // Positive is down. The resulting index will be clamped to the range
    // [0, numItems), and non-option items will be skipped.
    void adjustSelectedIndex(int delta);

    // Returns the number of items in the list.
    int numItems() const { return static_cast<int>(m_items.size()); }

    void setBaseWidth(int width) { m_baseWidth = std::min(m_maxWindowWidth, width); }

    // Computes the size of widget and children.
    virtual void layout() override;

    // Returns whether the popup wants to process events for the passed key.
    bool isInterestedInEventForKey(int keyCode);

    // Gets the height of a row.
    int getRowHeight(int index) const;

    int getRowBaseWidth(int index);

    virtual void setMaxHeight(int maxHeight) override { m_maxHeight = maxHeight; }

    void setMaxWidth(int maxWidth) { m_maxWindowWidth = maxWidth; }

    virtual void setMaxWidthAndLayout(int) override;

    void disconnectClient() { m_popupClient = 0; }

    const Vector<PopupItem*>& items() const { return m_items; }

    virtual int popupContentHeight() const override;

    static const int defaultMaxHeight;

protected:
    virtual void invalidateScrollCornerRect(const IntRect&) override { }

private:
    friend class PopupContainer;
    friend class RefCounted<PopupListBox>;

    PopupListBox(PopupMenuClient*, bool deviceSupportsTouch, PopupContainer*);
    virtual ~PopupListBox();

    // Hides the popup. Other classes should not call this. Use abandon instead.
    void hidePopup();

    // Returns true if the selection can be changed to index.
    // Disabled items, or labels cannot be selected.
    bool isSelectableItem(int index);

    // Select an index in the list, scrolling if necessary.
    void selectIndex(int index);

    // Accepts the selected index as the value to be displayed in the <select>
    // widget on the web page, and closes the popup. Returns true if index is
    // accepted.
    bool acceptIndex(int index);

    // Clears the selection (so no row appears selected).
    void clearSelection();

    // Scrolls to reveal the given index.
    void scrollToRevealRow(int index);
    void scrollToRevealSelection() { scrollToRevealRow(m_selectedIndex); }

    // Invalidates the row at the given index.
    void invalidateRow(int index);

    // Get the bounds of a row.
    IntRect getRowBounds(int index);

    // Converts a point to an index of the row the point is over
    int pointToRowIndex(const IntPoint&);

    // Paint an individual row
    void paintRow(GraphicsContext*, const IntRect&, int rowIndex);

    // Test if the given point is within the bounds of the popup window.
    bool isPointInBounds(const IntPoint&);

    // Called when the user presses a text key. Does a prefix-search of the items.
    void typeAheadFind(const PlatformKeyboardEvent&);

    // Returns the font to use for the given row
    Font getRowFont(int index) const;

    // Moves the selection down/up one item, taking care of looping back to the
    // first/last element if m_loopSelectionNavigation is true.
    void selectPreviousRow();
    void selectNextRow();

    int scrollX() const { return scrollPosition().x(); }
    int scrollY() const { return scrollPosition().y(); }
    void updateScrollbars(const IntPoint& desiredOffset);
    void setHasVerticalScrollbar(bool);
    Scrollbar* scrollbarAtWindowPoint(const IntPoint& windowPoint);
    IntRect contentsToWindow(const IntRect&) const;
    void setContentsSize(const IntSize&);
    void adjustScrollbarExistence();
    void updateScrollbarGeometry();
    IntRect windowClipRect() const;

    // If the device is a touch screen we increase the height of menu items
    // to make it easier to unambiguously touch them.
    bool m_deviceSupportsTouch;

    // This is the index of the item marked as "selected" - i.e. displayed in
    // the widget on the page.
    int m_originalIndex;

    // This is the index of the item that the user is hovered over or has
    // selected using the keyboard in the list. They have not confirmed this
    // selection by clicking or pressing enter yet however.
    int m_selectedIndex;

    // If >= 0, this is the index we should accept if the popup is "abandoned".
    // This is used for keyboard navigation, where we want the
    // selection to change immediately, and is only used if the settings
    // acceptOnAbandon field is true.
    int m_acceptedIndexOnAbandon;

    // This is the number of rows visible in the popup. The maximum number
    // visible at a time is defined as being kMaxVisibleRows. For a scrolled
    // popup, this can be thought of as the page size in data units.
    int m_visibleRows;

    // Our suggested width, not including scrollbar.
    int m_baseWidth;

    // The maximum height we can be without being off-screen.
    int m_maxHeight;

    // A list of the options contained within the <select>
    Vector<PopupItem*> m_items;

    // The <select> PopupMenuClient that opened us.
    PopupMenuClient* m_popupClient;

    // The scrollbar which has mouse capture. Mouse events go straight to this
    // if not null.
    RefPtr<Scrollbar> m_capturingScrollbar;

    // The last scrollbar that the mouse was over. Used for mouseover highlights.
    RefPtr<Scrollbar> m_lastScrollbarUnderMouse;

    // The string the user has typed so far into the popup. Used for typeAheadFind.
    String m_typedString;

    // The char the user has hit repeatedly. Used for typeAheadFind.
    UChar m_repeatingChar;

    // The last time the user hit a key. Used for typeAheadFind.
    TimeStamp m_lastCharTime;

    // If width exeeds screen width, we have to clip it.
    int m_maxWindowWidth;

    // To forward last mouse release event.
    RefPtrWillBePersistent<Element> m_focusedElement;

    PopupContainer* m_container;

    RefPtr<Scrollbar> m_verticalScrollbar;
    IntSize m_contentsSize;
    IntPoint m_scrollOffset;
};

} // namespace blink

#endif
