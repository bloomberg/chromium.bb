// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pods_container_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view_observer.h"

namespace ash {

class FeaturePodsContainerViewTest : public NoSessionAshTestBase,
                                     public FeaturePodControllerBase,
                                     public views::ViewObserver {
 public:
  FeaturePodsContainerViewTest() = default;
  ~FeaturePodsContainerViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    model_ = std::make_unique<UnifiedSystemTrayModel>();
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
    container_ = std::make_unique<FeaturePodsContainerView>(
        controller_.get(), true /* initially_expanded */);
    container_->AddObserver(this);
    preferred_size_changed_count_ = 0;

    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  }

  void TearDown() override {
    controller_.reset();
    container_.reset();
    model_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override { return nullptr; }
  void OnIconPressed() override {}
  SystemTrayItemUmaType GetUmaType() const override {
    return SystemTrayItemUmaType::UMA_TEST;
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    ++preferred_size_changed_count_;
  }

 protected:
  void EnablePagination() {
    scoped_feature_list_->InitAndEnableFeature(
        features::kSystemTrayFeaturePodsPagination);
  }

  void AddButtons(int count) {
    for (int i = 0; i < count; ++i) {
      buttons_.push_back(new FeaturePodButton(this));
      container()->AddChildView(buttons_.back());
    }
    container()->SetBoundsRect(gfx::Rect(container_->GetPreferredSize()));
    container()->Layout();
  }

  FeaturePodsContainerView* container() { return container_.get(); }

  PaginationModel* pagination_model() { return model_->pagination_model(); }

  int preferred_size_changed_count() const {
    return preferred_size_changed_count_;
  }

  std::vector<FeaturePodButton*> buttons_;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<FeaturePodsContainerView> container_;
  std::unique_ptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  int preferred_size_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FeaturePodsContainerViewTest);
};

TEST_F(FeaturePodsContainerViewTest, ExpandedAndCollapsed) {
  const int kNumberOfAddedButtons = kUnifiedFeaturePodItemsInRow * 3;
  EXPECT_LT(kUnifiedFeaturePodMaxItemsInCollapsed, kNumberOfAddedButtons);

  AddButtons(kNumberOfAddedButtons);

  // In expanded state, buttons are laid out in plane.
  EXPECT_LT(buttons_[0]->x(), buttons_[1]->x());
  EXPECT_EQ(buttons_[0]->y(), buttons_[1]->y());
  // If the row exceeds kUnifiedFeaturePodItemsInRow, the next button is placed
  // right under the first button.
  EXPECT_EQ(buttons_[0]->x(), buttons_[kUnifiedFeaturePodItemsInRow]->x());
  EXPECT_LT(buttons_[0]->y(), buttons_[kUnifiedFeaturePodItemsInRow]->y());
  // All buttons are visible.
  for (auto* button : buttons_)
    EXPECT_TRUE(button->GetVisible());

  container()->SetExpandedAmount(0.0);

  // In collapsed state, all buttons are laid out horizontally.
  for (int i = 1; i < kUnifiedFeaturePodMaxItemsInCollapsed; ++i)
    EXPECT_EQ(buttons_[0]->y(), buttons_[i]->y());

  // Buttons exceed kUnifiedFeaturePodMaxItemsInCollapsed are invisible.
  for (int i = 0; i < kNumberOfAddedButtons; ++i) {
    EXPECT_EQ(i < kUnifiedFeaturePodMaxItemsInCollapsed,
              buttons_[i]->GetVisible());
  }
}

TEST_F(FeaturePodsContainerViewTest, HiddenButtonRemainsHidden) {
  AddButtons(kUnifiedFeaturePodMaxItemsInCollapsed);
  // The button is invisible in expanded state.
  buttons_.front()->SetVisible(false);
  container()->SetExpandedAmount(0.0);
  EXPECT_FALSE(buttons_.front()->GetVisible());
  container()->SetExpandedAmount(1.0);
  EXPECT_FALSE(buttons_.front()->GetVisible());
}

TEST_F(FeaturePodsContainerViewTest, BecomeVisibleInCollapsed) {
  AddButtons(kUnifiedFeaturePodMaxItemsInCollapsed);
  // The button is invisible in expanded state.
  buttons_.back()->SetVisible(false);
  container()->SetExpandedAmount(0.0);
  // The button becomes visible in collapsed state.
  buttons_.back()->SetVisible(true);
  // As the container still has remaining space, the button will be visible.
  EXPECT_TRUE(buttons_.back()->GetVisible());
}

TEST_F(FeaturePodsContainerViewTest, StillHiddenInCollapsed) {
  AddButtons(kUnifiedFeaturePodMaxItemsInCollapsed + 1);
  // The button is invisible in expanded state.
  buttons_.back()->SetVisible(false);
  container()->SetExpandedAmount(0.0);
  // The button becomes visible in collapsed state.
  buttons_.back()->SetVisible(true);
  // As the container doesn't have remaining space, the button won't be visible.
  EXPECT_FALSE(buttons_.back()->GetVisible());

  container()->SetExpandedAmount(1.0);
  // The button becomes visible in expanded state.
  EXPECT_TRUE(buttons_.back()->GetVisible());
}

TEST_F(FeaturePodsContainerViewTest, DifferentButtonBecomeVisibleInCollapsed) {
  AddButtons(kUnifiedFeaturePodMaxItemsInCollapsed + 1);
  container()->SetExpandedAmount(0.0);
  // The last button is not visible as it doesn't have enough space.
  EXPECT_FALSE(buttons_.back()->GetVisible());
  // The first button becomes invisible.
  buttons_.front()->SetVisible(false);
  // The last button now has the space for it.
  EXPECT_TRUE(buttons_.back()->GetVisible());
}

TEST_F(FeaturePodsContainerViewTest, SizeChangeByExpanding) {
  // SetExpandedAmount() should not trigger PreferredSizeChanged().
  AddButtons(kUnifiedFeaturePodItemsInRow * 3 - 1);
  EXPECT_EQ(0, preferred_size_changed_count());
  container()->SetExpandedAmount(0.0);
  container()->SetExpandedAmount(0.5);
  container()->SetExpandedAmount(1.0);
  EXPECT_EQ(0, preferred_size_changed_count());
}

TEST_F(FeaturePodsContainerViewTest, SizeChangeByVisibility) {
  // Visibility change should trigger PreferredSizeChanged().
  AddButtons(kUnifiedFeaturePodItemsInRow * 2 + 1);
  EXPECT_EQ(0, preferred_size_changed_count());
  // The first button becomes invisible.
  buttons_.front()->SetVisible(false);
  EXPECT_EQ(1, preferred_size_changed_count());
  // The first button becomes visible.
  buttons_.front()->SetVisible(true);
  EXPECT_EQ(2, preferred_size_changed_count());
}

TEST_F(FeaturePodsContainerViewTest, NumberOfPagesChanged) {
  const int kNumberOfPages = 8;

  EnablePagination();
  AddButtons(kUnifiedFeaturePodItemsInRow * kUnifiedFeaturePodItemsRows *
             kNumberOfPages);

  // Adding buttons to fill kNumberOfPages should cause the the same number of
  // pages to be created.
  EXPECT_EQ(kNumberOfPages, pagination_model()->total_pages());

  // Adding an additional button causes a new page to be added.
  AddButtons(1);
  EXPECT_EQ(pagination_model()->total_pages(), kNumberOfPages + 1);
}

TEST_F(FeaturePodsContainerViewTest, PaginationTransition) {
  const int kNumberOfPages = 8;

  EnablePagination();
  AddButtons(kUnifiedFeaturePodItemsInRow * kUnifiedFeaturePodItemsRows *
             kNumberOfPages);

  // Position of a button should slide to the left during a page
  // transition to the next page.
  gfx::Rect current_bounds;
  gfx::Rect initial_bounds = buttons_[0]->bounds();
  gfx::Rect previous_bounds = initial_bounds;

  PaginationModel::Transition transition(
      pagination_model()->selected_page() + 1, 0);

  for (double i = 0.1; i <= 1.0; i += 0.1) {
    transition.progress = i;
    pagination_model()->SetTransition(transition);

    current_bounds = buttons_[0]->bounds();

    EXPECT_LT(current_bounds.x(), previous_bounds.x());
    EXPECT_EQ(current_bounds.y(), previous_bounds.y());

    previous_bounds = current_bounds;
  }

  // Button Position after page switch should move to the left by a page offset.
  int page_offset = container()->CalculatePreferredSize().width() +
                    kUnifiedFeaturePodsPageSpacing;
  gfx::Rect final_bounds =
      gfx::Rect(initial_bounds.x() - page_offset, initial_bounds.y(),
                initial_bounds.width(), initial_bounds.height());
  pagination_model()->SelectPage(1, false);
  container()->Layout();
  EXPECT_EQ(final_bounds, buttons_[0]->bounds());
}

}  // namespace ash
