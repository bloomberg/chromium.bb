// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/paged_view_structure.h"

#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/views/view_model.h"

namespace app_list {

PagedViewStructure::PagedViewStructure(AppsGridView* apps_grid_view)
    : apps_grid_view_(apps_grid_view) {}

PagedViewStructure::PagedViewStructure(const PagedViewStructure& other) =
    default;

PagedViewStructure::~PagedViewStructure() = default;

void PagedViewStructure::LoadFromMetadata() {
  // TODO(weidongg): Change the fake loading here and implement loading view
  // structure from position and page position in metadata.
  auto* view_model = apps_grid_view_->view_model();
  pages_.clear();
  pages_.emplace_back();
  auto& page = pages_[0];
  for (int i = 0; i < view_model->view_size(); ++i) {
    page.emplace_back(view_model->view_at(i));
  }
  Sanitize();
}

void PagedViewStructure::SaveToMetadata() {
  // TODO(weidongg): Implement saving page position change of each item into
  // metadata.
}

bool PagedViewStructure::Sanitize() {
  bool changed = false;
  std::vector<AppListItemView*> overflow_views;
  auto iter = pages_.begin();
  while (iter != pages_.end() || !overflow_views.empty()) {
    if (iter == pages_.end()) {
      // Add additional page if overflowing item views remain.
      pages_.emplace_back();
      iter = pages_.end() - 1;
      changed = true;
    }

    const size_t max_item_views =
        apps_grid_view_->TilesPerPage(static_cast<int>(iter - pages_.begin()));
    auto& page = *iter;

    if (!overflow_views.empty()) {
      // Put overflowing item views in current page.
      page.insert(page.begin(), overflow_views.begin(), overflow_views.end());
      overflow_views.clear();
      changed = true;
    }

    if (page.empty()) {
      // Remove empty page.
      iter = pages_.erase(iter);
      changed = true;
      continue;
    }

    if (page.size() > max_item_views) {
      // Remove overflowing item views from current page.
      overflow_views.insert(overflow_views.begin(),
                            page.begin() + max_item_views, page.end());
      page.erase(page.begin() + max_item_views, page.end());
      changed = true;
    }

    ++iter;
  }
  return changed;
}

void PagedViewStructure::Move(AppListItemView* view,
                              const GridIndex& target_index) {
  RemoveWithoutSanitize(view);
  Add(view, target_index);
}

void PagedViewStructure::Remove(AppListItemView* view) {
  RemoveWithoutSanitize(view);
  Sanitize();
}

void PagedViewStructure::RemoveWithoutSanitize(AppListItemView* view) {
  for (auto& page : pages_) {
    auto iter = std::find(page.begin(), page.end(), view);
    if (iter != page.end()) {
      page.erase(iter);
      break;
    }
  }
}

void PagedViewStructure::Add(AppListItemView* view,
                             const GridIndex& target_index) {
  AddWithoutSanitize(view, target_index);
  Sanitize();
}

void PagedViewStructure::AddWithoutSanitize(AppListItemView* view,
                                            const GridIndex& target_index) {
  const int view_structure_size = total_pages();
  DCHECK((target_index.page < view_structure_size &&
          target_index.slot <= items_on_page(target_index.page)) ||
         (target_index.page == view_structure_size && target_index.slot == 0));

  if (target_index.page == view_structure_size)
    pages_.emplace_back();

  auto& page = pages_[target_index.page];
  page.insert(page.begin() + target_index.slot, view);
}

GridIndex PagedViewStructure::GetIndexFromModelIndex(int model_index) const {
  AppListItemView* view = apps_grid_view_->view_model()->view_at(model_index);
  for (size_t i = 0; i < pages_.size(); ++i) {
    auto& page = pages_[i];
    for (size_t j = 0; j < page.size(); ++j) {
      if (page[j] == view)
        return GridIndex(i, j);
    }
  }
  return GetLastTargetIndex();
}

int PagedViewStructure::GetModelIndexFromIndex(const GridIndex& index) const {
  auto* view_model = apps_grid_view_->view_model();
  if (index.page >= total_pages() || index.slot >= items_on_page(index.page))
    return view_model->view_size();

  AppListItemView* view = pages_[index.page][index.slot];
  return view_model->GetIndexOfView(view);
}

GridIndex PagedViewStructure::GetLastTargetIndex() const {
  if (apps_grid_view_->view_model()->view_size() == 0)
    return GridIndex(0, 0);

  int last_page_index = total_pages() - 1;
  int target_slot = 0;
  auto& last_page = pages_.back();
  for (size_t i = 0; i < last_page.size(); ++i) {
    // Skip the item view being dragged if it exists in the last page.
    if (last_page[i] != apps_grid_view_->drag_view_)
      ++target_slot;
  }

  if (target_slot == apps_grid_view_->TilesPerPage(last_page_index)) {
    // The last page is full, so the last target visual index is the first slot
    // in the next new page.
    target_slot = 0;
    ++last_page_index;
  }
  return GridIndex(last_page_index, target_slot);
}

GridIndex PagedViewStructure::GetLastTargetIndexOfPage(int page_index) const {
  const int page_size = total_pages();
  DCHECK_LT(0, apps_grid_view_->view_model()->view_size());
  DCHECK_LE(page_index, page_size);

  if (page_index == page_size)
    return GridIndex(page_index, 0);

  int target_slot = 0;
  auto& page = pages_[page_index];
  for (size_t i = 0; i < page.size(); ++i) {
    // Skip the item view being dragged if it exists in the specified
    // page_index.
    if (page[i] != apps_grid_view_->drag_view())
      ++target_slot;
  }

  if (target_slot == apps_grid_view_->TilesPerPage(page_index)) {
    // The specified page is full, so the last target visual index is the last
    // slot in the page_index.
    --target_slot;
  }
  return GridIndex(page_index, target_slot);
}

int PagedViewStructure::GetTargetModelIndexForMove(
    AppListItemView* moved_view,
    const GridIndex& index) const {
  int target_model_index = 0;
  const int max_page = std::min(index.page, total_pages());
  for (int i = 0; i < max_page; ++i) {
    auto& page = pages_[i];
    target_model_index += page.size();

    // Skip the item view to be moved in the page if found.
    auto iter = std::find(page.begin(), page.end(), moved_view);
    if (iter != page.end())
      --target_model_index;
  }

  // If the target visual index is in the same page, do not skip the item view
  // because the following item views will fill the gap in the page.
  target_model_index += index.slot;
  return target_model_index;
}

bool PagedViewStructure::IsValidReorderTargetIndex(
    const GridIndex& index) const {
  if (apps_grid_view_->IsValidIndex(index))
    return true;

  // The user can drag an item view to another page's end.
  if (index.page <= total_pages() &&
      GetLastTargetIndexOfPage(index.page) == index) {
    return true;
  }

  return false;
}

}  // namespace app_list
