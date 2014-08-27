/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "platform/PopupMenu.h"

#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLSelectElement.h"
#include "core/page/EventHandler.h"
#include "core/rendering/RenderMenuList.h"
#include "core/testing/URLTestHelpers.h"
#include "platform/KeyboardCodes.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/PopupMenuClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/Color.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebDocument.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "public/web/WebViewClient.h"
#include "v8.h"
#include "web/PopupContainer.h"
#include "web/PopupListBox.h"
#include "web/PopupMenuChromium.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPopupMenuImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include <gtest/gtest.h>

using namespace blink;
using blink::URLTestHelpers::toKURL;

namespace {

class TestPopupMenuClient : public PopupMenuClient {
public:
    // Item at index 0 is selected by default.
    TestPopupMenuClient() : m_selectIndex(0), m_node(0), m_listSize(10) { }
    virtual ~TestPopupMenuClient() { }
    virtual void valueChanged(unsigned listIndex, bool fireEvents = true)
    {
        m_selectIndex = listIndex;
        if (m_node) {
            HTMLSelectElement* select = toHTMLSelectElement(m_node);
            select->optionSelectedByUser(select->listToOptionIndex(listIndex), fireEvents);
        }
    }
    virtual void selectionChanged(unsigned, bool) { }
    virtual void selectionCleared() { }

    virtual String itemText(unsigned listIndex) const
    {
        String str("Item ");
        str.append(String::number(listIndex));
        return str;
    }
    virtual String itemLabel(unsigned) const { return String(); }
    virtual String itemIcon(unsigned) const { return String(); }
    virtual String itemToolTip(unsigned listIndex) const { return itemText(listIndex); }
    virtual String itemAccessibilityText(unsigned listIndex) const { return itemText(listIndex); }
    virtual bool itemIsEnabled(unsigned listIndex) const { return m_disabledIndexSet.find(listIndex) == m_disabledIndexSet.end(); }
    virtual PopupMenuStyle itemStyle(unsigned listIndex) const
    {
        FontDescription fontDescription;
        fontDescription.setComputedSize(12.0);
        Font font(fontDescription);
        font.update(nullptr);
        return PopupMenuStyle(Color::black, Color::white, font, true, false, Length(), TextDirection(), false /* has text direction override */);
    }
    virtual PopupMenuStyle menuStyle() const { return itemStyle(0); }
    virtual int clientInsetLeft() const { return 0; }
    virtual int clientInsetRight() const { return 0; }
    virtual LayoutUnit clientPaddingLeft() const { return 0; }
    virtual LayoutUnit clientPaddingRight() const { return 0; }
    virtual int listSize() const { return m_listSize; }
    virtual int selectedIndex() const { return m_selectIndex; }
    virtual void popupDidHide() { }
    virtual bool itemIsSeparator(unsigned listIndex) const { return false; }
    virtual bool itemIsLabel(unsigned listIndex) const { return false; }
    virtual bool itemIsSelected(unsigned listIndex) const { return listIndex == m_selectIndex; }
    virtual bool valueShouldChangeOnHotTrack() const { return false; }
    virtual void setTextFromItem(unsigned listIndex) { }

    virtual FontSelector* fontSelector() const { return 0; }
    virtual HostWindow* hostWindow() const { return 0; }

    virtual PassRefPtr<Scrollbar> createScrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize) { return nullptr; }

    void setDisabledIndex(unsigned index) { m_disabledIndexSet.insert(index); }
    void setFocusedNode(Node* node) { m_node = node; }
    void setListSize(int listSize) { m_listSize = listSize; }

private:
    unsigned m_selectIndex;
    std::set<unsigned> m_disabledIndexSet;
    Node* m_node;
    int m_listSize;
};

class TestWebWidgetClient : public WebWidgetClient {
public:
    ~TestWebWidgetClient() { }
};

class TestWebPopupMenuImpl : public WebPopupMenuImpl {
public:
    static PassRefPtr<TestWebPopupMenuImpl> create(WebWidgetClient* client)
    {
        return adoptRef(new TestWebPopupMenuImpl(client));
    }

    ~TestWebPopupMenuImpl() { }

private:
    TestWebPopupMenuImpl(WebWidgetClient* client) : WebPopupMenuImpl(client) { }
};

class PopupTestWebViewClient : public FrameTestHelpers::TestWebViewClient {
public:
    PopupTestWebViewClient() : m_webPopupMenu(TestWebPopupMenuImpl::create(&m_webWidgetClient)) { }
    ~PopupTestWebViewClient() { }

    virtual WebWidget* createPopupMenu(WebPopupType) { return m_webPopupMenu.get(); }

    // We need to override this so that the popup menu size is not 0
    // (the layout code checks to see if the popup fits on the screen).
    virtual WebScreenInfo screenInfo()
    {
        WebScreenInfo screenInfo;
        screenInfo.availableRect.height = 2000;
        screenInfo.availableRect.width = 2000;
        return screenInfo;
    }

private:
    TestWebWidgetClient m_webWidgetClient;
    RefPtr<TestWebPopupMenuImpl> m_webPopupMenu;
};

class SelectPopupMenuTest : public testing::Test {
public:
    SelectPopupMenuTest()
        : baseURL("http://www.test.com/")
    {
    }

protected:
    virtual void SetUp()
    {
        m_helper.initialize(false, 0, &m_webviewClient);
        m_popupMenu = adoptRef(new PopupMenuChromium(*mainFrame()->frame(), &m_popupMenuClient));
    }

    virtual void TearDown()
    {
        m_popupMenu = nullptr;
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    // Returns true if there currently is a select popup in the WebView.
    bool popupOpen() const { return webView()->selectPopup(); }

    int selectedIndex() const { return m_popupMenuClient.selectedIndex(); }

    void showPopup()
    {
        m_popupMenu->show(FloatQuad(FloatRect(0, 0, 100, 100)), IntSize(100, 100), 0);
        ASSERT_TRUE(popupOpen());
    }

    void hidePopup()
    {
        m_popupMenu->hide();
        EXPECT_FALSE(popupOpen());
    }

    void simulateKeyDownEvent(int keyCode)
    {
        simulateKeyEvent(WebInputEvent::RawKeyDown, keyCode);
    }

    void simulateKeyUpEvent(int keyCode)
    {
        simulateKeyEvent(WebInputEvent::KeyUp, keyCode);
    }

    // Simulates a key event on the WebView.
    // The WebView forwards the event to the select popup if one is open.
    void simulateKeyEvent(WebInputEvent::Type eventType, int keyCode)
    {
        WebKeyboardEvent keyEvent;
        keyEvent.windowsKeyCode = keyCode;
        keyEvent.type = eventType;
        webView()->handleInputEvent(keyEvent);
    }

    // Simulates a mouse event on the select popup.
    void simulateLeftMouseDownEvent(const IntPoint& point)
    {
        PlatformMouseEvent mouseEvent(point, point, LeftButton, PlatformEvent::MousePressed,
            1, false, false, false, false, 0);
        webView()->selectPopup()->handleMouseDownEvent(mouseEvent);
    }
    void simulateLeftMouseUpEvent(const IntPoint& point)
    {
        PlatformMouseEvent mouseEvent(point, point, LeftButton, PlatformEvent::MouseReleased,
            1, false, false, false, false, 0);
        webView()->selectPopup()->handleMouseReleaseEvent(mouseEvent);
    }

    void registerMockedURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLLoad(toKURL(baseURL + fileName), WebString::fromUTF8(fileName.c_str()), WebString::fromUTF8("popup/"), WebString::fromUTF8("text/html"));
    }

    void loadFrame(WebFrame* frame, const std::string& fileName)
    {
        FrameTestHelpers::loadFrame(frame, baseURL + fileName);
    }

    WebViewImpl* webView() const { return m_helper.webViewImpl(); }
    WebLocalFrameImpl* mainFrame() const { return m_helper.webViewImpl()->mainFrameImpl(); }

protected:
    PopupTestWebViewClient m_webviewClient;
    TestPopupMenuClient m_popupMenuClient;
    RefPtr<PopupMenu> m_popupMenu;
    std::string baseURL;

private:
    FrameTestHelpers::WebViewHelper m_helper;
};

// Tests that show/hide and repeats. Select popups are reused in web pages when
// they are reopened, that what this is testing.
TEST_F(SelectPopupMenuTest, ShowThenHide)
{
    for (int i = 0; i < 3; i++) {
        showPopup();
        hidePopup();
    }
}

// Tests that showing a select popup and deleting it does not cause problem.
// This happens in real-life if a page navigates while a select popup is showing.
TEST_F(SelectPopupMenuTest, ShowThenDelete)
{
    showPopup();
    // Nothing else to do, TearDown() deletes the popup.
}

// Tests that losing focus closes the select popup.
TEST_F(SelectPopupMenuTest, ShowThenLoseFocus)
{
    showPopup();
    // Simulate losing focus.
    webView()->setFocus(false);

    // Popup should have closed.
    EXPECT_FALSE(popupOpen());
}

// Tests that pressing ESC closes the popup.
TEST_F(SelectPopupMenuTest, ShowThenPressESC)
{
    showPopup();
    simulateKeyDownEvent(VKEY_ESCAPE);
    // Popup should have closed.
    EXPECT_FALSE(popupOpen());
}

// Tests selecting an item with the arrows and enter/esc/tab.
TEST_F(SelectPopupMenuTest, SelectWithKeys)
{
    showPopup();
    // Simulate selecting the 2nd item by pressing Down, Down, enter.
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_RETURN);

    // Popup should have closed.
    EXPECT_TRUE(!popupOpen());
    EXPECT_EQ(2, selectedIndex());

    // It should work as well with ESC.
    showPopup();
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_ESCAPE);
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(3, selectedIndex());

    // It should work as well with TAB.
    showPopup();
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_TAB);
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(4, selectedIndex());
}

// Tests that selecting an item with the mouse does select the item and close
// the popup.
TEST_F(SelectPopupMenuTest, ClickItem)
{
    showPopup();

    int menuItemHeight = webView()->selectPopup()->menuItemHeight();
    // menuItemHeight * 1.5 means the Y position on the item at index 1.
    IntPoint row1Point(2, menuItemHeight * 1.5);
    // Simulate a click down/up on the first item.
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    // Popup should have closed and the item at index 1 selected.
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(1, selectedIndex());
}

// Tests that moving the mouse over an item and then clicking outside the select popup
// leaves the seleted item unchanged.
TEST_F(SelectPopupMenuTest, MouseOverItemClickOutside)
{
    showPopup();

    int menuItemHeight = webView()->selectPopup()->menuItemHeight();
    // menuItemHeight * 1.5 means the Y position on the item at index 1.
    IntPoint row1Point(2, menuItemHeight * 1.5);
    // Simulate the mouse moving over the first item.
    PlatformMouseEvent mouseEvent(row1Point, row1Point, NoButton, PlatformEvent::MouseMoved,
        1, false, false, false, false, 0);
    webView()->selectPopup()->handleMouseMoveEvent(mouseEvent);

    // Click outside the popup.
    simulateLeftMouseDownEvent(IntPoint(1000, 1000));

    // Popup should have closed and item 0 should still be selected.
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(0, selectedIndex());
}

// Tests that selecting an item with the keyboard and then clicking outside the select
// popup does select that item.
TEST_F(SelectPopupMenuTest, SelectItemWithKeyboardItemClickOutside)
{
    showPopup();

    // Simulate selecting the 2nd item by pressing Down, Down.
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_DOWN);

    // Click outside the popup.
    simulateLeftMouseDownEvent(IntPoint(1000, 1000));

    // Popup should have closed and the item should have been selected.
    EXPECT_FALSE(popupOpen());
    EXPECT_EQ(2, selectedIndex());
}

TEST_F(SelectPopupMenuTest, DISABLED_SelectItemEventFire)
{
    registerMockedURLLoad("select_event.html");
    webView()->settings()->setJavaScriptEnabled(true);
    loadFrame(mainFrame(), "select_event.html");

    m_popupMenuClient.setFocusedNode(mainFrame()->frame()->document()->focusedElement());

    showPopup();

    int menuItemHeight = webView()->selectPopup()->menuItemHeight();
    // menuItemHeight * 0.5 means the Y position on the item at index 0.
    IntPoint row1Point(2, menuItemHeight * 0.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    WebElement element = webView()->mainFrame()->document().getElementById("message");

    // mousedown event is held by select node, and we don't simulate the event for the node.
    // So we can only see mouseup and click event.
    EXPECT_STREQ("upclick", element.innerText().utf8().data());

    // Disable the item at index 1.
    m_popupMenuClient.setDisabledIndex(1);

    showPopup();
    // menuItemHeight * 1.5 means the Y position on the item at index 1.
    row1Point.setY(menuItemHeight * 1.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    // The item at index 1 is disabled, so the text should not be changed.
    EXPECT_STREQ("upclick", element.innerText().utf8().data());

    showPopup();
    // menuItemHeight * 2.5 means the Y position on the item at index 2.
    row1Point.setY(menuItemHeight * 2.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    // The item is changed to the item at index 2, from index 0, so change event is fired.
    EXPECT_STREQ("upclickchangeupclick", element.innerText().utf8().data());
}

TEST_F(SelectPopupMenuTest, FLAKY_SelectItemKeyEvent)
{
    registerMockedURLLoad("select_event.html");
    webView()->settings()->setJavaScriptEnabled(true);
    loadFrame(mainFrame(), "select_event.html");

    m_popupMenuClient.setFocusedNode(mainFrame()->frame()->document()->focusedElement());

    showPopup();

    // Siumulate to choose the item at index 1 with keyboard.
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_DOWN);
    simulateKeyDownEvent(VKEY_RETURN);

    WebElement element = webView()->mainFrame()->document().getElementById("message");
    // We only can see change event but no other mouse related events.
    EXPECT_STREQ("change", element.innerText().utf8().data());
}

TEST_F(SelectPopupMenuTest, SelectItemRemoveSelectOnChange)
{
    // Make sure no crash, even if select node is removed on 'change' event handler.
    registerMockedURLLoad("select_event_remove_on_change.html");
    webView()->settings()->setJavaScriptEnabled(true);
    loadFrame(mainFrame(), "select_event_remove_on_change.html");

    m_popupMenuClient.setFocusedNode(mainFrame()->frame()->document()->focusedElement());

    showPopup();

    int menuItemHeight = webView()->selectPopup()->menuItemHeight();
    // menuItemHeight * 1.5 means the Y position on the item at index 1.
    IntPoint row1Point(2, menuItemHeight * 1.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    WebElement element = webView()->mainFrame()->document().getElementById("message");
    EXPECT_STREQ("change", element.innerText().utf8().data());
}

TEST_F(SelectPopupMenuTest, SelectItemRemoveSelectOnClick)
{
    // Make sure no crash, even if select node is removed on 'click' event handler.
    registerMockedURLLoad("select_event_remove_on_click.html");
    webView()->settings()->setJavaScriptEnabled(true);
    loadFrame(mainFrame(), "select_event_remove_on_click.html");

    m_popupMenuClient.setFocusedNode(mainFrame()->frame()->document()->focusedElement());

    showPopup();

    int menuItemHeight = webView()->selectPopup()->menuItemHeight();
    // menuItemHeight * 1.5 means the Y position on the item at index 1.
    IntPoint row1Point(2, menuItemHeight * 1.5);
    simulateLeftMouseDownEvent(row1Point);
    simulateLeftMouseUpEvent(row1Point);

    WebElement element = webView()->mainFrame()->document().getElementById("message");
    EXPECT_STREQ("click", element.innerText().utf8().data());
}

#if OS(ANDROID)
TEST_F(SelectPopupMenuTest, DISABLED_PopupListBoxWithOverlayScrollbarEnabled)
#else
TEST_F(SelectPopupMenuTest, PopupListBoxWithOverlayScrollbarEnabled)
#endif
{
    Settings::setMockScrollbarsEnabled(true);
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
    EXPECT_TRUE(ScrollbarTheme::theme()->usesOverlayScrollbars());
    registerMockedURLLoad("select_rtl.html");
    loadFrame(mainFrame(), "select_rtl.html");

    m_popupMenuClient.setFocusedNode(mainFrame()->frame()->document()->focusedElement());
    m_popupMenuClient.setListSize(30);

    showPopup();
    PopupContainer* container = webView()->selectPopup();
    PopupListBox* listBox = container->listBox();

    EXPECT_EQ(container->width(), listBox->contentsSize().width() + 2);
    Settings::setMockScrollbarsEnabled(false);
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(false);
    EXPECT_FALSE(ScrollbarTheme::theme()->usesOverlayScrollbars());
}

#if OS(ANDROID)
TEST_F(SelectPopupMenuTest, DISABLED_PopupListBoxWithOverlayScrollbarDisabled)
#else
TEST_F(SelectPopupMenuTest, PopupListBoxWithOverlayScrollbarDisabled)
#endif
{
    EXPECT_FALSE(ScrollbarTheme::theme()->usesOverlayScrollbars());
    registerMockedURLLoad("select_rtl.html");
    loadFrame(mainFrame(), "select_rtl.html");

    m_popupMenuClient.setFocusedNode(mainFrame()->frame()->document()->focusedElement());
    m_popupMenuClient.setListSize(30);

    showPopup();
    PopupContainer* container = webView()->selectPopup();
    PopupListBox* listBox = container->listBox();

    EXPECT_EQ(container->width(), listBox->contentsSize().width() + ScrollbarTheme::theme()->scrollbarThickness() + 2);
}

class SelectPopupMenuStyleTest : public testing::Test {
public:
    SelectPopupMenuStyleTest()
        : baseURL("http://www.test.com/")
    {
    }

protected:
    virtual void SetUp() OVERRIDE
    {
        m_helper.initialize(false, 0, &m_webviewClient);
    }

    virtual void TearDown() OVERRIDE
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    // Returns true if there currently is a select popup in the WebView.
    bool popupOpen() const { return webView()->selectPopup(); }

    void registerMockedURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLLoad(toKURL(baseURL + fileName), WebString::fromUTF8(fileName.c_str()), WebString::fromUTF8("popup/"), WebString::fromUTF8("text/html"));
    }

    void loadFrame(WebFrame* frame, const std::string& fileName)
    {
        FrameTestHelpers::loadFrame(frame, baseURL + fileName);
    }

    WebViewImpl* webView() const { return m_helper.webViewImpl(); }
    WebLocalFrameImpl* mainFrame() const { return m_helper.webViewImpl()->mainFrameImpl(); }

protected:
    PopupTestWebViewClient m_webviewClient;
    RefPtr<PopupMenu> m_popupMenu;
    std::string baseURL;

private:
    FrameTestHelpers::WebViewHelper m_helper;
};

#if OS(MACOSX) || OS(ANDROID)
TEST_F(SelectPopupMenuStyleTest, DISABLED_PopupListBoxRTLRowWidth)
#else
TEST_F(SelectPopupMenuStyleTest, PopupListBoxRTLRowWidth)
#endif
{
    registerMockedURLLoad("select_rtl_width.html");
    loadFrame(mainFrame(), "select_rtl_width.html");
    HTMLSelectElement* select = toHTMLSelectElement(mainFrame()->frame()->document()->focusedElement());
    RenderMenuList* menuList = toRenderMenuList(select->renderer());
    ASSERT(menuList);
    menuList->showPopup();
    ASSERT(popupOpen());
    PopupListBox* listBox = webView()->selectPopup()->listBox();
    int ltrWidth = listBox->getRowBaseWidth(0);
    int rtlWidth = listBox->getRowBaseWidth(1);
    EXPECT_LT(rtlWidth, ltrWidth);
}

} // namespace
