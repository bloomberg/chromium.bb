// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/folder_image.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/folder_image_source.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"

namespace app_list {

FolderImage::FolderImage(AppListItemList* item_list) : item_list_(item_list) {
  item_list_->AddObserver(this);
}

FolderImage::~FolderImage() {
  for (auto* item : top_items_)
    item->RemoveObserver(this);
  item_list_->RemoveObserver(this);
}

void FolderImage::UpdateIcon() {
  for (auto* item : top_items_)
    item->RemoveObserver(this);
  top_items_.clear();

  for (size_t i = 0; i < kNumFolderTopItems && i < item_list_->item_count();
       ++i) {
    AppListItem* item = item_list_->item_at(i);
    item->AddObserver(this);
    top_items_.push_back(item);
  }
  RedrawIconAndNotify();
}

const gfx::ImageSkia& FolderImage::GetTopIcon(size_t item_index) {
  CHECK_LT(item_index, top_items_.size());
  return top_items_[item_index]->icon();
}

gfx::Rect FolderImage::GetTargetIconRectInFolderForItem(
    AppListItem* item,
    const gfx::Rect& folder_icon_bounds) {
  for (size_t i = 0; i < top_items_.size(); ++i) {
    if (item->id() == top_items_[i]->id()) {
      std::vector<gfx::Rect> rects =
          FolderImageSource::GetTopIconsBounds(folder_icon_bounds);
      return rects[i];
    }
  }

  gfx::Rect target_rect(folder_icon_bounds);
  target_rect.ClampToCenteredSize(FolderImageSource::ItemIconSize());
  return target_rect;
}

void FolderImage::AddObserver(FolderImageObserver* observer) {
  observers_.AddObserver(observer);
}

void FolderImage::RemoveObserver(FolderImageObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FolderImage::ItemIconChanged() {
  // Note: Must update the image only (cannot simply call UpdateIcon), because
  // UpdateIcon removes and re-adds the FolderImage as an observer of the
  // AppListItems, which causes the current iterator to call ItemIconChanged
  // again, and goes into an infinite loop.
  RedrawIconAndNotify();
}

void FolderImage::OnListItemAdded(size_t index, AppListItem* item) {
  if (index < kNumFolderTopItems)
    UpdateIcon();
}

void FolderImage::OnListItemRemoved(size_t index, AppListItem* item) {
  if (index < kNumFolderTopItems)
    UpdateIcon();
}

void FolderImage::OnListItemMoved(size_t from_index,
                                  size_t to_index,
                                  AppListItem* item) {
  if (from_index < kNumFolderTopItems || to_index < kNumFolderTopItems)
    UpdateIcon();
}

void FolderImage::RedrawIconAndNotify() {
  FolderImageSource::Icons top_icons;
  for (const auto* item : top_items_)
    top_icons.push_back(item->icon());

  const gfx::Size icon_size = gfx::Size(kGridIconDimension, kGridIconDimension);
  icon_ =
      gfx::ImageSkia(new FolderImageSource(top_icons, icon_size), icon_size);

  FOR_EACH_OBSERVER(FolderImageObserver, observers_, OnFolderImageUpdated());
}

}  // namespace app_list
