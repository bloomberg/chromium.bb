// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"

#include <utility>

#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/theme_resources.h"
#include "ui/gfx/color_palette.h"

FakeBaseTabStripController::FakeBaseTabStripController() {}

FakeBaseTabStripController::~FakeBaseTabStripController() {
}

void FakeBaseTabStripController::AddTab(int index, bool is_active) {
  num_tabs_++;
  tab_strip_->AddTabAt(index, TabRendererData(), is_active);
  ui::MouseEvent fake_event =
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                     base::TimeTicks::Now(), 0, 0);
  if (is_active)
    SelectTab(index, fake_event);
}

void FakeBaseTabStripController::AddPinnedTab(int index, bool is_active) {
  TabRendererData data;
  data.pinned = true;
  num_tabs_++;
  tab_strip_->AddTabAt(index, std::move(data), is_active);
  if (is_active)
    active_index_ = index;
}

void FakeBaseTabStripController::RemoveTab(int index) {
  num_tabs_--;
  // RemoveTabAt() expects the controller state to have been updated already.
  const bool was_active = index == active_index_;
  if (active_index_ > index) {
    --active_index_;
  } else if (active_index_ == index) {
    SetActiveIndex(std::min(active_index_, num_tabs_ - 1));
  }
  tab_strip_->RemoveTabAt(nullptr, index, was_active);
}

int FakeBaseTabStripController::CreateTabGroup() {
  ++num_groups_;
  return num_groups_ - 1;
}

void FakeBaseTabStripController::MoveTabIntoGroup(
    int index,
    base::Optional<int> new_group) {
  auto tab_group_pair = tab_to_group_.find(index);
  base::Optional<int> old_group =
      tab_group_pair != tab_to_group_.end()
          ? base::make_optional(tab_group_pair->second)
          : base::nullopt;
  if (new_group.has_value())
    tab_to_group_[index] = new_group.value();
  else
    tab_to_group_.erase(index);
  tab_strip_->ChangeTabGroup(index, old_group, new_group);
}

const TabGroupData* FakeBaseTabStripController::GetDataForGroup(
    int group) const {
  return &fake_group_data_;
}

std::vector<int> FakeBaseTabStripController::ListTabsInGroup(int group) const {
  std::vector<int> result;
  for (auto const& tab_group_pair : tab_to_group_) {
    if (tab_group_pair.second == group)
      result.push_back(tab_group_pair.first);
  }
  return result;
}

const ui::ListSelectionModel&
FakeBaseTabStripController::GetSelectionModel() const {
  return selection_model_;
}

int FakeBaseTabStripController::GetCount() const {
  return num_tabs_;
}

bool FakeBaseTabStripController::IsValidIndex(int index) const {
  return index >= 0 && index < num_tabs_;
}

bool FakeBaseTabStripController::IsActiveTab(int index) const {
  if (!IsValidIndex(index))
    return false;
  return active_index_ == index;
}

int FakeBaseTabStripController::GetActiveIndex() const {
  return active_index_;
}

bool FakeBaseTabStripController::IsTabSelected(int index) const {
  return false;
}

bool FakeBaseTabStripController::IsTabPinned(int index) const {
  return false;
}

void FakeBaseTabStripController::SelectTab(int index, const ui::Event& event) {
  if (!IsValidIndex(index) || active_index_ == index)
    return;

  SetActiveIndex(index);
}

void FakeBaseTabStripController::ExtendSelectionTo(int index) {
}

void FakeBaseTabStripController::ToggleSelected(int index) {
}

void FakeBaseTabStripController::AddSelectionFromAnchorTo(int index) {
}

bool FakeBaseTabStripController::BeforeCloseTab(int index,
                                                CloseTabSource source) {
  return true;
}

void FakeBaseTabStripController::CloseTab(int index, CloseTabSource source) {
  RemoveTab(index);
}

void FakeBaseTabStripController::ShowContextMenuForTab(
    Tab* tab,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
}

int FakeBaseTabStripController::HasAvailableDragActions() const {
  return 0;
}

void FakeBaseTabStripController::OnDropIndexUpdate(int index,
                                                   bool drop_before) {
}

bool FakeBaseTabStripController::IsCompatibleWith(TabStrip* other) const {
  return false;
}

void FakeBaseTabStripController::CreateNewTab() {
  AddTab(num_tabs_, true);
}

void FakeBaseTabStripController::CreateNewTabWithLocation(
    const base::string16& location) {
}

void FakeBaseTabStripController::StackedLayoutMaybeChanged() {
}

void FakeBaseTabStripController::OnStartedDraggingTabs() {}

void FakeBaseTabStripController::OnStoppedDraggingTabs() {}

bool FakeBaseTabStripController::IsFrameCondensed() const {
  return false;
}

bool FakeBaseTabStripController::HasVisibleBackgroundTabShapes() const {
  return false;
}

bool FakeBaseTabStripController::EverHasVisibleBackgroundTabShapes() const {
  return false;
}

bool FakeBaseTabStripController::ShouldPaintAsActiveFrame() const {
  return true;
}

bool FakeBaseTabStripController::CanDrawStrokes() const {
  return false;
}

SkColor FakeBaseTabStripController::GetFrameColor(
    BrowserNonClientFrameView::ActiveState active_state) const {
  return gfx::kPlaceholderColor;
}

SkColor FakeBaseTabStripController::GetToolbarTopSeparatorColor() const {
  return gfx::kPlaceholderColor;
}

int FakeBaseTabStripController::GetTabBackgroundResourceId(
    BrowserNonClientFrameView::ActiveState active_state,
    bool* has_custom_image) const {
  *has_custom_image = false;
  return IDR_THEME_TAB_BACKGROUND;
}

base::string16 FakeBaseTabStripController::GetAccessibleTabName(
    const Tab* tab) const {
  return base::string16();
}

Profile* FakeBaseTabStripController::GetProfile() const {
  return nullptr;
}

void FakeBaseTabStripController::SetActiveIndex(int new_index) {
  active_index_ = new_index;
  selection_model_.SetSelectedIndex(active_index_);
  if (IsValidIndex(active_index_))
    tab_strip_->SetSelection(selection_model_);
}
