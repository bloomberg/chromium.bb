// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_pods_container_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/feature_pod_button.h"

namespace ash {

FeaturePodsContainerView::FeaturePodsContainerView(bool initially_expanded)
    : expanded_amount_(initially_expanded ? 1.0 : 0.0) {}

FeaturePodsContainerView::~FeaturePodsContainerView() = default;

void FeaturePodsContainerView::SetExpandedAmount(double expanded_amount) {
  DCHECK(0.0 <= expanded_amount && expanded_amount <= 1.0);
  if (expanded_amount_ == expanded_amount)
    return;
  expanded_amount_ = expanded_amount;

  for (auto* view : children())
    static_cast<FeaturePodButton*>(view)->SetExpandedAmount(expanded_amount_);
  UpdateChildVisibility();
  // We have to call Layout() explicitly here.
  Layout();
}

int FeaturePodsContainerView::GetExpandedHeight() const {
  const int visible_count = GetVisibleCount();

  // floor(visible_count / kUnifiedFeaturePodItemsInRow)
  int number_of_lines = (visible_count + kUnifiedFeaturePodItemsInRow - 1) /
                        kUnifiedFeaturePodItemsInRow;
  return kUnifiedFeaturePodBottomPadding +
         (kUnifiedFeaturePodVerticalPadding + kUnifiedFeaturePodSize.height()) *
             number_of_lines;
}

int FeaturePodsContainerView::GetCollapsedHeight() const {
  return 2 * kUnifiedFeaturePodCollapsedVerticalPadding +
         kUnifiedFeaturePodCollapsedSize.height();
}

void FeaturePodsContainerView::SaveFocus() {
  const auto i = std::find_if(children().cbegin(), children().cend(),
                              [](const auto* v) { return v->HasFocus(); });
  focused_button_ = (i == children().cend()) ? nullptr : *i;
}

void FeaturePodsContainerView::RestoreFocus() {
  if (focused_button_)
    focused_button_->RequestFocus();
}

gfx::Size FeaturePodsContainerView::CalculatePreferredSize() const {
  return gfx::Size(
      kTrayMenuWidth,
      static_cast<int>(GetCollapsedHeight() * (1.0 - expanded_amount_) +
                       GetExpandedHeight() * expanded_amount_));
}

void FeaturePodsContainerView::ChildVisibilityChanged(View* child) {
  // ChildVisibilityChanged can change child visibility using
  // SetVisibleByContainer() in UpdateChildVisibility(), so we have to prevent
  // reentrancy.
  if (changing_visibility_)
    return;

  // Visibility change is caused by the child's SetVisible(), so update actual
  // visibility and propagate the container size change to the parent.
  UpdateChildVisibility();
  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void FeaturePodsContainerView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  UpdateChildVisibility();
}

void FeaturePodsContainerView::Layout() {
  UpdateCollapsedSidePadding();

  int visible_count = 0;
  for (auto* child : children()) {
    if (!child->visible())
      continue;

    gfx::Size child_size;
    if (expanded_amount_ > 0.0) {
      child_size = kUnifiedFeaturePodSize;

      // Flexibly give more height if the child view doesn't fit into the
      // default height, so that label texts won't be broken up in the middle.
      child_size.set_height(std::max(
          child_size.height(), child->GetHeightForWidth(child_size.height())));
    } else {
      child_size = kUnifiedFeaturePodCollapsedSize;
    }

    child->SetBoundsRect(
        gfx::Rect(GetButtonPosition(visible_count++), child_size));
    child->Layout();
  }
}

void FeaturePodsContainerView::UpdateChildVisibility() {
  DCHECK(!changing_visibility_);
  changing_visibility_ = true;

  int visible_count = 0;
  for (auto* view : children()) {
    auto* child = static_cast<FeaturePodButton*>(view);
    bool visible = child->visible_preferred() &&
                   (expanded_amount_ > 0.0 ||
                    visible_count < kUnifiedFeaturePodMaxItemsInCollapsed);
    child->SetVisibleByContainer(visible);
    if (visible)
      ++visible_count;
  }

  changing_visibility_ = false;
}

int FeaturePodsContainerView::GetVisibleCount() const {
  return std::count_if(
      children().cbegin(), children().cend(), [](const auto* v) {
        return static_cast<const FeaturePodButton*>(v)->visible_preferred();
      });
}

gfx::Point FeaturePodsContainerView::GetButtonPosition(
    int visible_index) const {
  int row = visible_index / kUnifiedFeaturePodItemsInRow;
  int column = visible_index % kUnifiedFeaturePodItemsInRow;
  int x = kUnifiedFeaturePodHorizontalSidePadding +
          (kUnifiedFeaturePodSize.width() +
           kUnifiedFeaturePodHorizontalMiddlePadding) *
              column;
  int y = kUnifiedFeaturePodTopPadding + (kUnifiedFeaturePodSize.height() +
                                          kUnifiedFeaturePodVerticalPadding) *
                                             row;

  // When fully expanded, or below the second row, always return the same
  // position.
  if (expanded_amount_ == 1.0 || row > 2)
    return gfx::Point(x, y);

  int collapsed_x =
      collapsed_side_padding_ + (kUnifiedFeaturePodCollapsedSize.width() +
                                 kUnifiedFeaturePodCollapsedHorizontalPadding) *
                                    visible_index;
  int collapsed_y = kUnifiedFeaturePodCollapsedVerticalPadding;

  // When fully collapsed, just return the collapsed position.
  if (expanded_amount_ == 0.0)
    return gfx::Point(collapsed_x, collapsed_y);

  // Button width is different between expanded and collapsed states.
  // During the transition, expanded width is used, so it should be adjusted.
  collapsed_x -= (kUnifiedFeaturePodSize.width() -
                  kUnifiedFeaturePodCollapsedSize.width()) /
                 2;

  return gfx::Point(
      x * expanded_amount_ + collapsed_x * (1.0 - expanded_amount_),
      y * expanded_amount_ + collapsed_y * (1.0 - expanded_amount_));
}

void FeaturePodsContainerView::UpdateCollapsedSidePadding() {
  const int visible_count =
      std::min(GetVisibleCount(), kUnifiedFeaturePodMaxItemsInCollapsed);

  int contents_width =
      visible_count * kUnifiedFeaturePodCollapsedSize.width() +
      (visible_count - 1) * kUnifiedFeaturePodCollapsedHorizontalPadding;

  collapsed_side_padding_ = (kTrayMenuWidth - contents_width) / 2;
  DCHECK(collapsed_side_padding_ > 0);
}

}  // namespace ash
