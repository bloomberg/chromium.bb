// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/close_desk_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

// Defines an observer to test DesksController notifications.
class TestObserver : public DesksController::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  const std::vector<const Desk*>& desks() const { return desks_; }

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override { desks_.emplace_back(desk); }
  void OnDeskRemoved(const Desk* desk) override { base::Erase(desks_, desk); }

 private:
  std::vector<const Desk*> desks_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class DesksTest : public AshTestBase {
 public:
  DesksTest() = default;
  ~DesksTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVirtualDesks);

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DesksTest);
};

TEST_F(DesksTest, DesksCreationAndRemoval) {
  TestObserver observer;
  auto* controller = DesksController::Get();
  controller->AddObserver(&observer);

  // There's always a default pre-existing desk that cannot be removed.
  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());

  // Add desks until no longer possible.
  while (controller->CanCreateDesks())
    controller->NewDesk();

  // Expect we've reached the max number of desks, and we've been notified only
  // with the newly created desks.
  EXPECT_EQ(DesksController::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(DesksController::kMaxNumberOfDesks - 1, observer.desks().size());
  EXPECT_TRUE(controller->CanRemoveDesks());

  // Remove all desks until no longer possible, and expect that there's always
  // one default desk remaining.
  while (controller->CanRemoveDesks())
    controller->RemoveDesk(observer.desks().back());

  EXPECT_EQ(1u, controller->desks().size());
  EXPECT_FALSE(controller->CanRemoveDesks());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(observer.desks().empty());

  controller->RemoveObserver(&observer);
}

TEST_F(DesksTest, DesksBarViewDeskCreation) {
  TestObserver observer;
  auto* controller = DesksController::Get();

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());

  const auto* overview_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          Shell::GetPrimaryRootWindow());

  // Initially the grid is not offset down when there are no desk mini_views
  // once animations are added.
  EXPECT_FALSE(overview_grid->IsDesksBarViewActive());

  const auto* desks_bar_view = overview_grid->GetDesksBarViewForTesting();

  // Since we have a single default desk, there should be no mini_views, and the
  // new desk button is enabled.
  DCHECK(desks_bar_view);
  EXPECT_TRUE(desks_bar_view->mini_views().empty());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->enabled());

  // Click many times on the new desk button and expect only the max number of
  // desks will be created, and the button is no longer enabled.
  const gfx::Point button_center =
      desks_bar_view->new_desk_button()->GetBoundsInScreen().CenterPoint();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(button_center);
  for (size_t i = 0; i < DesksController::kMaxNumberOfDesks + 2; ++i)
    event_generator->ClickLeftButton();

  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  EXPECT_EQ(DesksController::kMaxNumberOfDesks, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_FALSE(controller->CanCreateDesks());
  EXPECT_TRUE(controller->CanRemoveDesks());
  EXPECT_FALSE(desks_bar_view->new_desk_button()->enabled());

  // Hover over one of the mini_views, and expect that the close button becomes
  // visible.
  const auto* mini_view = desks_bar_view->mini_views().back().get();
  EXPECT_FALSE(mini_view->close_desk_button()->visible());
  const gfx::Point mini_view_center =
      mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  EXPECT_TRUE(mini_view->close_desk_button()->visible());

  // Use the close button to close the desk.
  event_generator->MoveMouseTo(
      mini_view->close_desk_button()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  // The new desk button is now enabled again.
  EXPECT_EQ(DesksController::kMaxNumberOfDesks - 1, controller->desks().size());
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(controller->CanCreateDesks());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->enabled());

  // Exit overview mode and re-enter. Since we have more than one pre-existing
  // desks, their mini_views should be created upon construction of the desks
  // bar.
  overview_controller->ToggleOverview();
  EXPECT_FALSE(overview_controller->IsSelecting());
  overview_controller->ToggleOverview();
  EXPECT_TRUE(overview_controller->IsSelecting());

  // Get the new grid and the new desk_bar_view.
  overview_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid->IsDesksBarViewActive());
  desks_bar_view = overview_grid->GetDesksBarViewForTesting();

  DCHECK(desks_bar_view);
  EXPECT_EQ(controller->desks().size(), desks_bar_view->mini_views().size());
  EXPECT_TRUE(desks_bar_view->new_desk_button()->enabled());
}

}  // namespace

}  // namespace ash
