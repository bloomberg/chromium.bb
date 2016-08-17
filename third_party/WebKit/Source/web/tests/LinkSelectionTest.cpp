// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Range.h"
#include "core/frame/FrameView.h"
#include "core/input/EventHandler.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuController.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "platform/Cursor.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/web/WebSettings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

using ::testing::_;

namespace blink {

IntSize scaled(IntSize p, float scale)
{
    p.scale(scale, scale);
    return p;
}

class LinkSelectionTestBase : public ::testing::Test {
protected:
    enum DragFlag {
        SendDownEvent = 1,
        SendUpEvent = 1 << 1
    };
    using DragFlags = unsigned;

    void emulateMouseDrag(const IntPoint& downPoint, const IntPoint& upPoint, int modifiers,
        DragFlags = SendDownEvent | SendUpEvent);

    void emulateMouseClick(const IntPoint& clickPoint, WebMouseEvent::Button, int modifiers, int count = 1);
    void emulateMouseDown(const IntPoint& clickPoint, WebMouseEvent::Button, int modifiers, int count = 1);

    String getSelectionText();

    FrameTestHelpers::WebViewHelper m_helper;
    WebViewImpl* m_webView = nullptr;
    Persistent<WebLocalFrameImpl> m_mainFrame = nullptr;
};

void LinkSelectionTestBase::emulateMouseDrag(const IntPoint& downPoint, const IntPoint& upPoint, int modifiers, DragFlags dragFlags)
{
    if (dragFlags & SendDownEvent) {
        const auto& downEvent = FrameTestHelpers::createMouseEvent(WebMouseEvent::MouseDown, WebMouseEvent::ButtonLeft, downPoint, modifiers);
        m_webView->handleInputEvent(downEvent);
    }

    const int kMoveEventsNumber = 10;
    const float kMoveIncrementFraction = 1. / kMoveEventsNumber;
    const auto& upDownVector = upPoint - downPoint;
    for (int i = 0; i < kMoveEventsNumber; ++i) {
        const auto& movePoint = downPoint + scaled(upDownVector, i * kMoveIncrementFraction);
        const auto& moveEvent = FrameTestHelpers::createMouseEvent(WebMouseEvent::MouseMove, WebMouseEvent::ButtonLeft, movePoint, modifiers);
        m_webView->handleInputEvent(moveEvent);
    }

    if (dragFlags & SendUpEvent) {
        const auto& upEvent = FrameTestHelpers::createMouseEvent(WebMouseEvent::MouseUp, WebMouseEvent::ButtonLeft, upPoint, modifiers);
        m_webView->handleInputEvent(upEvent);
    }
}

void LinkSelectionTestBase::emulateMouseClick(const IntPoint& clickPoint, WebMouseEvent::Button button, int modifiers, int count)
{
    auto event = FrameTestHelpers::createMouseEvent(WebMouseEvent::MouseDown, button, clickPoint, modifiers);
    event.clickCount = count;
    m_webView->handleInputEvent(event);
    event.type = WebMouseEvent::MouseUp;
    m_webView->handleInputEvent(event);
}

void LinkSelectionTestBase::emulateMouseDown(const IntPoint& clickPoint, WebMouseEvent::Button button, int modifiers, int count)
{
    auto event = FrameTestHelpers::createMouseEvent(WebMouseEvent::MouseDown, button, clickPoint, modifiers);
    event.clickCount = count;
    m_webView->handleInputEvent(event);
}

String LinkSelectionTestBase::getSelectionText()
{
    return m_mainFrame->selectionAsText();
}

class TestFrameClient : public FrameTestHelpers::TestWebFrameClient {
public:
    MOCK_METHOD4(loadURLExternally,
        void(const WebURLRequest&, WebNavigationPolicy, const WebString& downloadName, bool shouldReplaceCurrentEntry));
};

class LinkSelectionTest : public LinkSelectionTestBase {
protected:
    void SetUp() override
    {
        const char* const kHTMLString =
            "<a id='link' href='foo.com' style='font-size:20pt'>Text to select foobar</a>"
            "<div id='page_text'>Lorem ipsum dolor sit amet</div>";

        // We need to set deviceSupportsMouse setting to true and page's focus controller to active
        // so that FrameView can set the mouse cursor.
        m_webView = m_helper.initialize(false, &m_testFrameClient, nullptr, nullptr,
            [](WebSettings* settings) { settings->setDeviceSupportsMouse(true); });
        m_mainFrame = m_webView->mainFrameImpl();
        FrameTestHelpers::loadHTMLString(m_mainFrame, kHTMLString, URLTestHelpers::toKURL("http://foobar.com"));
        m_webView->resize(WebSize(800, 600));
        m_webView->page()->focusController().setActive(true);

        auto* document = m_mainFrame->frame()->document();
        ASSERT_NE(nullptr, document);
        auto* linkToSelect = document->getElementById("link")->firstChild();
        ASSERT_NE(nullptr, linkToSelect);
        // We get larger range that we actually want to select, because we need a slightly larger
        // rect to include the last character to the selection.
        const auto rangeToSelect = Range::create(*document, linkToSelect, 5, linkToSelect, 16);

        const auto& selectionRect = rangeToSelect->boundingBox();
        const auto& selectionRectCenterY = selectionRect.center().y();
        m_leftPointInLink = selectionRect.minXMinYCorner();
        m_leftPointInLink.setY(selectionRectCenterY);

        m_rightPointInLink = selectionRect.maxXMinYCorner();
        m_rightPointInLink.setY(selectionRectCenterY);
        m_rightPointInLink.move(-2, 0);
    }

    TestFrameClient m_testFrameClient;
    IntPoint m_leftPointInLink;
    IntPoint m_rightPointInLink;
};

TEST_F(LinkSelectionTest, MouseDragWithoutAltAllowNoLinkSelection)
{
    emulateMouseDrag(m_leftPointInLink, m_rightPointInLink, 0);
    EXPECT_TRUE(getSelectionText().isEmpty());
}

TEST_F(LinkSelectionTest, MouseDragWithAltAllowSelection)
{
    emulateMouseDrag(m_leftPointInLink, m_rightPointInLink, WebInputEvent::AltKey);
    EXPECT_EQ("to select", getSelectionText());
}

TEST_F(LinkSelectionTest, HandCursorDuringLinkDrag)
{
    emulateMouseDrag(m_rightPointInLink, m_leftPointInLink, 0, SendDownEvent);
    m_mainFrame->frame()->localFrameRoot()->eventHandler().scheduleCursorUpdate();
    testing::runDelayedTasks(50);
    const auto& cursor = m_mainFrame->frame()->chromeClient().lastSetCursorForTesting();
    EXPECT_EQ(Cursor::Hand, cursor.getType());
}

TEST_F(LinkSelectionTest, CaretCursorOverLinkDuringSelection)
{
    emulateMouseDrag(m_rightPointInLink, m_leftPointInLink, WebInputEvent::AltKey, SendDownEvent);
    m_mainFrame->frame()->localFrameRoot()->eventHandler().scheduleCursorUpdate();
    testing::runDelayedTasks(50);
    const auto& cursor = m_mainFrame->frame()->chromeClient().lastSetCursorForTesting();
    EXPECT_EQ(Cursor::IBeam, cursor.getType());
}

TEST_F(LinkSelectionTest, HandCursorOverLinkAfterContextMenu)
{
    // Move mouse.
    emulateMouseDrag(m_rightPointInLink, m_leftPointInLink, 0, 0);

    // Show context menu. We don't send mouseup event here since in browser it doesn't reach
    // blink because of shown context menu.
    emulateMouseDown(m_leftPointInLink, WebMouseEvent::ButtonRight, 0, 1);

    LocalFrame* frame = m_mainFrame->frame();
    // Hide context menu.
    frame->page()->contextMenuController().clearContextMenu();

    frame->localFrameRoot()->eventHandler().scheduleCursorUpdate();
    testing::runDelayedTasks(50);
    const auto& cursor = m_mainFrame->frame()->chromeClient().lastSetCursorForTesting();
    EXPECT_EQ(Cursor::Hand, cursor.getType());
}

TEST_F(LinkSelectionTest, SingleClickWithAltStartsDownload)
{
    EXPECT_CALL(m_testFrameClient, loadURLExternally(_, WebNavigationPolicy::WebNavigationPolicyDownload, WebString(), _));
    emulateMouseClick(m_leftPointInLink, WebMouseEvent::ButtonLeft, WebInputEvent::AltKey);
}

TEST_F(LinkSelectionTest, SingleClickWithAltStartsDownloadWhenTextSelected)
{
    auto* document = m_mainFrame->frame()->document();
    auto* textToSelect = document->getElementById("page_text")->firstChild();
    ASSERT_NE(nullptr, textToSelect);

    // Select some page text outside the link element.
    const Range* rangeToSelect = Range::create(*document, textToSelect, 1, textToSelect, 20);
    const auto& selectionRect = rangeToSelect->boundingBox();
    m_mainFrame->moveRangeSelection(selectionRect.minXMinYCorner(), selectionRect.maxXMaxYCorner());
    EXPECT_FALSE(getSelectionText().isEmpty());

    EXPECT_CALL(m_testFrameClient, loadURLExternally(_, WebNavigationPolicy::WebNavigationPolicyDownload, WebString(), _));
    emulateMouseClick(m_leftPointInLink, WebMouseEvent::ButtonLeft, WebInputEvent::AltKey);
}

class LinkSelectionClickEventsTest : public LinkSelectionTestBase {
protected:
    class MockEventListener final : public EventListener {
    public:
        static MockEventListener* create()
        {
            return new MockEventListener();
        }

        bool operator==(const EventListener& other) const final
        {
            return this == &other;
        }

        MOCK_METHOD2(handleEvent, void(ExecutionContext* executionContext, Event*));

    private:
        MockEventListener() : EventListener(CPPEventListenerType)
        {
        }
    };

    void SetUp() override
    {
        const char* const kHTMLString =
            "<div id='empty_div' style='width: 100px; height: 100px;'></div>"
            "<span id='text_div'>Sometexttoshow</span>";

        m_webView = m_helper.initialize(false);
        m_mainFrame = m_webView->mainFrameImpl();
        FrameTestHelpers::loadHTMLString(m_mainFrame, kHTMLString, URLTestHelpers::toKURL("http://foobar.com"));
        m_webView->resize(WebSize(800, 600));
        m_webView->page()->focusController().setActive(true);

        auto* document = m_mainFrame->frame()->document();
        ASSERT_NE(nullptr, document);

        auto* emptyDiv = document->getElementById("empty_div");
        auto* textDiv = document->getElementById("text_div");
        ASSERT_NE(nullptr, emptyDiv);
        ASSERT_NE(nullptr, textDiv);
    }

    void checkMouseClicks(Element& element, bool doubleClickEvent)
    {
        struct ScopedListenersCleaner {
            ScopedListenersCleaner(Element* element) : m_element(element) {}

            ~ScopedListenersCleaner()
            {
                m_element->removeAllEventListeners();
            }

            Persistent<Element> m_element;
        } const listenersCleaner(&element);

        MockEventListener* eventHandler = MockEventListener::create();
        element.addEventListener(
            doubleClickEvent ? EventTypeNames::dblclick : EventTypeNames::click,
            eventHandler);

        ::testing::InSequence s;
        EXPECT_CALL(*eventHandler, handleEvent(_, _)).Times(1);

        const auto& elemBounds = element.boundsInViewport();
        const int clickCount = doubleClickEvent ? 2 : 1;
        emulateMouseClick(elemBounds.center(), WebMouseEvent::ButtonLeft, 0, clickCount);

        if (doubleClickEvent) {
            EXPECT_EQ(element.innerText().isEmpty(), getSelectionText().isEmpty());
        }
    }
};

TEST_F(LinkSelectionClickEventsTest, SingleAndDoubleClickWillBeHandled)
{
    auto* document = m_mainFrame->frame()->document();
    auto* element = document->getElementById("empty_div");

    {
        SCOPED_TRACE("Empty div, single click");
        checkMouseClicks(*element, false);
    }

    {
        SCOPED_TRACE("Empty div, double click");
        checkMouseClicks(*element, true);
    }

    element = document->getElementById("text_div");

    {
        SCOPED_TRACE("Text div, single click");
        checkMouseClicks(*element, false);
    }

    {
        SCOPED_TRACE("Text div, double click");
        checkMouseClicks(*element, true);
    }
}

} // namespace blink
