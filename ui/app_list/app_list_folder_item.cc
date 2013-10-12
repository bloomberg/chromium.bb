// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_folder_item.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace app_list {

namespace {

const int kIconDimension = 48;
const size_t kNumTopApps = 4;
const int kItemIconDimension = 16;

// Genearats the folder icon with the top 4 child item icons laid in 2x2 tile.
class FolderImageSource : public gfx::CanvasImageSource {
 public:
  typedef std::vector<gfx::ImageSkia> Icons;

  FolderImageSource(const Icons& icons, const gfx::Size& size)
      : gfx::CanvasImageSource(size, false),
        icons_(icons),
        size_(size) {
    DCHECK(icons.size() <= kNumTopApps);
  }

  virtual ~FolderImageSource() {}

 private:
  void DrawIcon(gfx::Canvas* canvas,
                const gfx::ImageSkia& icon,
                const gfx::Size icon_size,
                int x, int y) {
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
    const SkColor kCirclColor = SkColorSetRGB(0xE1, 0xE1, 0xE1);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setColor(kCirclColor);
    canvas->DrawCircle(center, size().width() / 2, paint);

    if (icons_.size() == 0)
      return;

    // Tiled icon coordinates.
    const int delta_to_center = 1;
    const gfx::Size item_icon_size =
        gfx::Size(kItemIconDimension, kItemIconDimension);
    int left_x = center.x() - item_icon_size.width() - delta_to_center;
    int top_y = center.y() - item_icon_size.height() - delta_to_center;
    int right_x = center.x() + delta_to_center;
    int bottom_y = center.y() + delta_to_center;

    // top left icon
    size_t i = 0;
    DrawIcon(canvas, icons_[i++], item_icon_size, left_x, top_y);

    // top right icon
    if (i < icons_.size())
      DrawIcon(canvas, icons_[i++], item_icon_size, right_x, top_y);

    // left bottm icon
    if (i < icons_.size())
      DrawIcon(canvas, icons_[i++], item_icon_size, left_x, bottom_y);

    // right bottom icon
    if (i < icons_.size())
      DrawIcon(canvas, icons_[i], item_icon_size, right_x, bottom_y);
  }

  Icons icons_;
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(FolderImageSource);
};

}  // namespace

AppListFolderItem::AppListFolderItem(const std::string& id)
    : AppListItemModel(id),
      apps_(new Apps) {
}

AppListFolderItem::~AppListFolderItem() {
  for (size_t i = 0; i < top_items_.size(); ++i)
    top_items_[i]->RemoveObserver(this);
}

void AppListFolderItem::AddItem(AppListItemModel* item) {
  std::string sort_order = item->GetSortOrder();
  // Note: ui::ListModel is not a sorted list.
  size_t index = 0;
  for (; index < apps_->item_count(); ++index) {
    if (sort_order < apps_->GetItemAt(index)->GetSortOrder())
      break;
  }
  apps_->AddAt(index, item);
  if (index <= kNumTopApps)
    UpdateTopItems();
}

void AppListFolderItem::DeleteItem(const std::string& id) {
  for (size_t i = 0; i < apps_->item_count(); ++i) {
    AppListItemModel* item = apps_->GetItemAt(i);
    if (item->id() == id) {
      scoped_ptr<AppListItemModel> to_be_deleted(apps_->RemoveAt(i));
      DCHECK(item == to_be_deleted.get());
      if (i <= kNumTopApps)
        UpdateTopItems();
      return;
    }
  }
}

void AppListFolderItem::UpdateIcon() {
  FolderImageSource::Icons top_icons;
  for (size_t i = 0; i < top_items_.size(); ++i)
    top_icons.push_back(top_items_[i]->icon());

  const gfx::Size icon_size = gfx::Size(kIconDimension, kIconDimension);
  gfx::ImageSkia icon = gfx::ImageSkia(
      new FolderImageSource(top_icons, icon_size),
      icon_size);
  SetIcon(icon, false);
}

std::string AppListFolderItem::GetSortOrder() const {
  // For now, put folders at the end of the list.
  // TODO(stevenjb): Implement synced app list ordering.
  return "zzzzzzzz";
}

void AppListFolderItem::Activate(int event_flags) {
  // TODO(stevenjb/jennyz): Implement.
  VLOG(1) << "AppListFolderItem::Activate";
}

// static
const char AppListFolderItem::kAppType[] = "FolderItem";

const char* AppListFolderItem::GetAppType() const {
  return AppListFolderItem::kAppType;
}

ui::MenuModel* AppListFolderItem::GetContextMenuModel() {
  // TODO(stevenjb/jennyz): Implement.
  return NULL;
}

void AppListFolderItem::ItemIconChanged() {
  UpdateIcon();
}

void AppListFolderItem::ItemTitleChanged() {
}

void AppListFolderItem::ItemHighlightedChanged() {
}

void AppListFolderItem::ItemIsInstallingChanged() {
}

void AppListFolderItem::ItemPercentDownloadedChanged() {
}

void AppListFolderItem::UpdateTopItems() {
  for (size_t i = 0; i < top_items_.size(); ++i)
    top_items_[i]->RemoveObserver(this);
  top_items_.clear();

  for (size_t i = 0; i < kNumTopApps && i < apps_->item_count(); ++i) {
    AppListItemModel* item = apps_->GetItemAt(i);
    item->AddObserver(this);
    top_items_.push_back(item);
  }
  UpdateIcon();
}

}  // namespace app_list
