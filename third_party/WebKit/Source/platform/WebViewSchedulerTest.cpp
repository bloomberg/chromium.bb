// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebViewScheduler.h"

#include "public/platform/WebFrameScheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace blink {

class MockWebViewScheduler : public WebViewScheduler {
public:
    ~MockWebViewScheduler() override {}

    std::unique_ptr<WebFrameScheduler> createFrameScheduler(BlameContext*) override
    {
        return nullptr;
    }

    MOCK_METHOD1(setPageVisible, void(bool));
    MOCK_METHOD0(enableVirtualTime, void());
    MOCK_METHOD1(setAllowVirtualTimeToAdvance, void(bool));
    MOCK_CONST_METHOD0(virtualTimeAllowedToAdvance, bool());
};

class WebViewSchedulerTest : public ::testing::Test {
public:
    MockWebViewScheduler m_mockWebViewScheduler;
};

TEST_F(WebViewSchedulerTest, setVirtualTimePolicy_ADVANCE)
{
    EXPECT_CALL(m_mockWebViewScheduler, setAllowVirtualTimeToAdvance(true)).Times(1);
    m_mockWebViewScheduler.setVirtualTimePolicy(WebViewScheduler::VirtualTimePolicy::ADVANCE);
}

TEST_F(WebViewSchedulerTest, setVirtualTimePolicy_PAUSE)
{
    EXPECT_CALL(m_mockWebViewScheduler, setAllowVirtualTimeToAdvance(false)).Times(1);
    m_mockWebViewScheduler.setVirtualTimePolicy(WebViewScheduler::VirtualTimePolicy::PAUSE);
}

TEST_F(WebViewSchedulerTest, setVirtualTimePolicy_PAUSE_IF_NETWORK_FETCHES_PENDING_NothingPending)
{
    EXPECT_CALL(m_mockWebViewScheduler, setAllowVirtualTimeToAdvance(true)).Times(1);
    m_mockWebViewScheduler.setVirtualTimePolicy(WebViewScheduler::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING);
}

TEST_F(WebViewSchedulerTest, setVirtualTimePolicy_PAUSE_IF_NETWORK_FETCHES_PENDING_OneLoadPending)
{
    m_mockWebViewScheduler.incrementPendingResourceLoadCount();

    EXPECT_CALL(m_mockWebViewScheduler, setAllowVirtualTimeToAdvance(false)).Times(1);
    m_mockWebViewScheduler.setVirtualTimePolicy(WebViewScheduler::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING);
}

TEST_F(WebViewSchedulerTest, VirtualTimePolicy_PAUSE_IF_NETWORK_FETCHES_PENDING)
{
    EXPECT_CALL(m_mockWebViewScheduler, setAllowVirtualTimeToAdvance(_)).Times(1);
    m_mockWebViewScheduler.setVirtualTimePolicy(WebViewScheduler::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING);
    Mock::VerifyAndClearExpectations(&m_mockWebViewScheduler);

    EXPECT_CALL(m_mockWebViewScheduler, setAllowVirtualTimeToAdvance(false)).Times(1);
    m_mockWebViewScheduler.incrementPendingResourceLoadCount();
    Mock::VerifyAndClearExpectations(&m_mockWebViewScheduler);

    EXPECT_CALL(m_mockWebViewScheduler, setAllowVirtualTimeToAdvance(_)).Times(0);
    m_mockWebViewScheduler.incrementPendingResourceLoadCount();
    m_mockWebViewScheduler.decrementPendingResourceLoadCount();
    m_mockWebViewScheduler.incrementPendingResourceLoadCount();
    m_mockWebViewScheduler.decrementPendingResourceLoadCount();
    Mock::VerifyAndClearExpectations(&m_mockWebViewScheduler);

    EXPECT_CALL(m_mockWebViewScheduler, setAllowVirtualTimeToAdvance(true)).Times(1);
    m_mockWebViewScheduler.decrementPendingResourceLoadCount();
}

} // namespace blink
