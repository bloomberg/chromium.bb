// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_folder_item.h"

#include "base/guid.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item_list.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace app_list {

namespace {

const int kItemIconDimension = 16;

// Generates the folder icon with the top 4 child item icons laid in 2x2 tile.
class FolderImageSource : public gfx::CanvasImageSource {
 public:
  typedef std::vector<gfx::ImageSkia> Icons;

  FolderImageSource(const Icons& icons, const gfx::Size& size)
      : gfx::CanvasImageSource(size, false),
        icons_(icons),
        size_(size) {
    DCHECK(icons.size() <= kNumFolderTopItems);
  }

  virtual ~FolderImageSource() {}

 private:
  void DrawIcon(gfx::Canvas* canvas,
                const gfx::ImageSkia& icon,
                const gfx::Size icon_size,
                int x, int y) {
    if (icon.isNull())
      return;

    gfx::ImageSkia resized(
        gfx::ImageSkiaOperations::CreateResizedImage(
            icon, skia::ImageOperations::RESIZE_BEST, icon_size));
    canvas->DrawImageInt(resized, 0, 0, resized.width(), resized.height(),
        x, y, resized.width(), resized.height(), true);
  }

  // gfx::CanvasImageSource overrides:
  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    // Draw folder circle.
    gfx::Point center = gfx::Point(size().width() / 2 , size().height() / 2);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setColor(kFolderBubbleColor);
    canvas->DrawCircle(center, size().width() / 2, paint);

    if (icons_.size() == 0)
      return;

    // Draw top items' icons.
    const gfx::Size item_icon_size =
        gfx::Size(kItemIconDimension, kItemIconDimension);
    Rects top_icon_bounds =
        AppListFolderItem::GetTopIconsBounds(gfx::Rect(size()));

    for (size_t i= 0; i < kNumFolderTopItems && i < icons_.size(); ++i) {
      DrawIcon(canvas, icons_[i], item_icon_size,
               top_icon_bounds[i].x(), top_icon_bounds[i].y());
    }
  }

  Icons icons_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(FolderImageSource);
};

}  // namespace

AppListFolderItem::AppListFolderItem(const std::string& id,
                                     FolderType folder_type)
    : AppListItem(id),
      folder_type_(folder_type),
      item_list_(new AppListItemList) {
  item_list_->AddObserver(this);
}

AppListFolderItem::~AppListFolderItem() {
  for (size_t i = 0; i < top_items_.size(); ++i)
    top_items_[i]->RemoveObserver(this);
  item_list_->RemoveObserver(this);
}

void AppListFolderItem::UpdateIcon() {
  FolderImageSource::Icons top_icons;
  for (size_t i = 0; i < top_items_.size(); ++i)
    top_icons.push_back(top_items_[i]->icon());

  const gfx::Size icon_size =
      gfx::Size(kPreferredIconDimension, kPreferredIconDimension);
  gfx::ImageSkia icon = gfx::ImageSkia(
      new FolderImageSource(top_icons, icon_size),
      icon_size);
  SetIcon(icon, false);
}

const gfx::ImageSkia& AppListFolderItem::GetTopIcon(size_t item_index) {
  DCHECK(item_index <= top_items_.size());
  return top_items_[item_index]->icon();
}

gfx::Rect AppListFolderItem::GetTargetIconRectInFolderForItem(
    AppListItem* item,
    const gfx::Rect& folder_icon_bounds) {
  for (size_t i = 0; i < top_items_.size(); ++i) {
    if (item->id() == top_items_[i]->id()) {
      Rects rects = AppListFolderItem::GetTopIconsBounds(folder_icon_bounds);
      return rects[i];
    }
  }

  gfx::Rect target_rect(folder_icon_bounds);
  target_rect.ClampToCenteredSize(
      gfx::Size(kItemIconDimension, kItemIconDimension));
  return target_rect;
}

void AppListFolderItem::Activate(int event_flags) {
  // Folder handling is implemented by the View, so do nothing.
}

// static
const char AppListFolderItem::kItemType[] = "FolderItem";

// static
Rects AppListFolderItem::GetTopIconsBounds(
    const gfx::Rect& folder_icon_bounds) {
  const int delta_to_center = 1;
  gfx::Point icon_center = folder_icon_bounds.CenterPoint();
  Rects top_icon_bounds;

  // Get the top left icon bounds.
  int left_x = icon_center.x() - kItemIconDimension - delta_to_center;
  int top_y = icon_center.y() - kItemIconDimension - delta_to_center;
  gfx::Rect top_left(left_x, top_y, kItemIconDimension, kItemIconDimension);
  top_icon_bounds.push_back(top_left);

  // Get the top right icon bounds.
  int right_x = icon_center.x() + delta_to_center;
  gfx::Rect top_right(right_x, top_y, kItemIconDimension, kItemIconDimension);
  top_icon_bounds.push_back(top_right);

  // Get the bottom left icon bounds.
  int bottom_y = icon_center.y() + delta_to_center;
  gfx::Rect bottom_left(
      left_x, bottom_y, kItemIconDimension, kItemIconDimension);
  top_icon_bounds.push_back(bottom_left);

  // Get the bottom right icon bounds.
  gfx::Rect bottom_right(
      right_x, bottom_y, kItemIconDimension, kItemIconDimension);
  top_icon_bounds.push_back(bottom_right);

  return top_icon_bounds;
}

const char* AppListFolderItem::GetItemType() const {
  return AppListFolderItem::kItemType;
}

ui::MenuModel* AppListFolderItem::GetContextMenuModel() {
  // TODO(stevenjb/jennyz): Implement.
  return NULL;
}

AppListItem* AppListFolderItem::FindChildItem(const std::string& id) {
  return item_list_->FindItem(id);
}

size_t AppListFolderItem::ChildItemCount() const {
  return item_list_->item_count();
}

void AppListFolderItem::OnExtensionPreferenceChanged() {
  for (size_t i = 0; i < item_list_->item_count(); ++i)
    item_list_->item_at(i)->OnExtensionPreferenceChanged();
}

bool AppListFolderItem::CompareForTest(const AppListItem* other) const {
  if (!AppListItem::CompareForTest(other))
    return false;
  const AppListFolderItem* other_folder =
      static_cast<const AppListFolderItem*>(other);
  if (other_folder->item_list()->item_count() != item_list_->item_count())
    return false;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    if (!item_list()->item_at(i)->CompareForTest(
            other_folder->item_list()->item_at(i)))
      return false;
  }
  return true;
}

std::string AppListFolderItem::GenerateId() {
  return base::GenerateGUID();
}

void AppListFolderItem::ItemIconChanged() {
  UpdateIcon();
}

void AppListFolderItem::ItemNameChanged() {
}

void AppListFolderItem::ItemHighlightedChanged() {
}

void AppListFolderItem::ItemIsInstallingChanged() {
}

void AppListFolderItem::ItemPercentDownloadedChanged() {
}

void AppListFolderItem::OnListItemAdded(size_t index,
                                        AppListItem* item) {
  if (index <= kNumFolderTopItems)
    UpdateTopItems();
}

void AppListFolderItem::OnListItemRemoved(size_t index,
                                          AppListItem* item) {
  if (index <= kNumFolderTopItems)
    UpdateTopItems();
}

void AppListFolderItem::OnListItemMoved(size_t from_index,
                                        size_t to_index,
                                        AppListItem* item) {
  if (from_index <= kNumFolderTopItems || to_index <= kNumFolderTopItems)
    UpdateTopItems();
}

void AppListFolderItem::UpdateTopItems() {
  for (size_t i = 0; i < top_items_.size(); ++i)
    top_items_[i]->RemoveObserver(this);
  top_items_.clear();

  for (size_t i = 0;
       i < kNumFolderTopItems && i < item_list_->item_count(); ++i) {
    AppListItem* item = item_list_->item_at(i);
    item->AddObserver(this);
    top_items_.push_back(item);
  }
  UpdateIcon();
}

}  // namespace app_list
