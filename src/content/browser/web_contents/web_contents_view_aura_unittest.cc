// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

namespace {
constexpr gfx::Rect kBounds = gfx::Rect(0, 0, 20, 20);
}  // namespace

class WebContentsViewAuraTest : public RenderViewHostTestHarness {
 protected:
  WebContentsViewAuraTest() = default;
  ~WebContentsViewAuraTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    root_window()->SetBounds(kBounds);
    GetNativeView()->SetBounds(kBounds);
    GetNativeView()->Show();
    root_window()->AddChild(GetNativeView());

    occluding_window_.reset(aura::test::CreateTestWindowWithDelegateAndType(
        nullptr, aura::client::WINDOW_TYPE_NORMAL, 0, kBounds, root_window(),
        false));
  }

  void TearDown() override {
    occluding_window_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  WebContentsViewAura* view() {
    WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
    return static_cast<WebContentsViewAura*>(contents->GetView());
  }

  aura::Window* GetNativeView() { return web_contents()->GetNativeView(); }

  // |occluding_window_| occludes |web_contents()| when it's shown.
  std::unique_ptr<aura::Window> occluding_window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAuraTest);
};

TEST_F(WebContentsViewAuraTest, EnableDisableOverscroll) {
  WebContentsViewAura* wcva = view();
  wcva->SetOverscrollControllerEnabled(false);
  EXPECT_FALSE(wcva->navigation_overlay_);
  wcva->SetOverscrollControllerEnabled(true);
  EXPECT_TRUE(wcva->navigation_overlay_);
}

TEST_F(WebContentsViewAuraTest, ShowHideParent) {
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->Hide();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  root_window()->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
}

TEST_F(WebContentsViewAuraTest, OccludeView) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebContentsOcclusion);

  EXPECT_EQ(web_contents()->GetVisibility(), Visibility::VISIBLE);
  occluding_window_->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), Visibility::OCCLUDED);
  occluding_window_->Hide();
  EXPECT_EQ(web_contents()->GetVisibility(), Visibility::VISIBLE);
}

TEST_F(WebContentsViewAuraTest, MirroringEnabledForHiddenView) {
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->Hide();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  GetNativeView()->SetProperty(aura::client::kMirroringEnabledKey, true);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  GetNativeView()->SetProperty(aura::client::kMirroringEnabledKey, false);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  root_window()->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  GetNativeView()->SetProperty(aura::client::kMirroringEnabledKey, true);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  GetNativeView()->SetProperty(aura::client::kMirroringEnabledKey, false);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
}

TEST_F(WebContentsViewAuraTest, MirroringEnabledForOccludedView) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebContentsOcclusion);

  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  occluding_window_->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::OCCLUDED);
  GetNativeView()->SetProperty(aura::client::kMirroringEnabledKey, true);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  GetNativeView()->SetProperty(aura::client::kMirroringEnabledKey, false);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::OCCLUDED);
}

TEST_F(WebContentsViewAuraTest, MirroringEnabledForHiddenViewParent) {
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->Hide();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, true);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, false);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  root_window()->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, true);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, false);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
}

TEST_F(WebContentsViewAuraTest, MirroringEnabledForOccludedViewParent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebContentsOcclusion);

  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  occluding_window_->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::OCCLUDED);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, true);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, false);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::OCCLUDED);
}

TEST_F(WebContentsViewAuraTest, MirroringEnabledForHiddenViewHost) {
  // Make root_window() the host of GetNativeView(), and introduce an
  // |intermediate_window| in the hierarchy between root_window() and
  // GetNativeView().
  std::unique_ptr<aura::Window> intermediate_window(
      aura::test::CreateTestWindowWithDelegateAndType(
          nullptr, aura::client::WINDOW_TYPE_NORMAL, 0, kBounds, root_window(),
          false));
  GetNativeView()->SetProperty(aura::client::kHostWindowKey, root_window());
  intermediate_window->AddChild(GetNativeView());
  root_window()->StackChildAtBottom(intermediate_window.get());
  intermediate_window->Show();

  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->Hide();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, true);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, false);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::HIDDEN);
  root_window()->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, true);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, false);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
}

TEST_F(WebContentsViewAuraTest, MirroringEnabledForOccludedViewHost) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebContentsOcclusion);

  // Make root_window() the host of GetNativeView(), and introduce an
  // |intermediate_window| in the hierarchy between root_window() and
  // GetNativeView().
  std::unique_ptr<aura::Window> intermediate_window(
      aura::test::CreateTestWindowWithDelegateAndType(
          nullptr, aura::client::WINDOW_TYPE_NORMAL, 0, kBounds, root_window(),
          false));
  GetNativeView()->SetProperty(aura::client::kHostWindowKey, root_window());
  intermediate_window->AddChild(GetNativeView());
  root_window()->StackChildAtBottom(intermediate_window.get());
  intermediate_window->Show();

  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  occluding_window_->Show();
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::OCCLUDED);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, true);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  root_window()->SetProperty(aura::client::kMirroringEnabledKey, false);
  EXPECT_EQ(web_contents()->GetVisibility(), content::Visibility::OCCLUDED);
}

}  // namespace content
