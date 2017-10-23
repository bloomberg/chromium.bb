// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Frame.h"

#include "core/dom/UserGestureIndicator.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FrameTest : public ::testing::Test {
 public:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    Navigate("https://example.com/");

    ASSERT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
    ASSERT_FALSE(
        GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());
  }

  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }

  void Navigate(const String& destinationUrl) {
    const KURL& url = KURL(NullURL(), destinationUrl);
    FrameLoadRequest request(nullptr, ResourceRequest(url),
                             SubstituteData(SharedBuffer::Create()));
    GetDocument().GetFrame()->Loader().Load(request);
    blink::testing::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }

  void NavigateSameDomain(const String& page) {
    Navigate("https://test.example.com/" + page);
  }

  void NavigateDifferentDomain() { Navigate("https://example.org/"); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(FrameTest, NoGesture) {
  // A nullptr LocalFrame* will not set user gesture state.
  std::unique_ptr<UserGestureIndicator> holder =
      Frame::NotifyUserActivation(nullptr);
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
}

TEST_F(FrameTest, PossiblyExisting) {
  // A non-null LocalFrame* will set state, but a subsequent nullptr Document*
  // token will not override it.
  {
    std::unique_ptr<UserGestureIndicator> holder =
        Frame::NotifyUserActivation(GetDocument().GetFrame());
    EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
  }
  {
    std::unique_ptr<UserGestureIndicator> holder =
        Frame::NotifyUserActivation(nullptr);
    EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
  }
}

TEST_F(FrameTest, NewGesture) {
  // UserGestureToken::Status doesn't impact Document gesture state.
  std::unique_ptr<UserGestureIndicator> holder = Frame::NotifyUserActivation(
      GetDocument().GetFrame(), UserGestureToken::kNewGesture);
  EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
}

TEST_F(FrameTest, NavigateDifferentDomain) {
  std::unique_ptr<UserGestureIndicator> holder =
      Frame::NotifyUserActivation(GetDocument().GetFrame());
  EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());

  // Navigate to a different Document. In the main frame, user gesture state
  // will get reset. State will not persist since the domain has changed.
  NavigateDifferentDomain();
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());
}

TEST_F(FrameTest, NavigateSameDomainMultipleTimes) {
  std::unique_ptr<UserGestureIndicator> holder =
      Frame::NotifyUserActivation(GetDocument().GetFrame());
  EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());

  // Navigate to a different Document in the same domain.  In the main frame,
  // user gesture state will get reset, but persisted state will be true.
  NavigateSameDomain("page1");
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());

  // Navigate to a different Document in the same domain, the persisted
  // state will be true.
  NavigateSameDomain("page2");
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());

  // Navigate to the same URL in the same domain, the persisted state
  // will be true, but the user gesture state will be reset.
  NavigateSameDomain("page2");
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());

  // Navigate to a different Document in the same domain, the persisted
  // state will be true.
  NavigateSameDomain("page3");
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());
}

TEST_F(FrameTest, NavigateSameDomainDifferentDomain) {
  std::unique_ptr<UserGestureIndicator> holder =
      Frame::NotifyUserActivation(GetDocument().GetFrame());
  EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());

  // Navigate to a different Document in the same domain.  In the main frame,
  // user gesture state will get reset, but persisted state will be true.
  NavigateSameDomain("page1");
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_TRUE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());

  // Navigate to a different Document in a different domain, the persisted
  // state will be reset.
  NavigateDifferentDomain();
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());
}

TEST_F(FrameTest, NavigateSameDomainNoGesture) {
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());

  NavigateSameDomain("page1");
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());
}

}  // namespace blink
