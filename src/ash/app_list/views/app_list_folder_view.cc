// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_folder_view.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_folder_controller.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/folder_background_view.h"
#include "ash/app_list/views/folder_header_view.h"
#include "ash/app_list/views/page_switcher.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/top_icon_animation_view.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/controls/scroll_view_gradient_helper.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

using views::BoxLayout;

namespace ash {

namespace {

constexpr int kFolderHeaderPadding = 12;
constexpr int kOnscreenKeyboardTopPadding = 16;

constexpr int kTileSpacingInFolder = 8;

// Insets for the vertical scroll bar. The top is pushed down slightly to align
// with the icons, which keeps the scroll bar out of the rounded corner area.
constexpr gfx::Insets kVerticalScrollInsets(kTileSpacingInFolder, 0, 1, 1);

// Duration for fading in the target page when opening
// or closing a folder, and the duration for the top folder icon animation
// for flying in or out the folder.
constexpr base::TimeDelta kFolderTransitionDuration = base::Milliseconds(250);

// Transit from the background of the folder item's icon to the opened
// folder's background when opening the folder. Transit the other way when
// closing the folder.
class BackgroundAnimation : public AppListFolderView::Animation,
                            public ui::ImplicitAnimationObserver {
 public:
  BackgroundAnimation(bool show,
                      AppListFolderView* folder_view,
                      views::View* background_view)
      : show_(show),
        folder_view_(folder_view),
        background_view_(background_view) {}

  BackgroundAnimation(const BackgroundAnimation&) = delete;
  BackgroundAnimation& operator=(const BackgroundAnimation&) = delete;

  ~BackgroundAnimation() override = default;

 private:
  // AppListFolderView::Animation:
  void ScheduleAnimation(base::OnceClosure completion_callback) override {
    DCHECK(!completion_callback_);
    completion_callback_ = std::move(completion_callback);

    // Calculate the source and target states.
    const int icon_radius =
        folder_view_->GetAppListConfig()->folder_icon_radius();
    const int folder_radius =
        folder_view_->GetAppListConfig()->folder_background_radius();
    const int from_radius = show_ ? icon_radius : folder_radius;
    const int to_radius = show_ ? folder_radius : icon_radius;
    gfx::Rect from_rect = show_ ? folder_view_->folder_item_icon_bounds()
                                : background_view_->bounds();
    from_rect -= background_view_->bounds().OffsetFromOrigin();
    gfx::Rect to_rect = show_ ? background_view_->bounds()
                              : folder_view_->folder_item_icon_bounds();
    to_rect -= background_view_->bounds().OffsetFromOrigin();
    const SkColor background_color =
        AppListColorProvider::Get()->GetFolderBackgroundColor();
    const SkColor bubble_color =
        AppListColorProvider::Get()->GetFolderBubbleColor();
    const SkColor from_color = show_ ? bubble_color : background_color;
    const SkColor to_color = show_ ? background_color : bubble_color;

    background_view_->layer()->SetColor(from_color);
    background_view_->layer()->SetClipRect(from_rect);
    background_view_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(from_radius));

    ui::ScopedLayerAnimationSettings settings(
        background_view_->layer()->GetAnimator());
    settings.SetTransitionDuration(kFolderTransitionDuration);
    settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    settings.AddObserver(this);
    background_view_->layer()->SetColor(to_color);
    background_view_->layer()->SetClipRect(to_rect);
    background_view_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(to_radius));
    is_animating_ = true;
  }

  bool IsAnimationRunning() override { return is_animating_; }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    is_animating_ = false;
    folder_view_->RecordAnimationSmoothness();

    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

  // True if opening the folder.
  const bool show_;

  bool is_animating_ = false;

  AppListFolderView* const folder_view_;  // Not owned.
  views::View* const background_view_;    // Not owned.

  base::OnceClosure completion_callback_;
};

// Decrease the opacity of the folder item's title when opening the folder.
// Increase it when closing the folder.
class FolderItemTitleAnimation : public AppListFolderView::Animation,
                                 public views::AnimationDelegateViews {
 public:
  FolderItemTitleAnimation(bool show,
                           AppListFolderView* folder_view,
                           AppListItemView* folder_item_view)
      : views::AnimationDelegateViews(folder_view),
        show_(show),
        animation_(this),
        folder_view_(folder_view),
        folder_item_view_(folder_item_view) {
    SkColor title_color = AppListColorProvider::Get()->GetAppListItemTextColor(
        /*is_in_folder=*/false);
    // Calculate the source and target states.
    from_color_ = show_ ? title_color : SK_ColorTRANSPARENT;
    to_color_ = show_ ? SK_ColorTRANSPARENT : title_color;

    animation_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    animation_.SetSlideDuration(
        ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
        kFolderTransitionDuration);
  }

  FolderItemTitleAnimation(const FolderItemTitleAnimation&) = delete;
  FolderItemTitleAnimation& operator=(const FolderItemTitleAnimation&) = delete;

  ~FolderItemTitleAnimation() override = default;

 private:
  // AppListFolderView::Animation:
  void ScheduleAnimation(base::OnceClosure completion_callback) override {
    DCHECK(!completion_callback_);
    completion_callback_ = std::move(completion_callback);
    animation_.Show();
  }
  bool IsAnimationRunning() override { return animation_.is_animating(); }

  // gfx::AnimationDelegate
  void AnimationProgressed(const gfx::Animation* animation) override {
    folder_item_view_->title()->SetEnabledColor(gfx::Tween::ColorValueBetween(
        animation->GetCurrentValue(), from_color_, to_color_));
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    folder_item_view_->title()->SetEnabledColor(to_color_);
    folder_view_->RecordAnimationSmoothness();

    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  // True if opening the folder.
  const bool show_;

  // The source and target state of the title's color.
  SkColor from_color_;
  SkColor to_color_;

  gfx::SlideAnimation animation_;

  AppListFolderView* const folder_view_;  // Not owned.

  // The app list item view with which the folder view is associated.
  // NOTE: Users of `FolderItemTitleAnimation` should ensure the animation does
  // not outlive the `folder_item_view_`.
  AppListItemView* const folder_item_view_;

  base::OnceClosure completion_callback_;
};

// Transit from the items within the folder item icon to the same items in the
// opened folder when opening the folder. Transit the other way when closing the
// folder.
class TopIconAnimation : public AppListFolderView::Animation,
                         public TopIconAnimationObserver {
 public:
  TopIconAnimation(bool show,
                   AppListFolderView* folder_view,
                   views::ScrollView* scroll_view,
                   AppListItemView* folder_item_view)
      : show_(show),
        folder_view_(folder_view),
        scroll_view_(scroll_view),
        folder_item_view_(folder_item_view) {}

  TopIconAnimation(const TopIconAnimation&) = delete;
  TopIconAnimation& operator=(const TopIconAnimation&) = delete;

  ~TopIconAnimation() override {
    for (auto* view : top_icon_views_)
      view->RemoveObserver(this);
    top_icon_views_.clear();
  }

  // AppListFolderView::Animation
  void ScheduleAnimation(base::OnceClosure completion_callback) override {
    DCHECK(!completion_callback_);
    completion_callback_ = std::move(completion_callback);

    // Hide the original items in the folder until the animation ends.
    SetFirstPageItemViewsVisible(false);
    folder_item_view_->SetIconVisible(false);

    // Calculate the start and end bounds of the top item icons in the
    // animation.
    std::vector<gfx::Rect> top_item_views_bounds =
        GetTopItemViewsBoundsInFolderIcon();
    std::vector<gfx::Rect> first_page_item_views_bounds =
        GetFirstPageItemViewsBounds();
    top_icon_views_.clear();

    const AppListConfigType app_list_config_type =
        folder_view_->GetAppListConfig()->type();

    // Get top folder items that should be animated - note that item index in
    // the folder item list may not match the intended item bounds in
    // `first_page_item_views_bounds` if it's preceded by a drag item in the
    // item list.
    std::vector<const AppListItem*> top_items;
    const AppListItem* drag_item = folder_view_->items_grid_view()->drag_item();
    const AppListItemList* folder_items =
        folder_view_->folder_item()->item_list();
    for (size_t i = 0; i < folder_items->item_count(); ++i) {
      if (top_items.size() == first_page_item_views_bounds.size())
        break;
      const AppListItem* top_item = folder_items->item_at(i);
      if (top_item->GetIcon(app_list_config_type).isNull() ||
          top_item == drag_item) {
        // The item being dragged should be excluded.
        continue;
      }
      top_items.push_back(top_item);
    }

    for (size_t i = 0; i < top_items.size(); ++i) {
      const AppListItem* top_item = top_items[i];
      bool item_in_folder_icon = i < top_item_views_bounds.size();
      gfx::Rect scaled_rect = item_in_folder_icon
                                  ? top_item_views_bounds[i]
                                  : folder_view_->folder_item_icon_bounds();

      auto icon_view = std::make_unique<TopIconAnimationView>(
          folder_view_->items_grid_view(),
          top_item->GetIcon(app_list_config_type),
          base::UTF8ToUTF16(top_item->GetDisplayName()), scaled_rect, show_,
          item_in_folder_icon);
      auto* icon_view_ptr = icon_view.get();

      icon_view_ptr->AddObserver(this);
      // Add the transitional views into child views, and set its bounds to the
      // same location of the item in the folder list view.
      top_icon_views_.push_back(
          folder_view_->background_view()->AddChildView(std::move(icon_view)));
      icon_view_ptr->SetBoundsRect(first_page_item_views_bounds[i]);
      icon_view_ptr->TransformView(kFolderTransitionDuration);
    }

    if (top_icon_views_.empty())
      OnAnimationComplete();
  }

  bool IsAnimationRunning() override { return !top_icon_views_.empty(); }

  // TopIconAnimationObserver
  void OnTopIconAnimationsComplete(TopIconAnimationView* view) override {
    // Clean up the transitional view for which the animation completes.
    view->RemoveObserver(this);
    auto to_delete =
        std::find(top_icon_views_.begin(), top_icon_views_.end(), view);
    DCHECK(to_delete != top_icon_views_.end());
    top_icon_views_.erase(to_delete);

    folder_view_->RecordAnimationSmoothness();

    // An empty list indicates that all animations are done.
    if (top_icon_views_.empty())
      OnAnimationComplete();
  }

  // Called when all top icon animations complete.
  void OnAnimationComplete() {
    // Set top item views visible when opening the folder.
    if (show_)
      SetFirstPageItemViewsVisible(true);

    // Show the folder icon when closing the folder.
    if (!show_)
      folder_item_view_->SetIconVisible(true);

    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

 private:
  std::vector<gfx::Rect> GetTopItemViewsBoundsInFolderIcon() {
    const AppListConfig* const app_list_config =
        folder_view_->GetAppListConfig();
    size_t effective_folder_size =
        folder_view_->folder_item()->ChildItemCount();
    // If a folder item is being dragged, it should be hidden from the folder
    // item icon, and top icons bounds should be calculated as if the item is
    // not in the folder.
    if (folder_view_->items_grid_view()->drag_item())
      effective_folder_size -= 1;

    std::vector<gfx::Rect> top_icons_bounds = FolderImage::GetTopIconsBounds(
        *app_list_config, folder_view_->folder_item_icon_bounds(),
        std::min(effective_folder_size, FolderImage::kNumFolderTopItems));
    std::vector<gfx::Rect> top_item_views_bounds;
    const int icon_dimension = app_list_config->grid_icon_dimension();
    const int icon_bottom_padding = app_list_config->grid_icon_bottom_padding();
    const int tile_width = app_list_config->grid_tile_width();
    const int tile_height = app_list_config->grid_tile_height();
    for (gfx::Rect bounds : top_icons_bounds) {
      // Calculate the item view's bounds based on the icon bounds.
      gfx::Rect item_bounds(
          (icon_dimension - tile_width) / 2,
          (icon_dimension + icon_bottom_padding - tile_height) / 2, tile_width,
          tile_height);
      item_bounds = gfx::ScaleToRoundedRect(
          item_bounds, bounds.width() / static_cast<float>(icon_dimension),
          bounds.height() / static_cast<float>(icon_dimension));
      item_bounds.Offset(bounds.x(), bounds.y());

      top_item_views_bounds.emplace_back(item_bounds);
    }
    return top_item_views_bounds;
  }

  void SetFirstPageItemViewsVisible(bool visible) {
    // Items grid view has to be visible in case an item is being reparented, so
    // only set the opacity here.
    folder_view_->items_grid_view()->layer()->SetOpacity(visible ? 1.0f : 0.0f);
  }

  // Get the bounds of the items in the first page of the opened folder relative
  // to AppListFolderView.
  std::vector<gfx::Rect> GetFirstPageItemViewsBounds() {
    std::vector<gfx::Rect> items_bounds;
    // Go over items in the folder, and collect bounds of items that fit within
    // the bounds of the first "page" of apps.
    const size_t count = folder_view_->folder_item()->ChildItemCount();
    views::View* container =
        features::IsProductivityLauncherEnabled()
            ? static_cast<views::View*>(scroll_view_)
            : static_cast<views::View*>(folder_view_->items_grid_view());
    const gfx::RectF container_bounds(container->GetLocalBounds());
    for (size_t i = 0; i < count; ++i) {
      views::View* item = folder_view_->items_grid_view()->GetItemViewAt(i);
      if (folder_view_->items_grid_view()->IsViewHiddenForDrag(item))
        continue;

      // Stop if the item bounds are not within the container bounds - assumes
      // that subsequent item bounds would not be within the container view
      // either.
      gfx::RectF bounds_in_container(item->GetLocalBounds());
      views::View::ConvertRectToTarget(item, container, &bounds_in_container);
      if (!container_bounds.Contains(bounds_in_container))
        break;

      // Return the item bounds in AppListFolderView coordinates.
      gfx::RectF bounds_in_folder(item->GetLocalBounds());
      views::View::ConvertRectToTarget(item, folder_view_, &bounds_in_folder);
      items_bounds.emplace_back(
          folder_view_->GetMirroredRect(gfx::ToRoundedRect(bounds_in_folder)));
    }
    return items_bounds;
  }

  // True if opening the folder.
  const bool show_;

  AppListFolderView* const folder_view_;  // Not owned.

  // The scroll view that contains the apps grid. Used with
  // ProductivityLauncher.
  views::ScrollView* const scroll_view_;

  // The app list item view with which the folder view is associated.
  // NOTE: Users of `TopIconAnimation` should ensure the animation does
  // not outlive the `folder_item_view_`.
  AppListItemView* const folder_item_view_;

  std::vector<TopIconAnimationView*> top_icon_views_;

  base::OnceClosure completion_callback_;
};

// Transit from the bounds of the folder item icon to the opened folder's
// bounds and transit opacity from 0 to 1 when opening the folder. Transit the
// other way when closing the folder.
class ContentsContainerAnimation : public AppListFolderView::Animation,
                                   public ui::ImplicitAnimationObserver {
 public:
  ContentsContainerAnimation(bool show,
                             bool hide_for_reparent,
                             AppListFolderView* folder_view)
      : show_(show),
        hide_for_reparent_(hide_for_reparent),
        folder_view_(folder_view) {}

  ContentsContainerAnimation(const ContentsContainerAnimation&) = delete;
  ContentsContainerAnimation& operator=(const ContentsContainerAnimation&) =
      delete;

  ~ContentsContainerAnimation() override { StopObservingImplicitAnimations(); }

  // AppListFolderView::Animation
  void ScheduleAnimation(base::OnceClosure completion_callback) override {
    DCHECK(!completion_callback_);
    completion_callback_ = std::move(completion_callback);

    // Transform used to scale the folder's contents container from the bounds
    // of the folder icon to that of the opened folder.
    gfx::Transform transform;
    const gfx::Rect scaled_rect(folder_view_->folder_item_icon_bounds());
    const gfx::Rect rect(folder_view_->contents_container()->bounds());
    ui::Layer* layer = folder_view_->contents_container()->layer();
    transform.Translate(scaled_rect.x() - rect.x(), scaled_rect.y() - rect.y());
    transform.Scale(static_cast<double>(scaled_rect.width()) / rect.width(),
                    static_cast<double>(scaled_rect.height()) / rect.height());

    if (show_)
      layer->SetTransform(transform);
    layer->SetOpacity(show_ ? 0.0f : 1.0f);

    // The folder should be set visible only after it is scaled down and
    // transparent to prevent the flash of the view right before the animation.
    folder_view_->SetVisible(true);

    ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
    animation.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    animation.AddObserver(this);
    animation.SetTransitionDuration(kFolderTransitionDuration);
    layer->SetTransform(show_ ? gfx::Transform() : transform);
    layer->SetOpacity(show_ ? 1.0f : 0.0f);

    is_animation_running_ = true;
  }

  bool IsAnimationRunning() override { return is_animation_running_; }

  // ui::ImplicitAnimationObserver
  void OnImplicitAnimationsCompleted() override {
    is_animation_running_ = false;

    // If the view is hidden for reparenting a folder item, it has to be
    // visible, so that drag_view_ can keep receiving mouse events.
    if (!show_ && !hide_for_reparent_)
      folder_view_->SetVisible(false);

    // Set the view bounds offscreen, so that it won't overlap the root level
    // apps grid view during folder item reparenting transitional period.
    // Keeping the same width and height avoids re-layout and ensures that
    // AppListItemView continues to receive events. The view will be set
    // invisible at the end of the drag.
    if (hide_for_reparent_) {
      const gfx::Rect& bounds = folder_view_->bounds();
      folder_view_->SetPosition(gfx::Point(-bounds.width(), -bounds.height()));
    }

    // Reset the transform after animation so that the following folder's
    // preferred bounds is calculated correctly.
    folder_view_->contents_container()->layer()->SetTransform(gfx::Transform());
    folder_view_->RecordAnimationSmoothness();

    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

 private:
  // True if opening the folder.
  const bool show_;

  // True if an item in the folder is being reparented to root grid view.
  const bool hide_for_reparent_;

  AppListFolderView* const folder_view_;

  bool is_animation_running_ = false;

  base::OnceClosure completion_callback_;
};

// ScrollViewWithMaxHeight limits its preferred size to a maximum height that
// shows 4 apps grid rows.
class ScrollViewWithMaxHeight : public views::ScrollView {
 public:
  explicit ScrollViewWithMaxHeight(AppListFolderView* folder_view)
      : views::ScrollView(views::ScrollView::ScrollWithLayers::kEnabled),
        folder_view_(folder_view) {}
  ScrollViewWithMaxHeight(const ScrollViewWithMaxHeight&) = delete;
  ScrollViewWithMaxHeight& operator=(const ScrollViewWithMaxHeight&) = delete;
  ~ScrollViewWithMaxHeight() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = views::ScrollView::CalculatePreferredSize();
    const int tile_height =
        folder_view_->items_grid_view()->GetTotalTileSize(/*page=*/0).height();
    // Show a maximum of 4 full rows, plus a little bit of the next row to make
    // it obvious the view can scroll.
    const int max_height = (tile_height * 4) + (tile_height / 4);
    size.set_height(std::min(size.height(), max_height));
    return size;
  }

 private:
  AppListFolderView* const folder_view_;
};

}  // namespace

AppListFolderView::AppListFolderView(AppListFolderController* folder_controller,
                                     AppsGridView* root_apps_grid_view,
                                     ContentsView* contents_view,
                                     AppListA11yAnnouncer* a11y_announcer,
                                     AppListViewDelegate* view_delegate)
    : folder_controller_(folder_controller),
      root_apps_grid_view_(root_apps_grid_view),
      a11y_announcer_(a11y_announcer),
      view_delegate_(view_delegate) {
  DCHECK(folder_controller_);
  DCHECK(root_apps_grid_view_);
  DCHECK(a11y_announcer_);
  DCHECK(view_delegate_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // The background's corner radius cannot be changed in the same layer of the
  // contents container using layer animation, so use another layer to perform
  // such changes.
  background_view_ = AddChildView(std::make_unique<views::View>());
  background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  background_view_->layer()->SetFillsBoundsOpaquely(false);
  background_view_->layer()->SetBackgroundBlur(
      ColorProvider::kBackgroundBlurSigma);
  background_view_->layer()->SetBackdropFilterQuality(
      ColorProvider::kBackgroundBlurQuality);

  contents_container_ = AddChildView(std::make_unique<views::View>());
  contents_container_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  if (features::IsProductivityLauncherEnabled())
    CreateScrollableAppsGrid();
  else
    CreatePagedAppsGrid(contents_view);

  AppListModelProvider::Get()->AddObserver(this);
}

void AppListFolderView::CreatePagedAppsGrid(ContentsView* contents_view) {
  DCHECK(contents_view);
  // Cache typed apps grid view pointer to perform setup specific to
  // PagedAppsGridView (e.g. `SetMaxRows()`) without requiring static cast.
  PagedAppsGridView* items_grid_view =
      contents_container_->AddChildView(std::make_unique<PagedAppsGridView>(
          contents_view, a11y_announcer_, this, /*folder_controller=*/nullptr,
          /*container_delegate=*/this));
  contents_container_->layer()->SetMasksToBounds(true);
  items_grid_view_ = items_grid_view;

  items_grid_view_->Init();
  items_grid_view->SetMaxColumnsAndRows(
      kMaxFolderColumns,
      /*max_rows_on_first_page=*/kMaxPagedFolderRows,
      /*max_rows=*/kMaxPagedFolderRows);
  items_grid_view->SetFixedTilePadding(kTileSpacingInFolder / 2,
                                       kTileSpacingInFolder / 2);

  folder_header_view_ = contents_container_->AddChildView(
      std::make_unique<FolderHeaderView>(this));
  folder_header_view_->SetProperty(views::kMarginsKey,
                                   gfx::Insets(kFolderHeaderPadding, 0));

  page_switcher_ =
      contents_container_->AddChildView(std::make_unique<PageSwitcher>(
          items_grid_view->pagination_model(), false /* vertical */,
          view_delegate_->IsInTabletMode(),
          AppListColorProvider::Get()->GetFolderBackgroundColor()));

  contents_container_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(gfx::Insets(kTileSpacingInFolder))
      .SetCollapseMargins(true)
      .SetChildViewIgnoredByLayout(page_switcher_, true);
}

void AppListFolderView::CreateScrollableAppsGrid() {
  // The top part of the folder contents is a scrollable apps grid.
  scroll_view_ = contents_container_->AddChildView(
      std::make_unique<ScrollViewWithMaxHeight>(this));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  // Don't paint a background. The folder already has one.
  scroll_view_->SetBackgroundColor(absl::nullopt);
  // Arrow keys are used to select app icons.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // Set up fade in/fade out gradients at top/bottom of scroll view.
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  gradient_helper_ = std::make_unique<ScrollViewGradientHelper>(scroll_view_);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto vertical_scroll =
      std::make_unique<RoundedScrollBar>(/*horizontal=*/false);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  // Add margins inside the scroll contents.
  auto scroll_contents = std::make_unique<views::View>();
  scroll_contents->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(gfx::Insets(kTileSpacingInFolder))
      .SetCollapseMargins(true);

  // Create the apps grid.
  auto* items_grid_view =
      scroll_contents->AddChildView(std::make_unique<ScrollableAppsGridView>(
          a11y_announcer_, view_delegate_, this, scroll_view_,
          /*folder_controller=*/nullptr, /*focus_delegate=*/nullptr));
  items_grid_view_ = items_grid_view;
  items_grid_view->Init();
  items_grid_view->SetMaxColumns(kMaxFolderColumns);
  items_grid_view->SetFixedTilePadding(kTileSpacingInFolder / 2,
                                       kTileSpacingInFolder / 2);
  scroll_view_->SetContents(std::move(scroll_contents));

  // In the common case, the parent view is large and the folder has a small
  // number of apps, so the scroll view's size will be limited by the apps grid
  // view's preferred size. However, if the parent view is small, the scroll
  // view will scale down, so there is enough space for the header view.
  scroll_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));

  folder_header_view_ = contents_container_->AddChildView(
      std::make_unique<FolderHeaderView>(this));
  folder_header_view_->SetProperty(views::kMarginsKey,
                                   gfx::Insets(kFolderHeaderPadding, 0));

  // No margins on `contents_container_` because the scroll view needs to fully
  // extend to the parent's edges.
  contents_container_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

AppListFolderView::~AppListFolderView() {
  AppListModelProvider::Get()->RemoveObserver(this);

  // This prevents the AppsGridView's destructor from calling the now-deleted
  // AppListFolderView's methods if a drag is in progress at the time.
  items_grid_view_->set_folder_delegate(nullptr);

  // Make sure |page_switcher_| is deleted before |items_grid_view_| because
  // |page_switcher_| uses the PaginationModel owned by |items_grid_view_|.
  delete page_switcher_;
}

void AppListFolderView::UpdateAppListConfig(const AppListConfig* config) {
  items_grid_view_->UpdateAppListConfig(config);
  ShrinkGridTileMarginsWhenNeeded();
}

void AppListFolderView::ConfigureForFolderItemView(
    AppListItemView* folder_item_view) {
  DCHECK(folder_item_view->is_folder());
  DCHECK(folder_item_view->item());
  DCHECK(items_grid_view_->app_list_config());

  // Clear any remaining state from the last time the folder was shown. E.g.
  // cancel any pending hide animations.
  ResetState(/*restore_folder_item_view_state=*/true);

  folder_item_view_ = folder_item_view;
  folder_item_view_observer_.Observe(folder_item_view);

  folder_item_ = static_cast<AppListFolderItem*>(folder_item_view->item());

  AppListModel* const model = AppListModelProvider::Get()->model();
  items_grid_view_->SetModel(model);
  items_grid_view_->SetItemList(folder_item_->item_list());
  folder_header_view_->SetFolderItem(folder_item_);

  model_observation_.Observe(model);

  UpdatePreferredBounds();
}

void AppListFolderView::ScheduleShowHideAnimation(bool show,
                                                  bool hide_for_reparent) {
  if (show)
    a11y_announcer_->AnnounceFolderOpened();
  else
    a11y_announcer_->AnnounceFolderClosed();

  show_hide_metrics_tracker_ =
      GetWidget()->GetCompositor()->RequestNewThroughputTracker();
  show_hide_metrics_tracker_->Start(
      metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
        UMA_HISTOGRAM_PERCENTAGE(
            "Apps.AppListFolder.ShowHide.AnimationSmoothness", smoothness);
      })));

  if (!features::IsProductivityLauncherEnabled()) {
    static_cast<PagedAppsGridView*>(items_grid_view_)
        ->pagination_model()
        ->SelectPage(0, false);
  }

  folder_visibility_animations_.clear();

  // Animate the background corner radius, opacity and bounds.
  folder_visibility_animations_.push_back(
      std::make_unique<BackgroundAnimation>(show, this, background_view_));

  // Animate the folder item's title's opacity.
  folder_visibility_animations_.push_back(
      std::make_unique<FolderItemTitleAnimation>(show, this,
                                                 folder_item_view_));

  // Animate the bounds and opacity of items in the first page of the opened
  // folder.
  folder_visibility_animations_.push_back(std::make_unique<TopIconAnimation>(
      show, this, scroll_view_, folder_item_view_));

  // Animate the bounds and opacity of the contents container.
  folder_visibility_animations_.push_back(
      std::make_unique<ContentsContainerAnimation>(show, hide_for_reparent,
                                                   this));

  base::RepeatingClosure animation_completion_callback;
  if (!show) {
    animation_completion_callback = base::BarrierClosure(
        folder_visibility_animations_.size(),
        base::BindOnce(&AppListFolderView::OnHideAnimationDone,
                       weak_ptr_factory_.GetWeakPtr(), hide_for_reparent));
  } else if (animation_done_test_callback_) {
    animation_completion_callback = base::BarrierClosure(
        folder_visibility_animations_.size(),
        base::BindOnce(&AppListFolderView::OnShowAnimationDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  for (auto& animation : folder_visibility_animations_)
    animation->ScheduleAnimation(animation_completion_callback);
}

void AppListFolderView::Layout() {
  views::View::Layout();

  if (gradient_helper_)
    gradient_helper_->UpdateGradientZone();

  // Position page switcher independently of the layout manager, as its
  // position does not fit with vertical layout alignment (it's expected to
  // float over the header view in the bottom right corner).
  if (page_switcher_) {
    const gfx::Size page_switcher_size = page_switcher_->GetPreferredSize();
    const gfx::Rect folder_header_bounds = folder_header_view_->bounds();
    const int page_switcher_x =
        folder_header_bounds.right() - page_switcher_size.width();
    // The page switcher has a different height than the folder header, but it
    // still needs to be aligned with it.
    const int page_switcher_y =
        folder_header_bounds.y() -
        (page_switcher_size.height() - folder_header_bounds.height()) / 2;
    page_switcher_->SetBoundsRect(gfx::Rect(
        gfx::Point(page_switcher_x, page_switcher_y), page_switcher_size));
  }
  // `BackgroundAnimation` animates the clip rect during open/close.
  if (!IsAnimationRunning()) {
    // The folder view can change size due to app install/uninstall. Ensure the
    // rounded corners have the correct position. https://crbug.com/993282
    background_view_->layer()->SetClipRect(background_view_->GetLocalBounds());
  }
}

void AppListFolderView::ChildPreferredSizeChanged(views::View* child) {
  UpdatePreferredBounds();
  PreferredSizeChanged();
}

void AppListFolderView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  // If the active model changed, close the folder view, as the backing app list
  // item is about to go away.
  if (folder_item_) {
    ResetState(/*restore_folder_item_view_state=*/false);

    folder_controller_->ShowApps(/*folder_item_view=*/nullptr,
                                 /*select_folder=*/false);
  }
}

void AppListFolderView::OnViewIsDeleting(views::View* view) {
  DCHECK_EQ(view, folder_item_view_);

  // If the original view got removed, clear any references to it, this includes
  // animations that may try to access the view to update its visibility.
  folder_visibility_animations_.clear();
  folder_item_view_observer_.Reset();
  folder_item_view_ = nullptr;
}

void AppListFolderView::OnAppListItemWillBeDeleted(AppListItem* item) {
  if (item == folder_item_) {
    ResetState(/*restore_folder_item_view_state=*/true);

    // If the folder item associated with this view is removed from the model,
    // (e.g. the last item in the folder was deleted), reset the view and signal
    // the container view to show the app list instead.
    // Pass nullptr to ShowApps() to avoid triggering animation from the deleted
    // folder.
    folder_controller_->ShowApps(/*folder_item_view=*/nullptr,
                                 /*select_folder=*/false);
  }
}

void AppListFolderView::ResetState(bool restore_folder_item_view_state) {
  DVLOG(1) << __FUNCTION__;
  if (folder_item_) {
    items_grid_view_->ClearSelectedView();
    items_grid_view_->SetItemList(nullptr);
    items_grid_view_->SetModel(nullptr);
    folder_header_view_->SetFolderItem(nullptr);
    folder_item_ = nullptr;
  }

  model_observation_.Reset();

  show_hide_metrics_tracker_.reset();

  // Clear in-progress animations, as they may depend on the
  // `folder_item_view_`.
  folder_visibility_animations_.clear();

  // Transition all the states immediately to the end of folder closing
  // animation.
  background_view_->layer()->SetColor(SK_ColorTRANSPARENT);
  if (restore_folder_item_view_state && folder_item_view_) {
    folder_item_view_->SetIconVisible(true);
    folder_item_view_->title()->SetEnabledColor(
        AppListColorProvider::Get()->GetAppListItemTextColor(
            /*is_in_folder=*/false));
  }

  folder_item_view_observer_.Reset();
  folder_item_view_ = nullptr;

  preferred_bounds_ = gfx::Rect();
  folder_item_icon_bounds_ = gfx::Rect();
}

void AppListFolderView::OnShowAnimationDone() {
  if (animation_done_test_callback_)
    std::move(animation_done_test_callback_).Run();
}

void AppListFolderView::OnHideAnimationDone(bool hide_for_reparent) {
  // If the folder view is hiding for folder closure, reset the
  // folder state when the animations complete. Not resetting state
  // immediately so the folder view keeps tracking folder item
  // view's liveness (so it can reset animations if the folder item
  // view gets deleted).
  // If the view is hidden for reparent, the state will be cleared
  // when the reparent drag ends.
  if (!hide_for_reparent) {
    ResetState(
        /*reset_folder_item_view_state=*/true);
  }

  if (animation_done_test_callback_)
    std::move(animation_done_test_callback_).Run();
}

void AppListFolderView::UpdatePreferredBounds() {
  if (!folder_item_view_)
    return;

  // Calculate the folder icon's bounds relative to our parent.
  gfx::RectF rect(folder_item_view_->GetIconBounds());
  ConvertRectToTarget(folder_item_view_, parent(), &rect);
  gfx::Rect icon_bounds_in_container =
      parent()->GetMirroredRect(gfx::ToEnclosingRect(rect));

  // The opened folder view's center should try to overlap with the folder
  // item's center while it must fit within the bounds of the parent.
  preferred_bounds_ = gfx::Rect(GetPreferredSize());
  preferred_bounds_ += (icon_bounds_in_container.CenterPoint() -
                        preferred_bounds_.CenterPoint());

  if (!bounding_box_.IsEmpty())
    preferred_bounds_.AdjustToFit(bounding_box_);

  // Calculate the folder icon's bounds relative to this view.
  folder_item_icon_bounds_ =
      icon_bounds_in_container - preferred_bounds_.OffsetFromOrigin();

  // Adjust folder item icon bounds for RTL (cannot use GetMirroredRect(), as
  // the current view bounds might not match the preferred bounds).
  if (base::i18n::IsRTL()) {
    folder_item_icon_bounds_.set_x(preferred_bounds_.width() -
                                   folder_item_icon_bounds_.x() -
                                   folder_item_icon_bounds_.width());
  }
}

int AppListFolderView::GetYOffsetForFolder() {
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (!keyboard_controller->IsEnabled())
    return 0;

  // This view should be on top of on-screen keyboard to prevent the folder
  // title from being blocked.
  const gfx::Rect occluded_bounds =
      keyboard_controller->GetWorkspaceOccludedBoundsInScreen();
  if (!occluded_bounds.IsEmpty()) {
    gfx::Point keyboard_top_right = occluded_bounds.top_right();
    ConvertPointFromScreen(parent(), &keyboard_top_right);

    // Our final Y-Offset is determined by combining the space from the bottom
    // of the folder to the top of the keyboard, and the padding that should
    // exist between the keyboard and the folder bottom.
    // std::min() is used so that positive offsets are ignored.
    return std::min(keyboard_top_right.y() - kOnscreenKeyboardTopPadding -
                        preferred_bounds_.bottom(),
                    0);
  }

  // If no offset is calculated above, then we need none.
  return 0;
}

bool AppListFolderView::IsAnimationRunning() const {
  for (auto& animation : folder_visibility_animations_) {
    if (animation->IsAnimationRunning())
      return true;
  }
  return false;
}

void AppListFolderView::SetBoundingBox(const gfx::Rect& bounding_box) {
  bounding_box_ = bounding_box;
  ShrinkGridTileMarginsWhenNeeded();
}

void AppListFolderView::SetAnimationDoneTestCallback(
    base::OnceClosure animation_done_callback) {
  DCHECK(!animation_done_callback || !animation_done_test_callback_);
  animation_done_test_callback_ = std::move(animation_done_callback);
}

void AppListFolderView::RecordAnimationSmoothness() {
  // RecordAnimationSmoothness is called when ContentsContainerAnimation
  // ends as well. Do not record show/hide metrics for that.
  if (show_hide_metrics_tracker_) {
    show_hide_metrics_tracker_->Stop();
    show_hide_metrics_tracker_.reset();
  }
}

void AppListFolderView::OnTabletModeChanged(bool started) {
  folder_header_view()->set_tablet_mode(started);
  if (page_switcher_)
    page_switcher_->set_is_tablet_mode(started);
}

void AppListFolderView::OnScrollEvent(ui::ScrollEvent* event) {
  items_grid_view_->HandleScrollFromParentView(
      gfx::Vector2d(event->x_offset(), event->y_offset()), event->type());
  event->SetHandled();
}

void AppListFolderView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSEWHEEL) {
    items_grid_view_->HandleScrollFromParentView(
        event->AsMouseWheelEvent()->offset(), ui::ET_MOUSEWHEEL);
    event->SetHandled();
  }
}

bool AppListFolderView::IsDragPointOutsideOfFolder(
    const gfx::Point& drag_point) {
  gfx::Point drag_point_in_folder = drag_point;
  views::View::ConvertPointToTarget(items_grid_view_, this,
                                    &drag_point_in_folder);
  return !GetLocalBounds().Contains(drag_point_in_folder);
}

// When user drags a folder item out of the folder boundary ink bubble, the
// folder view UI will be hidden, and switch back to top level AppsGridView.
// The dragged item will seamlessly move on the top level AppsGridView.
// In order to achieve the above, we keep the folder view and its child grid
// view visible with opacity 0, so that the drag_view_ on the hidden grid view
// will keep receiving mouse event. At the same time, we initiated a new
// drag_view_ in the top level grid view, and keep it moving with the hidden
// grid view's drag_view_, so that the dragged item can be engaged in drag and
// drop flow in the top level grid view. During the reparenting process, the
// drag_view_ in hidden grid view will dispatch the drag and drop event to
// the top level grid view, until the drag ends.
void AppListFolderView::ReparentItem(
    AppListItemView* original_drag_view,
    const gfx::Point& drag_point_in_folder_grid) {
  // Convert the drag point relative to the root level AppsGridView.
  gfx::Point to_root_level_grid = drag_point_in_folder_grid;
  ConvertPointToTarget(items_grid_view_, root_apps_grid_view_,
                       &to_root_level_grid);
  // Ensures the icon updates to reflect that the icon has been removed during
  // the drag
  folder_item_->NotifyOfDraggedItem(original_drag_view->item());
  root_apps_grid_view_->InitiateDragFromReparentItemInRootLevelGridView(
      original_drag_view, to_root_level_grid,
      base::BindOnce(&AppListFolderView::CancelReparentDragFromRootGrid,
                     weak_ptr_factory_.GetWeakPtr()));
  folder_controller_->ReparentFolderItemTransit(folder_item_);
}

void AppListFolderView::DispatchDragEventForReparent(
    AppsGridView::Pointer pointer,
    const gfx::Point& drag_point_in_folder_grid) {
  gfx::Point drag_point_in_root_grid = drag_point_in_folder_grid;
  // Temporarily reset the transform of the contents container so that the point
  // can be correctly converted to the root grid's coordinates.
  gfx::Transform original_transform = contents_container_->GetTransform();
  contents_container_->SetTransform(gfx::Transform());
  ConvertPointToTarget(items_grid_view_, root_apps_grid_view_,
                       &drag_point_in_root_grid);
  contents_container_->SetTransform(original_transform);

  root_apps_grid_view_->UpdateDragFromReparentItem(pointer,
                                                   drag_point_in_root_grid);
}

void AppListFolderView::DispatchEndDragEventForReparent(
    bool events_forwarded_to_drag_drop_host,
    bool cancel_drag,
    std::unique_ptr<AppDragIconProxy> drag_icon_proxy) {
  folder_item_->NotifyOfDraggedItem(nullptr);
  folder_controller_->ReparentDragEnded();

  // Cache `folder_item_view_`, as it will get reset in `HideViewImmediately()`.
  AppListItemView* const folder_item_view = folder_item_view_;

  // The view was not hidden in order to keeping receiving mouse events. Hide it
  // now as the reparenting ended.
  HideViewImmediately();

  root_apps_grid_view_->EndDragFromReparentItemInRootLevel(
      folder_item_view, events_forwarded_to_drag_drop_host, cancel_drag,
      std::move(drag_icon_proxy));
}

void AppListFolderView::HideViewImmediately() {
  SetVisible(false);
  ResetState(/*restore_folder_item_view_state=*/true);
}

void AppListFolderView::ResetItemsGridForClose() {
  if (items_grid_view()->has_dragged_item())
    items_grid_view()->CancelDragWithNoDropAnimation();
  items_grid_view()->ClearSelectedView();
}

void AppListFolderView::CloseFolderPage() {
  DVLOG(1) << __FUNCTION__;
  // When a folder closes only show the selection highlight if there was already
  // one showing.
  const bool select_folder = items_grid_view()->has_selected_view();
  ResetItemsGridForClose();
  folder_controller_->ShowApps(folder_item_view_, select_folder);
}

void AppListFolderView::FocusNameInput() {
  folder_header_view_->SetTextFocus();
}

void AppListFolderView::FocusFirstItem(bool silent) {
  DVLOG(1) << __FUNCTION__;
  AppListItemView* first_item_view =
      items_grid_view()->view_model()->view_at(0);
  if (silent) {
    first_item_view->SilentlyRequestFocus();
  } else {
    first_item_view->RequestFocus();
  }
}

bool AppListFolderView::IsOEMFolder() const {
  return folder_item_->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM;
}

void AppListFolderView::HandleKeyboardReparent(AppListItemView* reparented_view,
                                               ui::KeyboardCode key_code) {
  folder_controller_->ReparentFolderItemTransit(folder_item_);
  root_apps_grid_view_->HandleKeyboardReparent(reparented_view,
                                               folder_item_view_, key_code);
  folder_controller_->ReparentDragEnded();
  HideViewImmediately();
}

bool AppListFolderView::IsPointWithinPageFlipBuffer(
    const gfx::Point& point) const {
  // The page flip buffer is anywhere within the bounds of the
  // |contents_container_|.
  gfx::Point point_in_parent = point;
  ConvertPointToTarget(items_grid_view_, contents_container_, &point_in_parent);
  return GetContentsBounds().Contains(point_in_parent);
}

bool AppListFolderView::IsPointWithinBottomDragBuffer(
    const gfx::Point& point,
    int page_flip_zone_size) const {
  // Folders page horizontally and do not have a bottom drag buffer.
  return false;
}

void AppListFolderView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGenericContainer;
}

void AppListFolderView::OnGestureEvent(ui::GestureEvent* event) {
  // Capture scroll events so they don't bubble up to the apps container, where
  // they may cause the root apps grid view to scroll, or get translated into
  // apps grid view drag.
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN)
    event->SetHandled();
}

void AppListFolderView::SetItemName(AppListFolderItem* item,
                                    const std::string& name) {
  AppListModelProvider::Get()->model()->SetItemName(item, name);
}

const AppListConfig* AppListFolderView::GetAppListConfig() const {
  return items_grid_view_->app_list_config();
}

ui::Compositor* AppListFolderView::GetCompositor() {
  return GetWidget()->GetCompositor();
}

void AppListFolderView::CancelReparentDragFromRootGrid() {
  items_grid_view_->EndDrag(/*cancel=*/true);
}

void AppListFolderView::ShrinkGridTileMarginsWhenNeeded() {
  // Productivity launcher uses scrollable grid for folders, which handles the
  // case where the items grid does not fit into bounds provided by the folder
  // bounding box.
  if (features::IsProductivityLauncherEnabled())
    return;

  if (bounding_box_.IsEmpty() || !GetAppListConfig())
    return;

  // Calculate the expected folder height when it has the max possible number of
  // rows. The margins should be shrunk if this height does not fit within
  // the bounding box.
  const int max_folder_height =
      folder_header_view_->GetPreferredSize().height() + kFolderHeaderPadding +
      GetAppListConfig()->grid_tile_height() * kMaxPagedFolderRows +
      (kMaxPagedFolderRows - 1) * kTileSpacingInFolder;
  const bool shrink_margins = max_folder_height > bounding_box_.height();
  items_grid_view_->SetFixedTilePadding(
      (shrink_margins ? 0 : kTileSpacingInFolder) / 2,
      (shrink_margins ? 0 : kTileSpacingInFolder) / 2);
}

BEGIN_METADATA(AppListFolderView, views::View)
END_METADATA

}  // namespace ash
