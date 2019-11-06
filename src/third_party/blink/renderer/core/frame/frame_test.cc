// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/document_interface_broker_test_helpers.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class FrameTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    Navigate("https://example.com/", false);

    ASSERT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
    ASSERT_FALSE(
        GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());
  }

  void Navigate(const String& destinationUrl, bool user_activated) {
    const KURL& url = KURL(NullURL(), destinationUrl);
    auto navigation_params =
        WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(), url);
    if (user_activated)
      navigation_params->is_user_activated = true;
    GetDocument().GetFrame()->Loader().CommitNavigation(
        std::move(navigation_params), nullptr /* extra_data */);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }

  void NavigateSameDomain(const String& page) {
    NavigateSameDomain(page, true);
  }

  void NavigateSameDomain(const String& page, bool user_activated) {
    Navigate("https://test.example.com/" + page, user_activated);
  }

  void NavigateDifferentDomain() { Navigate("https://example.org/", false); }
};

TEST_F(FrameTest, NoGesture) {
  // A nullptr LocalFrame* will not set user gesture state.
  std::unique_ptr<UserGestureIndicator> holder =
      LocalFrame::NotifyUserActivation(nullptr);
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
}

TEST_F(FrameTest, PossiblyExisting) {
  // A non-null LocalFrame* will set state, but a subsequent nullptr Document*
  // token will not override it.
  {
    std::unique_ptr<UserGestureIndicator> holder =
        LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
    EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
  }
  {
    std::unique_ptr<UserGestureIndicator> holder =
        LocalFrame::NotifyUserActivation(nullptr);
    EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
  }
}

TEST_F(FrameTest, NewGesture) {
  // UserGestureToken::Status doesn't impact Document gesture state.
  std::unique_ptr<UserGestureIndicator> holder =
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame(),
                                       UserGestureToken::kNewGesture);
  EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
}

TEST_F(FrameTest, NavigateDifferentDomain) {
  std::unique_ptr<UserGestureIndicator> holder =
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
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
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
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
      LocalFrame::NotifyUserActivation(GetDocument().GetFrame());
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

  NavigateSameDomain("page1", false);
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      GetDocument().GetFrame()->HasReceivedUserGestureBeforeNavigation());
}

TEST_F(FrameTest, UserActivationInterfaceTest) {
  ScopedUserActivationV2ForTest scoped_feature(true);

  // Initially both sticky and transient bits are false.
  EXPECT_FALSE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));

  LocalFrame::NotifyUserActivation(GetDocument().GetFrame());

  // Now both sticky and transient bits are true, hence consumable.
  EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));
  EXPECT_TRUE(
      LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame()));

  // After consumption, only the transient bit resets to false.
  EXPECT_TRUE(GetDocument().GetFrame()->HasBeenActivated());
  EXPECT_FALSE(
      LocalFrame::HasTransientUserActivation(GetDocument().GetFrame()));
  EXPECT_FALSE(
      LocalFrame::ConsumeTransientUserActivation(GetDocument().GetFrame()));
}

TEST_F(FrameTest, TestDocumentInterfaceBrokerOverride) {
  mojo::PendingRemote<mojom::blink::DocumentInterfaceBroker> doc;
  FrameHostTestDocumentInterfaceBroker frame_interface_broker(
      &GetDocument().GetFrame()->GetDocumentInterfaceBroker(),
      doc.InitWithNewPipeAndPassReceiver());
  GetDocument().GetFrame()->SetDocumentInterfaceBrokerForTesting(
      doc.PassPipe());

  mojo::Remote<mojom::blink::FrameHostTestInterface> frame_test;
  GetDocument()
      .GetFrame()
      ->GetDocumentInterfaceBroker()
      .GetFrameHostTestInterface(frame_test.BindNewPipeAndPassReceiver());
  frame_test->GetName(base::BindOnce([](const WTF::String& result) {
    EXPECT_EQ(result, kGetNameTestResponse);
  }));
  frame_interface_broker.Flush();
}

}  // namespace blink
