// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test/web_engine_browser_test.h"

#include "base/files/file_path.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/test/frame_for_test.h"
#include "fuchsia/engine/test/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"

namespace {

constexpr char kAutoplayVp8Url[] = "/play_video.html?autoplay=1&codecs=vp8";

}  // namespace

class AutoplayTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  AutoplayTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~AutoplayTest() override = default;

  AutoplayTest(const AutoplayTest&) = delete;
  AutoplayTest& operator=(const AutoplayTest&) = delete;

  void SetUpOnMainThread() override {
    CHECK(embedded_test_server()->Start());
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) final {
    SetHeadlessInCommandLine(command_line);
    cr_fuchsia::WebEngineBrowserTest::SetUpCommandLine(command_line);
  }

 protected:
  // Creates a Frame with |navigation_listener_| attached and |policy|
  // applied.
  cr_fuchsia::FrameForTest CreateFrame(fuchsia::web::AutoplayPolicy policy) {
    fuchsia::web::CreateFrameParams params;
    params.set_autoplay_policy(policy);
    auto frame = cr_fuchsia::FrameForTest::Create(context(), std::move(params));
    return frame;
  }
};

IN_PROC_BROWSER_TEST_F(
    AutoplayTest,
    UserActivationPolicy_UserActivatedViaSimulatedInteraction) {
  const GURL kUrl(embedded_test_server()->GetURL(kAutoplayVp8Url));
  constexpr const char kPageLoadedTitle[] = "initial title";

  cr_fuchsia::FrameForTest frame =
      CreateFrame(fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION);

  fuchsia::web::LoadUrlParams params;
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), {}, kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, kPageLoadedTitle);

  context_impl()
      ->GetFrameImplForTest(&frame.ptr())
      ->web_contents_for_test()
      ->GetMainFrame()
      ->NotifyUserActivation(
          blink::mojom::UserActivationNotificationType::kTest);

  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, "playing");
}

IN_PROC_BROWSER_TEST_F(AutoplayTest,
                       UserActivationPolicy_UserActivatedNavigation) {
  const GURL kUrl(embedded_test_server()->GetURL(kAutoplayVp8Url));

  cr_fuchsia::FrameForTest frame =
      CreateFrame(fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION);

  fuchsia::web::LoadUrlParams params;
  params.set_was_user_activated(true);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), std::move(params), kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, "playing");
}

IN_PROC_BROWSER_TEST_F(AutoplayTest, UserActivationPolicy_NoUserActivation) {
  const GURL kUrl(embedded_test_server()->GetURL(kAutoplayVp8Url));

  cr_fuchsia::FrameForTest frame =
      CreateFrame(fuchsia::web::AutoplayPolicy::REQUIRE_USER_ACTIVATION);

  fuchsia::web::LoadUrlParams params;
  params.set_was_user_activated(false);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), std::move(params), kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, "blocked");
}

IN_PROC_BROWSER_TEST_F(AutoplayTest,
                       AllowAllPolicy_DefaultNotUserActivatedNavigation) {
  const GURL kUrl(embedded_test_server()->GetURL(kAutoplayVp8Url));

  cr_fuchsia::FrameForTest frame =
      CreateFrame(fuchsia::web::AutoplayPolicy::ALLOW);

  // The page is deliberately not user activated.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, "playing");
}
