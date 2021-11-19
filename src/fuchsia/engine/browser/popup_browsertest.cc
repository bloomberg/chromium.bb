// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>

#include "base/fuchsia/mem_buffer_util.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/browser/frame_impl_browser_test_base.h"
#include "fuchsia/engine/test/frame_for_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPage1Path[] = "/title1.html";
constexpr char kPage2Path[] = "/title2.html";
constexpr char kPage1Title[] = "title 1";

constexpr char kPopupParentPath[] = "/popup_parent.html";
constexpr char kPopupRedirectPath[] = "/popup_child.html";
constexpr char kPopupMultiplePath[] = "/popup_multiple.html";

constexpr char kChildQueryParamName[] = "child_url";
constexpr char kPopupChildFile[] = "popup_child.html";
constexpr char kAutoplayFileAndQuery[] =
    "play_video.html?autoplay=1&codecs=vp8";
constexpr char kAutoPlayBlockedTitle[] = "blocked";
constexpr char kAutoPlaySuccessTitle[] = "playing";

class TestPopupListener : public fuchsia::web::PopupFrameCreationListener {
 public:
  TestPopupListener() = default;
  ~TestPopupListener() override = default;

  TestPopupListener(const TestPopupListener&) = delete;
  TestPopupListener& operator=(const TestPopupListener&) = delete;

  void GetAndAckNextPopup(fuchsia::web::FramePtr* frame,
                          fuchsia::web::PopupFrameCreationInfo* creation_info) {
    if (!frame_) {
      base::RunLoop run_loop;
      received_popup_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    *frame = frame_.Bind();
    *creation_info = std::move(creation_info_);

    popup_ack_callback_();
    popup_ack_callback_ = {};
  }

 private:
  void OnPopupFrameCreated(fidl::InterfaceHandle<fuchsia::web::Frame> frame,
                           fuchsia::web::PopupFrameCreationInfo creation_info,
                           OnPopupFrameCreatedCallback callback) override {
    creation_info_ = std::move(creation_info);
    frame_ = std::move(frame);

    popup_ack_callback_ = std::move(callback);

    if (received_popup_callback_)
      std::move(received_popup_callback_).Run();
  }

  fidl::InterfaceHandle<fuchsia::web::Frame> frame_;
  fuchsia::web::PopupFrameCreationInfo creation_info_;
  base::OnceClosure received_popup_callback_;
  OnPopupFrameCreatedCallback popup_ack_callback_;
};

class PopupTest : public FrameImplTestBaseWithServer {
 public:
  PopupTest()
      : popup_listener_binding_(&popup_listener_),
        popup_nav_listener_binding_(&popup_nav_listener_) {}

  ~PopupTest() override = default;
  PopupTest(const PopupTest&) = delete;
  PopupTest& operator=(const PopupTest&) = delete;

 protected:
  // Builds a URL for the kPopupParentPath page to pop up a Frame with
  // |child_file_and_query|. |child_file_and_query| may optionally include a
  // query string.
  GURL GetParentPageTestServerUrl(const char* child) const {
    const std::string url = base::StringPrintf("%s?%s=%s", kPopupParentPath,
                                               kChildQueryParamName, child);

    return embedded_test_server()->GetURL(url);
  }

  // Loads a page that autoplays video in a popup, populates the popup_*
  // members, and returns its URL.
  GURL LoadAutoPlayingPageInPopup(
      fuchsia::web::CreateFrameParams parent_frame_params) {
    GURL popup_parent_url = GetParentPageTestServerUrl(kAutoplayFileAndQuery);
    GURL popup_child_url = embedded_test_server()->GetURL(
        base::StringPrintf("/%s", kAutoplayFileAndQuery));

    auto parent_frame = cr_fuchsia::FrameForTest::Create(
        context(), std::move(parent_frame_params));

    parent_frame->SetPopupFrameCreationListener(
        popup_listener_binding_.NewBinding());

    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        parent_frame.GetNavigationController(), {}, popup_parent_url.spec()));

    fuchsia::web::PopupFrameCreationInfo popup_info;
    popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
    EXPECT_EQ(popup_info.initial_url(), popup_child_url);

    popup_frame_->SetNavigationEventListener(
        popup_nav_listener_binding_.NewBinding());

    return popup_child_url;
  }

  fuchsia::web::FramePtr popup_frame_;

  TestPopupListener popup_listener_;
  fidl::Binding<fuchsia::web::PopupFrameCreationListener>
      popup_listener_binding_;

  cr_fuchsia::TestNavigationListener popup_nav_listener_;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      popup_nav_listener_binding_;
};

IN_PROC_BROWSER_TEST_F(PopupTest, PopupWindowRedirect) {
  GURL popup_parent_url = GetParentPageTestServerUrl(kPopupChildFile);
  GURL popup_child_url(embedded_test_server()->GetURL(kPopupRedirectPath));
  GURL title1_url(embedded_test_server()->GetURL(kPage1Path));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->SetPopupFrameCreationListener(popup_listener_binding_.NewBinding());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), {}, popup_parent_url.spec()));

  // Verify the popup's initial URL, "popup_child.html".
  fuchsia::web::PopupFrameCreationInfo popup_info;
  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), popup_child_url);

  // Verify that the popup eventually redirects to "title1.html".
  popup_frame_->SetNavigationEventListener(
      popup_nav_listener_binding_.NewBinding());
  popup_nav_listener_.RunUntilUrlAndTitleEquals(title1_url, kPage1Title);
}

IN_PROC_BROWSER_TEST_F(PopupTest, MultiplePopups) {
  GURL popup_parent_url(embedded_test_server()->GetURL(kPopupMultiplePath));
  GURL title1_url(embedded_test_server()->GetURL(kPage1Path));
  GURL title2_url(embedded_test_server()->GetURL(kPage2Path));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->SetPopupFrameCreationListener(popup_listener_binding_.NewBinding());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), {}, popup_parent_url.spec()));

  fuchsia::web::PopupFrameCreationInfo popup_info;
  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), title1_url);

  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), title2_url);
}

// Verifies that the child popup Frame has the same default CreateFrameParams as
// the parent Frame by verifying that autoplay is blocked in the child. This
// mostly verifies that AutoPlaySucceedsis actually modifies behavior.
IN_PROC_BROWSER_TEST_F(PopupTest,
                       PopupFrameHasSameCreateFrameParams_AutoplayBlocked) {
  // The default autoplay_policy is REQUIRE_USER_ACTIVATION.
  fuchsia::web::CreateFrameParams parent_frame_params;

  // Load the page and wait for the popup Frame to be created.
  GURL popup_child_url =
      LoadAutoPlayingPageInPopup(std::move(parent_frame_params));

  // Verify that the child does not autoplay media.
  popup_nav_listener_.RunUntilUrlAndTitleEquals(popup_child_url,
                                                kAutoPlayBlockedTitle);
}

// Verifies that the child popup Frame has the same CreateFrameParams as the
// parent Frame by allowing autoplay in the parent's params and verifying that
// autoplay succeeds in the child.
IN_PROC_BROWSER_TEST_F(PopupTest,
                       PopupFrameHasSameCreateFrameParams_AutoplaySucceeds) {
  // Set autoplay to always be allowed in the parent frame.
  fuchsia::web::CreateFrameParams parent_frame_params;
  parent_frame_params.set_autoplay_policy(fuchsia::web::AutoplayPolicy::ALLOW);

  // Load the page and wait for the popup Frame to be created.
  GURL popup_child_url =
      LoadAutoPlayingPageInPopup(std::move(parent_frame_params));

  // Verify that the child autoplays media.
  popup_nav_listener_.RunUntilUrlAndTitleEquals(popup_child_url,
                                                kAutoPlaySuccessTitle);
}

}  // namespace
