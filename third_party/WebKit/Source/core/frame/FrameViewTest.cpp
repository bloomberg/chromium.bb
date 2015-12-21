// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/FrameView.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/EmptyClients.h"
#include "core/page/Page.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/test/FakeGraphicsLayerFactory.h"
#include "platform/heap/Handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"

// This test ensures that FrameView informs the ChromeClient of changes to the
// paint artifact so that they can be shown to the user (e.g. via the
// compositor).

using testing::_;
using testing::AnyNumber;

namespace blink {
namespace {

class MockChromeClient : public EmptyChromeClient {
public:
    MockChromeClient() : m_hasScheduledAnimation(false) { }

    // ChromeClient
    GraphicsLayerFactory* graphicsLayerFactory() const override
    {
        return FakeGraphicsLayerFactory::instance();
    }
    MOCK_METHOD1(didPaint, void(const PaintArtifact&));
    MOCK_METHOD2(attachRootGraphicsLayer, void(GraphicsLayer*, LocalFrame* localRoot));

    void scheduleAnimation(Widget*) override { m_hasScheduledAnimation = true; }
    bool m_hasScheduledAnimation;
};

class FrameViewTestBase : public testing::Test {
protected:
    FrameViewTestBase()
        : m_chromeClient(adoptPtrWillBeNoop(new MockChromeClient))
    { }

    void SetUp() override
    {
        Page::PageClients clients;
        fillWithEmptyClients(clients);
        clients.chromeClient = m_chromeClient.get();
        m_pageHolder = DummyPageHolder::create(IntSize(800, 600), &clients);
        m_pageHolder->page().settings().setAcceleratedCompositingEnabled(true);
    }

    Document& document() { return m_pageHolder->document(); }
    MockChromeClient& chromeClient() { return *m_chromeClient; }

private:
    OwnPtrWillBePersistent<MockChromeClient> m_chromeClient;
    OwnPtr<DummyPageHolder> m_pageHolder;
};

class FrameViewTest : public FrameViewTestBase {
protected:
    FrameViewTest()
    {
        EXPECT_CALL(chromeClient(), attachRootGraphicsLayer(_, _)).Times(AnyNumber());
    }
};

class FrameViewSlimmingPaintV2Test : public FrameViewTestBase {
protected:
    FrameViewSlimmingPaintV2Test()
    {
        // We shouldn't attach a root graphics layer. In this mode, that's not
        // our responsibility.
        EXPECT_CALL(chromeClient(), attachRootGraphicsLayer(_, _)).Times(0);
    }

    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
        FrameViewTestBase::SetUp();
    }

    void TearDown() override
    {
        m_featuresBackup.restore();
    }

private:
    RuntimeEnabledFeatures::Backup m_featuresBackup;
};

TEST_F(FrameViewSlimmingPaintV2Test, PaintOnce)
{
    EXPECT_CALL(chromeClient(), didPaint(_));
    document().body()->setInnerHTML("Hello world", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
}

TEST_F(FrameViewSlimmingPaintV2Test, PaintAndRepaint)
{
    EXPECT_CALL(chromeClient(), didPaint(_)).Times(2);
    document().body()->setInnerHTML("Hello", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    document().body()->setInnerHTML("Hello world", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
}

TEST_F(FrameViewTest, SetPaintInvalidationDuringUpdateAllLifecyclePhases)
{
    document().body()->setInnerHTML("<div id='a' style='color: blue'>A</div>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    document().getElementById("a")->setAttribute(HTMLNames::styleAttr, "color: green");
    chromeClient().m_hasScheduledAnimation = false;
    document().view()->updateAllLifecyclePhases();
    EXPECT_FALSE(chromeClient().m_hasScheduledAnimation);
}

TEST_F(FrameViewTest, SetPaintInvalidationOutOfUpdateAllLifecyclePhases)
{
    document().body()->setInnerHTML("<div id='a' style='color: blue'>A</div>", ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
    chromeClient().m_hasScheduledAnimation = false;
    document().getElementById("a")->layoutObject()->setShouldDoFullPaintInvalidation();
    EXPECT_TRUE(chromeClient().m_hasScheduledAnimation);
    chromeClient().m_hasScheduledAnimation = false;
    document().getElementById("a")->layoutObject()->setShouldDoFullPaintInvalidation();
    EXPECT_TRUE(chromeClient().m_hasScheduledAnimation);
    chromeClient().m_hasScheduledAnimation = false;
    document().view()->updateAllLifecyclePhases();
    EXPECT_FALSE(chromeClient().m_hasScheduledAnimation);
}

} // namespace
} // namespace blink
