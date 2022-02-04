// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::NiceMock;

class FakeCastToolbarIcon : public MediaRouterActionController::Observer {
 public:
  FakeCastToolbarIcon() = default;
  ~FakeCastToolbarIcon() override = default;

  void ShowIcon() override { icon_shown_ = true; }

  void HideIcon() override { icon_shown_ = false; }

  void ActivateIcon() override {}
  void DeactivateIcon() override {}

  bool IsShown() const { return icon_shown_; }

 private:
  bool icon_shown_ = false;
};

class MediaRouterActionControllerUnitTest : public BrowserWithTestWindowTest {
 public:
  MediaRouterActionControllerUnitTest()
      : issue_(media_router::IssueInfo(
            "title notification",
            media_router::IssueInfo::Action::DISMISS,
            media_router::IssueInfo::Severity::NOTIFICATION)) {}

  MediaRouterActionControllerUnitTest(
      const MediaRouterActionControllerUnitTest&) = delete;
  MediaRouterActionControllerUnitTest& operator=(
      const MediaRouterActionControllerUnitTest&) = delete;

  ~MediaRouterActionControllerUnitTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    router_ = std::make_unique<NiceMock<media_router::MockMediaRouter>>();
    controller_ =
        std::make_unique<MediaRouterActionController>(profile(), router_.get());
    controller_->AddObserver(&icon_);

    SetAlwaysShowActionPref(false);

    local_display_route_list_.emplace_back("routeId1", source1_, "sinkId1",
                                           "description", true);
    non_local_display_route_list_.emplace_back("routeId2", source1_, "sinkId2",
                                               "description", false);
  }

  void TearDown() override {
    controller_.reset();
    router_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  bool IsIconShown() const {
    base::RunLoop().RunUntilIdle();
    return icon_.IsShown();
  }

  void SetAlwaysShowActionPref(bool always_show) {
    MediaRouterActionController::SetAlwaysShowActionPref(profile(),
                                                         always_show);
  }

 protected:
  std::unique_ptr<MediaRouterActionController> controller_;
  std::unique_ptr<media_router::MockMediaRouter> router_;
  FakeCastToolbarIcon icon_;

  const media_router::Issue issue_;

  // Fake Sources, used for the Routes.
  const media_router::MediaSource source1_{"fakeSource1"};
  const media_router::MediaSource source2_{"fakeSource2"};

  std::vector<media_router::MediaRoute> local_display_route_list_;
  std::vector<media_router::MediaRoute> non_local_display_route_list_;
};

// TODO(b/185139027): Remove this class once
// |media_router::kGlobalMediaControlsCastStartStop| is enabled by default.
class MediaRouterActionControllerGMCUnitTest
    : public MediaRouterActionControllerUnitTest {
 public:
  MediaRouterActionControllerGMCUnitTest()
      : cast_source_("cast:1234"),
        mirroring_source_("urn:x-org.chromium.media:source:tab:*") {}
  void SetUp() override {
    MediaRouterActionControllerUnitTest::SetUp();
    feature_list_.InitAndEnableFeature(
        media_router::kGlobalMediaControlsCastStartStop);

    local_display_cast_route_list_.emplace_back("routeId4", cast_source_,
                                                "sinkId4", "description", true);
    local_display_mirroring_route_list_.emplace_back(
        "routeId5", mirroring_source_, "sinkId5", "description", true);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  const media_router::MediaSource cast_source_;
  const media_router::MediaSource mirroring_source_;
  std::vector<media_router::MediaRoute> local_display_cast_route_list_;
  std::vector<media_router::MediaRoute> local_display_mirroring_route_list_;
};

TEST_F(MediaRouterActionControllerUnitTest, EphemeralIconForRoutesAndIssues) {
  EXPECT_FALSE(IsIconShown());

  // Creating a local route should show the action icon.
  controller_->OnRoutesUpdated(local_display_route_list_);
  EXPECT_TRUE(controller_->has_local_display_route_);
  EXPECT_TRUE(IsIconShown());
  // Removing the local route should hide the icon.
  controller_->OnRoutesUpdated(non_local_display_route_list_);
  EXPECT_FALSE(controller_->has_local_display_route_);
  EXPECT_FALSE(IsIconShown());

  // Creating an issue should show the action icon.
  controller_->OnIssue(issue_);
  EXPECT_TRUE(controller_->has_issue_);
  EXPECT_TRUE(IsIconShown());
  // Removing the issue should hide the icon.
  controller_->OnIssuesCleared();
  EXPECT_FALSE(controller_->has_issue_);
  EXPECT_FALSE(IsIconShown());

  controller_->OnIssue(issue_);
  controller_->OnRoutesUpdated(local_display_route_list_);
  controller_->OnIssuesCleared();
  // When the issue disappears, the icon should remain visible if there's
  // a local route.
  EXPECT_TRUE(IsIconShown());
  controller_->OnRoutesUpdated(std::vector<media_router::MediaRoute>());
  EXPECT_FALSE(IsIconShown());
}

TEST_F(MediaRouterActionControllerUnitTest, EphemeralIconForDialog) {
  EXPECT_FALSE(IsIconShown());

  // Showing a dialog should show the icon.
  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  // Showing and hiding a dialog shouldn't hide the icon as long as we have a
  // positive number of dialogs.
  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller_->OnDialogHidden();
  EXPECT_TRUE(IsIconShown());
  // When we have zero dialogs, the icon should be hidden.
  controller_->OnDialogHidden();
  EXPECT_FALSE(IsIconShown());

  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller_->OnRoutesUpdated(local_display_route_list_);
  // Hiding the dialog while there are local routes shouldn't hide the icon.
  controller_->OnDialogHidden();
  EXPECT_TRUE(IsIconShown());
  controller_->OnRoutesUpdated(non_local_display_route_list_);
  EXPECT_FALSE(IsIconShown());

  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller_->OnIssue(issue_);
  // Hiding the dialog while there is an issue shouldn't hide the icon.
  controller_->OnDialogHidden();
  EXPECT_TRUE(IsIconShown());
  controller_->OnIssuesCleared();
  EXPECT_FALSE(IsIconShown());
}

TEST_F(MediaRouterActionControllerUnitTest, EphemeralIconForContextMenu) {
  EXPECT_FALSE(IsIconShown());

  controller_->OnDialogShown();
  EXPECT_TRUE(IsIconShown());
  controller_->OnDialogHidden();
  controller_->OnContextMenuShown();
  // Hiding the dialog immediately before showing a context menu shouldn't hide
  // the icon.
  EXPECT_TRUE(IsIconShown());

  // Hiding the context menu should hide the icon.
  controller_->OnContextMenuHidden();
  EXPECT_FALSE(IsIconShown());
}

TEST_F(MediaRouterActionControllerUnitTest, ObserveAlwaysShowPrefChange) {
  EXPECT_FALSE(IsIconShown());

  SetAlwaysShowActionPref(true);
  EXPECT_TRUE(IsIconShown());

  controller_->OnRoutesUpdated(local_display_route_list_);
  SetAlwaysShowActionPref(false);
  // Unchecking the option while having a local route shouldn't hide the icon.
  EXPECT_TRUE(IsIconShown());

  SetAlwaysShowActionPref(true);
  controller_->OnRoutesUpdated(non_local_display_route_list_);
  // Removing the local route should not hide the icon.
  EXPECT_TRUE(IsIconShown());

  SetAlwaysShowActionPref(false);
  EXPECT_FALSE(IsIconShown());
}

TEST_F(MediaRouterActionControllerGMCUnitTest,
       EphemeralIconForMirroringSessions) {
  EXPECT_FALSE(IsIconShown());
  // Creating a cast route should not show the action icon.
  controller_->OnRoutesUpdated({local_display_cast_route_list_});
  EXPECT_FALSE(IsIconShown());

  // Creating a local mirroring route should show the action icon.
  controller_->OnRoutesUpdated(local_display_mirroring_route_list_);
  EXPECT_TRUE(IsIconShown());
}
