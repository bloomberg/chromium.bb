// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/apps_container_view.h"

#include <algorithm>

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"

namespace app_list {

namespace {

// Transitional view used for top item icons animation when opening or closing
// a folder.
class TopIconAnimationView : public views::View,
                             public ui::ImplicitAnimationObserver {
 public:
  TopIconAnimationView(const gfx::ImageSkia& icon,
                       const gfx::Rect& scaled_rect,
                       bool open_folder)
      : icon_size_(kPreferredIconDimension, kPreferredIconDimension),
        icon_(new views::ImageView),
        scaled_rect_(scaled_rect),
        open_folder_(open_folder) {
    DCHECK(!icon.isNull());
    gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
          icon,
          skia::ImageOperations::RESIZE_BEST, icon_size_));
    icon_->SetImage(resized);
    AddChildView(icon_);

#if defined(USE_AURA)
    SetPaintToLayer(true);
    SetFillsBoundsOpaquely(false);
#endif
  }
  virtual ~TopIconAnimationView() {}

  void AddObserver(TopIconAnimationObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(TopIconAnimationObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  void TransformView() {
    // Transform used for scaling down the icon and move it back inside to the
    // original folder icon.
    const float kIconTransformScale = 0.33333f;
    gfx::Transform transform;
    transform.Translate(scaled_rect_.x() - layer()->bounds().x(),
                        scaled_rect_.y() - layer()->bounds().y());
    transform.Scale(kIconTransformScale, kIconTransformScale);

    if (open_folder_) {
      // Transform to a scaled down icon inside the original folder icon.
      layer()->SetTransform(transform);
    }

    // Animate the icon to its target location and scale when opening or
    // closing a folder.
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.AddObserver(this);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kFolderTransitionInDurationMs));
    layer()->SetTransform(open_folder_ ? gfx::Transform() : transform);
  }

 private:
  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return icon_size_;
  }

  virtual void Layout() OVERRIDE {
    icon_->SetBoundsRect(GetContentsBounds());
  }

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    SetVisible(false);
    FOR_EACH_OBSERVER(TopIconAnimationObserver,
                      observers_,
                      OnTopIconAnimationsComplete(this));
  }

  gfx::Size icon_size_;
  views::ImageView* icon_;  // Owned by views hierarchy.
  // Rect of the scaled down top item icon inside folder icon's ink bubble.
  gfx::Rect scaled_rect_;
  // True: opening folder; False: closing folder.
  bool open_folder_;

  ObserverList<TopIconAnimationObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(TopIconAnimationView);
};

}  // namespace

AppsContainerView::AppsContainerView(AppListMainView* app_list_main_view,
                                     PaginationModel* pagination_model,
                                     AppListModel* model,
                                     content::WebContents* start_page_contents)
    : model_(model),
      show_state_(SHOW_APPS) {
  apps_grid_view_ = new AppsGridView(
      app_list_main_view, pagination_model, start_page_contents);
  apps_grid_view_->SetLayout(kPreferredIconDimension,
                             kPreferredCols,
                             kPreferredRows);
  AddChildView(apps_grid_view_);

  app_list_folder_view_ = new AppListFolderView(
      this,
      model,
      app_list_main_view,
      start_page_contents);
  AddChildView(app_list_folder_view_);

  apps_grid_view_->SetModel(model_);
  apps_grid_view_->SetItemList(model_->item_list());
}

AppsContainerView::~AppsContainerView() {
}

void AppsContainerView::ShowActiveFolder(AppListFolderItem* folder_item) {
  app_list_folder_view_->SetAppListFolderItem(folder_item);
  SetShowState(SHOW_ACTIVE_FOLDER);

  CreateViewsForFolderTopItemsAnimation(folder_item, true);
}

void AppsContainerView::ShowApps(AppListFolderItem* folder_item) {
  if (folder_item)
    CreateViewsForFolderTopItemsAnimation(folder_item, false);
  else
    top_icon_views_.clear();
  // Hide the active folder view until the animation completes.
  apps_grid_view_->activated_item_view()->SetVisible(false);
  SetShowState(SHOW_APPS);
}

gfx::Size AppsContainerView::GetPreferredSize() {
  const gfx::Size grid_size = apps_grid_view_->GetPreferredSize();
  const gfx::Size folder_view_size = app_list_folder_view_->GetPreferredSize();

  int width = std::max(grid_size.width(), folder_view_size.width());
  int height = std::max(grid_size.height(), folder_view_size.height());
  return gfx::Size(width, height);
}

void AppsContainerView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  switch (show_state_) {
    case SHOW_APPS:
      app_list_folder_view_->ScheduleShowHideAnimation(false);
      apps_grid_view_->SetBoundsRect(rect);
      apps_grid_view_->ScheduleShowHideAnimation(true);
      break;
    case SHOW_ACTIVE_FOLDER:
      apps_grid_view_->ScheduleShowHideAnimation(false);
      app_list_folder_view_->SetBoundsRect(rect);
      app_list_folder_view_->ScheduleShowHideAnimation(true);
      break;
    default:
      NOTREACHED();
  }
}

bool AppsContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  if (show_state_ == SHOW_APPS)
    return apps_grid_view_->OnKeyPressed(event);
  else
    return app_list_folder_view_->OnKeyPressed(event);
}

void AppsContainerView::OnTopIconAnimationsComplete(views::View* icon_view) {
  bool animations_done = true;
  for (size_t i = 0; i < top_icon_views_.size(); ++i) {
    if (top_icon_views_[i]->visible()) {
      animations_done = false;
      break;
    }
  }

  if (animations_done) {
    // Clean up the transitional views using for top item icon animation.
    for (size_t i = 0; i < top_icon_views_.size(); ++i) {
      TopIconAnimationView* icon_view =
          static_cast<TopIconAnimationView*>(top_icon_views_[i]);
      icon_view->RemoveObserver(this);
      delete icon_view;
    }
    top_icon_views_.clear();

    // Show the folder icon when closing the folder.
    if (show_state_ == SHOW_APPS && apps_grid_view_->activated_item_view())
      apps_grid_view_->activated_item_view()->SetVisible(true);
  }
}

void AppsContainerView::SetShowState(ShowState show_state) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;
  Layout();
}

Rects AppsContainerView::GetTopItemIconBoundsInActiveFolder() {
  // Get the active folder's icon bounds relative to AppsContainerView.
  AppListItemView* folder_view = apps_grid_view_->activated_item_view();
  gfx::Rect to_grid_view = folder_view->ConvertRectToParent(
      folder_view->GetIconBounds());
  gfx::Rect to_container = apps_grid_view_->ConvertRectToParent(to_grid_view);

  return AppListFolderItem::GetTopIconsBounds(to_container);
}

void AppsContainerView::CreateViewsForFolderTopItemsAnimation(
    AppListFolderItem* active_folder,
    bool open_folder) {
  top_icon_views_.clear();
  std::vector<gfx::Rect> top_items_bounds =
      GetTopItemIconBoundsInActiveFolder();
  size_t top_items_count =
      std::min(kNumFolderTopItems, active_folder->item_list()->item_count());
  for (size_t i = 0; i < top_items_count; ++i) {
    TopIconAnimationView* icon_view = new TopIconAnimationView(
        active_folder->GetTopIcon(i), top_items_bounds[i], open_folder);
    icon_view->AddObserver(this);
    top_icon_views_.push_back(icon_view);

    // Add the transitional views into child views, and set its bounds to the
    // same location of the item in the folder list view.
    AddChildView(top_icon_views_[i]);
    top_icon_views_[i]->SetBoundsRect(
        app_list_folder_view_->ConvertRectToParent(
            app_list_folder_view_->GetItemIconBoundsAt(i)));
    static_cast<TopIconAnimationView*>(top_icon_views_[i])->TransformView();
  }
}

}  // namespace app_list
