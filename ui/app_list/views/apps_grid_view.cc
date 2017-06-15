// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/apps_grid_view.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/guid.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/pagination_controller.h"
#include "ui/app_list/views/app_list_drag_and_drop_host.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/apps_grid_view_delegate.h"
#include "ui/app_list/views/page_switcher_horizontal.h"
#include "ui/app_list/views/page_switcher_vertical.h"
#include "ui/app_list/views/pulsing_block_view.h"
#include "ui/app_list/views/top_icon_animation_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// Distance a drag needs to be from the app grid to be considered 'outside', at
// which point we rearrange the apps to their pre-drag configuration, as a drop
// then would be canceled. We have a buffer to make it easier to drag apps to
// other pages.
const int kDragBufferPx = 20;

// Padding space in pixels between pages.
const int kPagePadding = 40;

// Preferred tile size when showing in fixed layout.
const int kPreferredTileWidth = 100;
const int kPreferredTileHeight = 100;

// Padding on each side of a tile.
const int kTileLeftRightPadding = 10;
const int kTileBottomPadding = 6;
const int kTileTopPadding = 6;

// Width in pixels of the area on the sides that triggers a page flip.
const int kPageFlipZoneSize = 40;

// Delay in milliseconds to do the page flip.
const int kPageFlipDelayInMs = 1000;

// The drag and drop proxy should get scaled by this factor.
const float kDragAndDropProxyScale = 1.5f;

// Delays in milliseconds to show folder dropping preview circle.
const int kFolderDroppingDelay = 150;

// Delays in milliseconds to show re-order preview.
const int kReorderDelay = 120;

// Delays in milliseconds to show folder item reparent UI.
const int kFolderItemReparentDelay = 50;

// Radius of the circle, in which if entered, show folder dropping preview
// UI.
const int kFolderDroppingCircleRadius = 39;

// Returns the size of a tile view excluding its padding.
gfx::Size GetTileViewSize() {
  return gfx::Size(kPreferredTileWidth, kPreferredTileHeight);
}

// Returns the padding around a tile view.
gfx::Insets GetTilePadding() {
  return gfx::Insets(-kTileTopPadding, -kTileLeftRightPadding,
                     -kTileBottomPadding, -kTileLeftRightPadding);
}

// RowMoveAnimationDelegate is used when moving an item into a different row.
// Before running the animation, the item's layer is re-created and kept in
// the original position, then the item is moved to just before its target
// position and opacity set to 0. When the animation runs, this delegate moves
// the layer and fades it out while fading in the item at the same time.
class RowMoveAnimationDelegate : public gfx::AnimationDelegate {
 public:
  RowMoveAnimationDelegate(views::View* view,
                           ui::Layer* layer,
                           const gfx::Rect& layer_target)
      : view_(view),
        layer_(layer),
        layer_start_(layer ? layer->bounds() : gfx::Rect()),
        layer_target_(layer_target) {
  }
  ~RowMoveAnimationDelegate() override {}

  // gfx::AnimationDelegate overrides:
  void AnimationProgressed(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();

    if (layer_) {
      layer_->SetOpacity(1 - animation->GetCurrentValue());
      layer_->SetBounds(animation->CurrentValueBetween(layer_start_,
                                                       layer_target_));
      layer_->ScheduleDraw();
    }
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
    view_->SchedulePaint();
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1.0f);
    view_->SchedulePaint();
  }

 private:
  // The view that needs to be wrapped. Owned by views hierarchy.
  views::View* view_;

  std::unique_ptr<ui::Layer> layer_;
  const gfx::Rect layer_start_;
  const gfx::Rect layer_target_;

  DISALLOW_COPY_AND_ASSIGN(RowMoveAnimationDelegate);
};

// ItemRemoveAnimationDelegate is used to show animation for removing an item.
// This happens when user drags an item into a folder. The dragged item will
// be removed from the original list after it is dropped into the folder.
class ItemRemoveAnimationDelegate : public gfx::AnimationDelegate {
 public:
  explicit ItemRemoveAnimationDelegate(views::View* view)
      : view_(view) {
  }

  ~ItemRemoveAnimationDelegate() override {}

  // gfx::AnimationDelegate overrides:
  void AnimationProgressed(const gfx::Animation* animation) override {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }

 private:
  std::unique_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(ItemRemoveAnimationDelegate);
};

// ItemMoveAnimationDelegate observes when an item finishes animating when it is
// not moving between rows. This is to ensure an item is repainted for the
// "zoom out" case when releasing an item being dragged.
class ItemMoveAnimationDelegate : public gfx::AnimationDelegate {
 public:
  explicit ItemMoveAnimationDelegate(views::View* view) : view_(view) {}

  void AnimationEnded(const gfx::Animation* animation) override {
    view_->SchedulePaint();
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    view_->SchedulePaint();
  }

 private:
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(ItemMoveAnimationDelegate);
};

// Returns true if the |item| is a folder item.
bool IsFolderItem(AppListItem* item) {
  return (item->GetItemType() == AppListFolderItem::kItemType);
}

bool IsOEMFolderItem(AppListItem* item) {
  return IsFolderItem(item) &&
         (static_cast<AppListFolderItem*>(item))->folder_type() ==
             AppListFolderItem::FOLDER_TYPE_OEM;
}

int ClampToRange(int value, int min, int max) {
  return std::min(std::max(value, min), max);
}

}  // namespace

AppsGridView::AppsGridView(AppsGridViewDelegate* delegate)
    : model_(NULL),
      item_list_(NULL),
      delegate_(delegate),
      folder_delegate_(NULL),
      page_switcher_view_(NULL),
      cols_(0),
      rows_per_page_(0),
      selected_view_(NULL),
      drag_view_(NULL),
      drag_start_page_(-1),
      drag_pointer_(NONE),
      drop_attempt_(DROP_FOR_NONE),
      drag_and_drop_host_(NULL),
      forward_events_to_drag_and_drop_host_(false),
      page_flip_target_(-1),
      page_flip_delay_in_ms_(kPageFlipDelayInMs),
      bounds_animator_(this),
      activated_folder_item_view_(NULL),
      dragging_for_reparent_item_(false),
      is_fullscreen_app_list_enabled_(features::IsFullscreenAppListEnabled()) {
  SetPaintToLayer();
  // Clip any icons that are outside the grid view's bounds. These icons would
  // otherwise be visible to the user when the grid view is off screen.
  layer()->SetMasksToBounds(true);
  layer()->SetFillsBoundsOpaquely(false);

  pagination_model_.SetTransitionDurations(kPageTransitionDurationInMs,
                                           kOverscrollPageTransitionDurationMs);

  pagination_model_.AddObserver(this);

  if (is_fullscreen_app_list_enabled_) {
    page_switcher_view_ = new PageSwitcherVertical(&pagination_model_);
    pagination_controller_.reset(new PaginationController(
        &pagination_model_, PaginationController::SCROLL_AXIS_VERTICAL));
  } else {
    page_switcher_view_ = new PageSwitcherHorizontal(&pagination_model_);
    pagination_controller_.reset(new PaginationController(
        &pagination_model_, PaginationController::SCROLL_AXIS_HORIZONTAL));
  }
  AddChildView(page_switcher_view_);
}

AppsGridView::~AppsGridView() {
  // Coming here |drag_view_| should already be canceled since otherwise the
  // drag would disappear after the app list got animated away and closed,
  // which would look odd.
  DCHECK(!drag_view_);
  if (drag_view_)
    EndDrag(true);

  if (model_)
    model_->RemoveObserver(this);
  pagination_model_.RemoveObserver(this);

  if (item_list_)
    item_list_->RemoveObserver(this);

  // Make sure |page_switcher_view_| is deleted before |pagination_model_|.
  view_model_.Clear();
  RemoveAllChildViews(true);
}

void AppsGridView::SetLayout(int cols, int rows_per_page) {
  cols_ = cols;
  rows_per_page_ = rows_per_page;

  SetBorder(views::CreateEmptyBorder(0, kAppsGridPadding, 0, kAppsGridPadding));
}

// static
gfx::Size AppsGridView::GetTotalTileSize() {
  gfx::Rect rect(GetTileViewSize());
  rect.Inset(GetTilePadding());
  return rect.size();
}

void AppsGridView::ResetForShowApps() {
  activated_folder_item_view_ = NULL;
  ClearDragState();
  layer()->SetOpacity(1.0f);
  SetVisible(true);
  // Set all views to visible in case they weren't made visible again by an
  // incomplete animation.
  for (int i = 0; i < view_model_.view_size(); ++i) {
    view_model_.view_at(i)->SetVisible(true);
  }
  CHECK_EQ(item_list_->item_count(),
           static_cast<size_t>(view_model_.view_size()));
}

void AppsGridView::SetModel(AppListModel* model) {
  if (model_)
    model_->RemoveObserver(this);

  model_ = model;
  if (model_)
    model_->AddObserver(this);

  Update();
}

void AppsGridView::SetItemList(AppListItemList* item_list) {
  if (item_list_)
    item_list_->RemoveObserver(this);
  item_list_ = item_list;
  if (item_list_)
    item_list_->AddObserver(this);
  Update();
}

void AppsGridView::SetSelectedView(AppListItemView* view) {
  if (IsSelectedView(view) || IsDraggedView(view))
    return;

  Index index = GetIndexOfView(view);
  if (IsValidIndex(index))
    SetSelectedItemByIndex(index);
}

void AppsGridView::ClearSelectedView(AppListItemView* view) {
  if (view && IsSelectedView(view)) {
    selected_view_->SchedulePaint();
    selected_view_ = NULL;
  }
}

void AppsGridView::ClearAnySelectedView() {
  if (selected_view_) {
    selected_view_->SchedulePaint();
    selected_view_ = NULL;
  }
}

bool AppsGridView::IsSelectedView(const AppListItemView* view) const {
  return selected_view_ == view;
}

void AppsGridView::InitiateDrag(AppListItemView* view,
                                Pointer pointer,
                                const ui::LocatedEvent& event) {
  DCHECK(view);
  if (drag_view_ || pulsing_blocks_model_.view_size())
    return;

  drag_view_ = view;
  drag_view_init_index_ = GetIndexOfView(drag_view_);
  drag_view_offset_ = event.location();
  drag_start_page_ = pagination_model_.selected_page();
  reorder_placeholder_ = drag_view_init_index_;
  ExtractDragLocation(event, &drag_start_grid_view_);
  drag_view_start_ = gfx::Point(drag_view_->x(), drag_view_->y());
}

bool AppsGridView::UpdateDragFromItem(Pointer pointer,
                                      const ui::LocatedEvent& event) {
  if (!drag_view_)
    return false;  // Drag canceled.

  gfx::Point drag_point_in_grid_view;
  ExtractDragLocation(event, &drag_point_in_grid_view);
  UpdateDrag(pointer, drag_point_in_grid_view);
  if (!dragging())
    return false;

  // If a drag and drop host is provided, see if the drag operation needs to be
  // forwarded.
  gfx::Point location_in_screen = drag_point_in_grid_view;
  views::View::ConvertPointToScreen(this, &location_in_screen);
  DispatchDragEventToDragAndDropHost(location_in_screen);
  if (drag_and_drop_host_)
    drag_and_drop_host_->UpdateDragIconProxy(location_in_screen);
  return true;
}

void AppsGridView::UpdateDrag(Pointer pointer, const gfx::Point& point) {
  if (folder_delegate_)
    UpdateDragStateInsideFolder(pointer, point);

  if (!drag_view_)
    return;  // Drag canceled.

  gfx::Vector2d drag_vector(point - drag_start_grid_view_);
  if (!dragging() && ExceededDragThreshold(drag_vector)) {
    drag_pointer_ = pointer;
    // Move the view to the front so that it appears on top of other views.
    ReorderChildView(drag_view_, -1);
    bounds_animator_.StopAnimatingView(drag_view_);
    // Stopping the animation may have invalidated our drag view due to the
    // view hierarchy changing.
    if (!drag_view_)
      return;

    if (!dragging_for_reparent_item_)
      StartDragAndDropHostDrag(point);
  }

  if (drag_pointer_ != pointer)
    return;

  drag_view_->SetPosition(drag_view_start_ + drag_vector);

  last_drag_point_ = point;
  const Index last_reorder_drop_target = reorder_drop_target_;
  const Index last_folder_drop_target = folder_drop_target_;
  DropAttempt last_drop_attempt = drop_attempt_;
  CalculateDropTarget();

  MaybeStartPageFlipTimer(last_drag_point_);

  gfx::Point page_switcher_point(last_drag_point_);
  views::View::ConvertPointToTarget(this, page_switcher_view_,
                                    &page_switcher_point);
  page_switcher_view_->UpdateUIForDragPoint(page_switcher_point);

  if (last_folder_drop_target != folder_drop_target_ ||
      last_reorder_drop_target != reorder_drop_target_ ||
      last_drop_attempt != drop_attempt_) {
    if (drop_attempt_ == DROP_FOR_REORDER) {
      folder_dropping_timer_.Stop();
      reorder_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kReorderDelay),
          this, &AppsGridView::OnReorderTimer);
    } else if (drop_attempt_ == DROP_FOR_FOLDER) {
      reorder_timer_.Stop();
      folder_dropping_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kFolderDroppingDelay),
          this, &AppsGridView::OnFolderDroppingTimer);
    }

    // Reset the previous drop target.
    SetAsFolderDroppingTarget(last_folder_drop_target, false);
  }
}

void AppsGridView::EndDrag(bool cancel) {
  // EndDrag was called before if |drag_view_| is NULL.
  if (!drag_view_)
    return;

  // Coming here a drag and drop was in progress.
  bool landed_in_drag_and_drop_host = forward_events_to_drag_and_drop_host_;
  if (forward_events_to_drag_and_drop_host_) {
    DCHECK(!IsDraggingForReparentInRootLevelGridView());
    forward_events_to_drag_and_drop_host_ = false;
    drag_and_drop_host_->EndDrag(cancel);
    if (IsDraggingForReparentInHiddenGridView()) {
      folder_delegate_->DispatchEndDragEventForReparent(
          true /* events_forwarded_to_drag_drop_host */,
          cancel /* cancel_drag */);
    }
  } else {
    if (IsDraggingForReparentInHiddenGridView()) {
      // Forward the EndDrag event to the root level grid view.
      folder_delegate_->DispatchEndDragEventForReparent(
          false /* events_forwarded_to_drag_drop_host */,
          cancel /* cancel_drag */);
      EndDragForReparentInHiddenFolderGridView();
      return;
    }

    if (IsDraggingForReparentInRootLevelGridView()) {
      // An EndDrag can be received during a reparent via a model change. This
      // is always a cancel and needs to be forwarded to the folder.
      DCHECK(cancel);
      delegate_->CancelDragInActiveFolder();
      return;
    }

    if (!cancel && dragging()) {
      // Regular drag ending path, ie, not for reparenting.
      CalculateDropTarget();
      if (EnableFolderDragDropUI() && drop_attempt_ == DROP_FOR_FOLDER &&
          IsValidIndex(folder_drop_target_)) {
        MoveItemToFolder(drag_view_, folder_drop_target_);
      } else if (IsValidIndex(reorder_drop_target_)) {
        MoveItemInModel(drag_view_, reorder_drop_target_);
      }
    }
  }

  if (drag_and_drop_host_) {
    // If we had a drag and drop proxy icon, we delete it and make the real
    // item visible again.
    drag_and_drop_host_->DestroyDragIconProxy();
    // Issue 439055: MoveItemToFolder() can sometimes delete |drag_view_|
    if (drag_view_) {
      if (landed_in_drag_and_drop_host) {
        // Move the item directly to the target location, avoiding the
        // "zip back" animation if the user was pinning it to the shelf.
        int i = reorder_drop_target_.slot;
        gfx::Rect bounds = view_model_.ideal_bounds(i);
        drag_view_->SetBoundsRect(bounds);
      }
      // Fade in slowly if it landed in the shelf.
      SetViewHidden(drag_view_, false /* show */,
                    !landed_in_drag_and_drop_host /* animate */);
    }
  }

  SetAsFolderDroppingTarget(folder_drop_target_, false);
  ClearDragState();
  AnimateToIdealBounds();

  StopPageFlipTimer();

  // If user releases mouse inside a folder's grid view, burst the folder
  // container ink bubble.
  if (folder_delegate_ && !IsDraggingForReparentInHiddenGridView())
    folder_delegate_->UpdateFolderViewBackground(false);
}

void AppsGridView::StopPageFlipTimer() {
  page_flip_timer_.Stop();
  page_flip_target_ = -1;
}

AppListItemView* AppsGridView::GetItemViewAt(int index) const {
  return view_model_.view_at(index);
}

void AppsGridView::SetTopItemViewsVisible(bool visible) {
  int top_item_count = std::min(static_cast<int>(kNumFolderTopItems),
                                view_model_.view_size());
  for (int i = 0; i < top_item_count; ++i)
    GetItemViewAt(i)->icon()->SetVisible(visible);
}

void AppsGridView::ScheduleShowHideAnimation(bool show) {
  // Stop any previous animation.
  layer()->GetAnimator()->StopAnimating();

  // Set initial state.
  SetVisible(true);
  layer()->SetOpacity(show ? 0.0f : 1.0f);

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.AddObserver(this);
  animation.SetTweenType(
      show ? kFolderFadeInTweenType : kFolderFadeOutTweenType);
  animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      show ? kFolderTransitionInDurationMs : kFolderTransitionOutDurationMs));

  layer()->SetOpacity(show ? 1.0f : 0.0f);
}

void AppsGridView::InitiateDragFromReparentItemInRootLevelGridView(
    AppListItemView* original_drag_view,
    const gfx::Rect& drag_view_rect,
    const gfx::Point& drag_point,
    bool has_native_drag) {
  DCHECK(original_drag_view && !drag_view_);
  DCHECK(!dragging_for_reparent_item_);

  // Since the item is new, its placeholder is conceptually at the back of the
  // entire apps grid.
  reorder_placeholder_ = GetLastViewIndex();

  // Create a new AppListItemView to duplicate the original_drag_view in the
  // folder's grid view.
  AppListItemView* view = new AppListItemView(this, original_drag_view->item());
  AddChildView(view);
  drag_view_ = view;
  drag_view_->SetPaintToLayer();
  drag_view_->layer()->SetFillsBoundsOpaquely(false);
  drag_view_->SetBoundsRect(drag_view_rect);
  drag_view_->SetDragUIState();  // Hide the title of the drag_view_.

  // Hide the drag_view_ for drag icon proxy when a native drag is responsible
  // for showing the icon.
  if (has_native_drag)
    SetViewHidden(drag_view_, true /* hide */, true /* no animate */);

  // Add drag_view_ to the end of the view_model_.
  view_model_.Add(drag_view_, view_model_.view_size());

  drag_start_page_ = pagination_model_.selected_page();
  drag_start_grid_view_ = drag_point;

  drag_view_start_ = gfx::Point(drag_view_->x(), drag_view_->y());

  // Set the flag in root level grid view.
  dragging_for_reparent_item_ = true;
}

void AppsGridView::UpdateDragFromReparentItem(Pointer pointer,
                                              const gfx::Point& drag_point) {
  // Note that if a cancel ocurrs while reparenting, the |drag_view_| in both
  // root and folder grid views is cleared, so the check in UpdateDragFromItem()
  // for |drag_view_| being NULL (in the folder grid) is sufficient.
  DCHECK(drag_view_);
  DCHECK(IsDraggingForReparentInRootLevelGridView());

  UpdateDrag(pointer, drag_point);
}

bool AppsGridView::IsDraggedView(const AppListItemView* view) const {
  return drag_view_ == view;
}

void AppsGridView::ClearDragState() {
  drop_attempt_ = DROP_FOR_NONE;
  drag_pointer_ = NONE;
  reorder_drop_target_ = Index();
  folder_drop_target_ = Index();
  reorder_placeholder_ = Index();
  drag_start_grid_view_ = gfx::Point();
  drag_start_page_ = -1;
  drag_view_offset_ = gfx::Point();

  if (drag_view_) {
    drag_view_->OnDragEnded();
    if (IsDraggingForReparentInRootLevelGridView()) {
      const int drag_view_index = view_model_.GetIndexOfView(drag_view_);
      CHECK_EQ(view_model_.view_size() - 1, drag_view_index);
      DeleteItemViewAtIndex(drag_view_index);
    }
  }
  drag_view_ = NULL;
  dragging_for_reparent_item_ = false;
}

void AppsGridView::SetDragViewVisible(bool visible) {
  DCHECK(drag_view_);
  SetViewHidden(drag_view_, !visible, true);
}

void AppsGridView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  drag_and_drop_host_ = drag_and_drop_host;
}

bool AppsGridView::IsAnimatingView(AppListItemView* view) {
  return bounds_animator_.IsAnimating(view);
}

gfx::Size AppsGridView::CalculatePreferredSize() const {
  const gfx::Insets insets(GetInsets());
  gfx::Size size = GetTileGridSize();
  if (is_fullscreen_app_list_enabled_) {
    // If we are in a folder, ignore the page switcher for width calculations.
    int page_switcher_width =
        folder_delegate_ ? 0 : page_switcher_view_->GetPreferredSize().width();
    size.Enlarge(insets.width() + page_switcher_width, insets.height());
  } else {
    // If we are in a folder, ignore the page switcher for height calculations.
    int page_switcher_height =
        folder_delegate_ ? 0 : page_switcher_view_->GetPreferredSize().height();
    size.Enlarge(insets.width(), insets.height() + page_switcher_height);
  }
  return size;
}

bool AppsGridView::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  // TODO(koz): Only accept a specific drag type for app shortcuts.
  *formats = OSExchangeData::FILE_NAME;
  return true;
}

bool AppsGridView::CanDrop(const OSExchangeData& data) {
  return true;
}

int AppsGridView::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

void AppsGridView::Layout() {
  if (bounds_animator_.IsAnimating())
    bounds_animator_.Cancel();

  CalculateIdealBounds();
  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view != drag_view_)
      view->SetBoundsRect(view_model_.ideal_bounds(i));
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model_);

  gfx::Rect rect(GetContentsBounds());
  if (is_fullscreen_app_list_enabled_) {
    const int page_switcher_width =
        page_switcher_view_->GetPreferredSize().width();
    rect.set_x(rect.right() - page_switcher_width);
    rect.set_width(page_switcher_width);
  } else {
    const int page_switcher_height =
        page_switcher_view_->GetPreferredSize().height();
    rect.set_y(rect.bottom() - page_switcher_height);
    rect.set_height(page_switcher_height);
  }
  page_switcher_view_->SetBoundsRect(rect);
}

bool AppsGridView::OnKeyPressed(const ui::KeyEvent& event) {
  bool handled = false;
  if (selected_view_)
    handled = static_cast<views::View*>(selected_view_)->OnKeyPressed(event);

  if (!handled) {
    const int forward_dir = base::i18n::IsRTL() ? -1 : 1;
    switch (event.key_code()) {
      case ui::VKEY_LEFT:
        MoveSelected(0, -forward_dir, 0);
        return true;
      case ui::VKEY_RIGHT:
        MoveSelected(0, forward_dir, 0);
        return true;
      case ui::VKEY_UP:
        if (selected_view_)  // Don't initiate selection with UP
          MoveSelected(0, 0, -1);
        return true;
      case ui::VKEY_DOWN:
        MoveSelected(0, 0, 1);
        return true;
      case ui::VKEY_PRIOR: {
        MoveSelected(-1, 0, 0);
        return true;
      }
      case ui::VKEY_NEXT: {
        MoveSelected(1, 0, 0);
        return true;
      }
      case ui::VKEY_TAB: {
        if (event.IsShiftDown()) {
          ClearAnySelectedView();  // ContentsView will move focus back.
        } else {
          MoveSelected(0, 0, 0);  // Ensure but don't change selection.
          handled = true;         // TABing internally doesn't move focus.
        }
        break;
      }
      default:
        break;
    }
  }

  return handled;
}

bool AppsGridView::OnKeyReleased(const ui::KeyEvent& event) {
  bool handled = false;
  if (selected_view_)
    handled = selected_view_->OnKeyReleased(event);

  return handled;
}

bool AppsGridView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  return pagination_controller_->OnScroll(
      gfx::Vector2d(event.x_offset(), event.y_offset()),
      PaginationController::SCROLL_MOUSE_WHEEL);
}

void AppsGridView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!details.is_add && details.parent == this) {
    // The view being delete should not have reference in |view_model_|.
    CHECK_EQ(-1, view_model_.GetIndexOfView(details.child));

    if (selected_view_ == details.child)
      selected_view_ = NULL;
    if (activated_folder_item_view_ == details.child)
      activated_folder_item_view_ = NULL;

    if (drag_view_ == details.child)
      EndDrag(true);

    bounds_animator_.StopAnimatingView(details.child);
  }
}

void AppsGridView::OnGestureEvent(ui::GestureEvent* event) {
  if (pagination_controller_->OnGestureEvent(*event, GetContentsBounds()))
    event->SetHandled();
}

void AppsGridView::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() == ui::ET_SCROLL_FLING_CANCEL)
    return;

  gfx::Vector2dF offset(event->x_offset(), event->y_offset());
  if (pagination_controller_->OnScroll(gfx::ToFlooredVector2d(offset),
                                       PaginationController::SCROLL_TOUCHPAD)) {
    event->SetHandled();
    event->StopPropagation();
  }
}

void AppsGridView::Update() {
  DCHECK(!selected_view_ && !drag_view_);
  view_model_.Clear();
  if (!item_list_ || !item_list_->item_count())
    return;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    AppListItemView* view = CreateViewForItemAtIndex(i);
    view_model_.Add(view, i);
    AddChildView(view);
  }
  UpdatePaging();
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();
}

void AppsGridView::UpdatePaging() {
  int total_page = view_model_.view_size() && tiles_per_page()
                       ? (view_model_.view_size() - 1) / tiles_per_page() + 1
                       : 0;

  pagination_model_.SetTotalPages(total_page);
}

void AppsGridView::UpdatePulsingBlockViews() {
  const int existing_items = item_list_ ? item_list_->item_count() : 0;
  const int available_slots =
      tiles_per_page() - existing_items % tiles_per_page();
  const int desired = model_->status() == AppListModel::STATUS_SYNCING ?
      available_slots : 0;

  if (pulsing_blocks_model_.view_size() == desired)
    return;

  while (pulsing_blocks_model_.view_size() > desired) {
    PulsingBlockView* view = pulsing_blocks_model_.view_at(0);
    pulsing_blocks_model_.Remove(0);
    delete view;
  }

  while (pulsing_blocks_model_.view_size() < desired) {
    PulsingBlockView* view = new PulsingBlockView(GetTotalTileSize(), true);
    pulsing_blocks_model_.Add(view, 0);
    AddChildView(view);
  }
}

AppListItemView* AppsGridView::CreateViewForItemAtIndex(size_t index) {
  // The drag_view_ might be pending for deletion, therefore view_model_
  // may have one more item than item_list_.
  DCHECK_LE(index, item_list_->item_count());
  AppListItemView* view = new AppListItemView(this,
                                              item_list_->item_at(index));
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
  return view;
}

AppsGridView::Index AppsGridView::GetIndexFromModelIndex(
    int model_index) const {
  return Index(model_index / tiles_per_page(), model_index % tiles_per_page());
}

int AppsGridView::GetModelIndexFromIndex(const Index& index) const {
  return index.page * tiles_per_page() + index.slot;
}

void AppsGridView::EnsureViewVisible(const Index& index) {
  if (pagination_model_.has_transition())
    return;

  if (IsValidIndex(index))
    pagination_model_.SelectPage(index.page, false);
}

void AppsGridView::SetSelectedItemByIndex(const Index& index) {
  if (GetIndexOfView(selected_view_) == index)
    return;

  AppListItemView* new_selection = GetViewAtIndex(index);
  if (!new_selection)
    return;  // Keep current selection.

  if (selected_view_)
    selected_view_->SchedulePaint();

  EnsureViewVisible(index);
  selected_view_ = new_selection;
  selected_view_->SetTitleSubpixelAA();
  selected_view_->SchedulePaint();
  selected_view_->NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION, true);
}

bool AppsGridView::IsValidIndex(const Index& index) const {
  return index.page >= 0 && index.page < pagination_model_.total_pages() &&
         index.slot >= 0 && index.slot < tiles_per_page() &&
         GetModelIndexFromIndex(index) < view_model_.view_size();
}

AppsGridView::Index AppsGridView::GetIndexOfView(
    const AppListItemView* view) const {
  const int model_index = view_model_.GetIndexOfView(view);
  if (model_index == -1)
    return Index();

  return GetIndexFromModelIndex(model_index);
}

AppListItemView* AppsGridView::GetViewAtIndex(const Index& index) const {
  if (!IsValidIndex(index))
    return NULL;

  const int model_index = GetModelIndexFromIndex(index);
  return GetItemViewAt(model_index);
}

AppsGridView::Index AppsGridView::GetLastViewIndex() const {
  DCHECK_LT(0, view_model_.view_size());
  int view_index = view_model_.view_size() - 1;
  return Index(view_index / tiles_per_page(), view_index % tiles_per_page());
}

void AppsGridView::MoveSelected(int page_delta,
                                int slot_x_delta,
                                int slot_y_delta) {
  if (!selected_view_)
    return SetSelectedItemByIndex(Index(pagination_model_.selected_page(), 0));

  const Index& selected = GetIndexOfView(selected_view_);
  int target_slot = selected.slot + slot_x_delta + slot_y_delta * cols_;

  if (selected.slot % cols_ == 0 && slot_x_delta == -1) {
    if (selected.page > 0) {
      page_delta = -1;
      target_slot = selected.slot + cols_ - 1;
    } else {
      target_slot = selected.slot;
    }
  }

  if (selected.slot % cols_ == cols_ - 1 && slot_x_delta == 1) {
    if (selected.page < pagination_model_.total_pages() - 1) {
      page_delta = 1;
      target_slot = selected.slot - cols_ + 1;
    } else {
      target_slot = selected.slot;
    }
  }

  // Clamp the target slot to the last item if we are moving to the last page
  // but our target slot is past the end of the item list.
  if (page_delta &&
      selected.page + page_delta == pagination_model_.total_pages() - 1) {
    int last_item_slot = (view_model_.view_size() - 1) % tiles_per_page();
    if (last_item_slot < target_slot) {
      target_slot = last_item_slot;
    }
  }

  int target_page = std::min(pagination_model_.total_pages() - 1,
                             std::max(selected.page + page_delta, 0));
  SetSelectedItemByIndex(Index(target_page, target_slot));
}

void AppsGridView::CalculateIdealBounds() {
  gfx::Size grid_size = GetTileGridSize();

  // Page size including padding pixels. A tile.x + page_width means the same
  // tile slot in the next page; similarly for tile.y + page_height.
  const int page_width = grid_size.width() + kPagePadding;
  const int page_height = grid_size.height() + kPagePadding;

  // If there is a transition, calculates offset for current and target page.
  const int current_page = pagination_model_.selected_page();
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  const bool is_valid = pagination_model_.is_valid_page(transition.target_page);

  // Transition to previous page means negative offset.
  const int dir = transition.target_page > current_page ? -1 : 1;

  const int total_views =
      view_model_.view_size() + pulsing_blocks_model_.view_size();
  int slot_index = 0;
  for (int i = 0; i < total_views; ++i) {
    if (i < view_model_.view_size() && view_model_.view_at(i) == drag_view_)
      continue;

    Index view_index = GetIndexFromModelIndex(slot_index);

    // Leaves a blank space in the grid for the current reorder placeholder.
    if (reorder_placeholder_ == view_index) {
      ++slot_index;
      view_index = GetIndexFromModelIndex(slot_index);
    }

    // Decide the x or y offset for current item.
    int x_offset = 0;
    int y_offset = 0;

    if (pagination_controller_->scroll_axis() ==
        PaginationController::SCROLL_AXIS_HORIZONTAL) {
      if (view_index.page < current_page)
        x_offset = -page_width;
      else if (view_index.page > current_page)
        x_offset = page_width;

      if (is_valid) {
        if (view_index.page == current_page ||
            view_index.page == transition.target_page) {
          x_offset += transition.progress * page_width * dir;
        }
      }
    } else {
      if (view_index.page < current_page)
        y_offset = -page_height;
      else if (view_index.page > current_page)
        y_offset = page_height;

      if (is_valid) {
        if (view_index.page == current_page ||
            view_index.page == transition.target_page) {
          y_offset += transition.progress * page_height * dir;
        }
      }
    }

    const int row = view_index.slot / cols_;
    const int col = view_index.slot % cols_;
    gfx::Rect tile_slot = GetExpectedTileBounds(row, col);
    tile_slot.Offset(x_offset, y_offset);
    if (i < view_model_.view_size()) {
      view_model_.set_ideal_bounds(i, tile_slot);
    } else {
      pulsing_blocks_model_.set_ideal_bounds(i - view_model_.view_size(),
                                             tile_slot);
    }

    ++slot_index;
  }
}

void AppsGridView::AnimateToIdealBounds() {
  const gfx::Rect visible_bounds(GetVisibleBounds());

  CalculateIdealBounds();
  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view == drag_view_)
      continue;

    const gfx::Rect& target = view_model_.ideal_bounds(i);
    if (bounds_animator_.GetTargetBounds(view) == target)
      continue;

    const gfx::Rect& current = view->bounds();
    const bool current_visible = visible_bounds.Intersects(current);
    const bool target_visible = visible_bounds.Intersects(target);
    const bool visible = current_visible || target_visible;

    const int y_diff = target.y() - current.y();
    if (visible && y_diff && y_diff % GetTotalTileSize().height() == 0) {
      AnimationBetweenRows(view,
                           current_visible,
                           current,
                           target_visible,
                           target);
    } else if (visible || bounds_animator_.IsAnimating(view)) {
      bounds_animator_.AnimateViewTo(view, target);
      bounds_animator_.SetAnimationDelegate(
          view, std::unique_ptr<gfx::AnimationDelegate>(
                    new ItemMoveAnimationDelegate(view)));
    } else {
      view->SetBoundsRect(target);
    }
  }
}

void AppsGridView::AnimationBetweenRows(AppListItemView* view,
                                        bool animate_current,
                                        const gfx::Rect& current,
                                        bool animate_target,
                                        const gfx::Rect& target) {
  // Determine page of |current| and |target|. -1 means in the left invisible
  // page, 0 is the center visible page and 1 means in the right invisible page.
  const int current_page = current.x() < 0 ? -1 :
      current.x() >= width() ? 1 : 0;
  const int target_page = target.x() < 0 ? -1 :
      target.x() >= width() ? 1 : 0;

  const int dir = current_page < target_page ||
      (current_page == target_page && current.y() < target.y()) ? 1 : -1;

  std::unique_ptr<ui::Layer> layer;
  if (animate_current) {
    layer = view->RecreateLayer();
    layer->SuppressPaint();

    view->layer()->SetFillsBoundsOpaquely(false);
    view->layer()->SetOpacity(0.f);
  }

  gfx::Size total_tile_size = GetTotalTileSize();
  gfx::Rect current_out(current);
  current_out.Offset(dir * total_tile_size.width(), 0);

  gfx::Rect target_in(target);
  if (animate_target)
    target_in.Offset(-dir * total_tile_size.width(), 0);
  view->SetBoundsRect(target_in);
  bounds_animator_.AnimateViewTo(view, target);

  bounds_animator_.SetAnimationDelegate(
      view,
      std::unique_ptr<gfx::AnimationDelegate>(
          new RowMoveAnimationDelegate(view, layer.release(), current_out)));
}

void AppsGridView::ExtractDragLocation(const ui::LocatedEvent& event,
                                       gfx::Point* drag_point) {
  // Use root location of |event| instead of location in |drag_view_|'s
  // coordinates because |drag_view_| has a scale transform and location
  // could have integer round error and causes jitter.
  *drag_point = event.root_location();

  // GetWidget() could be NULL for tests.
  if (GetWidget()) {
    aura::Window::ConvertPointToTarget(
        GetWidget()->GetNativeWindow()->GetRootWindow(),
        GetWidget()->GetNativeWindow(),
        drag_point);
  }

  views::View::ConvertPointFromWidget(this, drag_point);
}

void AppsGridView::CalculateDropTarget() {
  DCHECK(drag_view_);

  gfx::Point point = drag_view_->icon()->bounds().CenterPoint();
  views::View::ConvertPointToTarget(drag_view_, this, &point);
  if (!IsPointWithinDragBuffer(point)) {
    // Reset the reorder target to the original position if the cursor is
    // outside the drag buffer.
    if (IsDraggingForReparentInRootLevelGridView()) {
      drop_attempt_ = DROP_FOR_NONE;
      return;
    }

    reorder_drop_target_ = drag_view_init_index_;
    drop_attempt_ = DROP_FOR_REORDER;
    return;
  }

  if (EnableFolderDragDropUI() &&
      CalculateFolderDropTarget(point, &folder_drop_target_)) {
    drop_attempt_ = DROP_FOR_FOLDER;
    return;
  }

  drop_attempt_ = DROP_FOR_REORDER;
  CalculateReorderDropTarget(point, &reorder_drop_target_);
}

bool AppsGridView::CalculateFolderDropTarget(const gfx::Point& point,
                                             Index* drop_target) const {
  // Folders can't be dropped into other folders.
  if (IsFolderItem(drag_view_->item()))
    return false;

  // A folder drop shouldn't happen on the reorder placeholder since that would
  // be merging an item with itself.
  Index nearest_tile_index(GetNearestTileIndexForPoint(point));
  if (!IsValidIndex(nearest_tile_index) ||
      nearest_tile_index == reorder_placeholder_) {
    return false;
  }

  int distance_to_tile_center =
      (point - GetExpectedTileBounds(nearest_tile_index.slot).CenterPoint())
          .Length();
  if (distance_to_tile_center > kFolderDroppingCircleRadius)
    return false;

  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(nearest_tile_index.slot);
  if (!target_view)
    return false;

  AppListItem* target_item = target_view->item();

  // Items can only be dropped into non-folders (which have no children) or
  // folders that have fewer than the max allowed items.
  // The OEM folder does not allow drag/drop of other items into it.
  if (target_item->ChildItemCount() >= kMaxFolderItems ||
      IsOEMFolderItem(target_item)) {
    return false;
  }

  *drop_target = nearest_tile_index;
  DCHECK(IsValidIndex(*drop_target));
  return true;
}

void AppsGridView::CalculateReorderDropTarget(const gfx::Point& point,
                                              Index* drop_target) const {
  gfx::Rect bounds = GetContentsBounds();
  Index grid_index = GetNearestTileIndexForPoint(point);
  gfx::Point reorder_placeholder_center =
      GetExpectedTileBounds(reorder_placeholder_.slot).CenterPoint();

  int x_offset_direction = 0;
  if (grid_index == reorder_placeholder_) {
    x_offset_direction = reorder_placeholder_center.x() < point.x() ? -1 : 1;
  } else {
    x_offset_direction = reorder_placeholder_ < grid_index ? -1 : 1;
  }

  gfx::Size total_tile_size = GetTotalTileSize();
  int row = grid_index.slot / cols_;

  // Offset the target column based on the direction of the target. This will
  // result in earlier targets getting their reorder zone shifted backwards
  // and later targets getting their reorder zones shifted forwards.
  //
  // This makes eordering feel like the user is slotting items into the spaces
  // between apps.
  int x_offset = x_offset_direction *
                 (total_tile_size.width() - kFolderDroppingCircleRadius) / 2;
  int col = (point.x() - bounds.x() + x_offset) / total_tile_size.width();
  col = ClampToRange(col, 0, cols_ - 1);
  *drop_target =
      std::min(Index(pagination_model_.selected_page(), row * cols_ + col),
               GetLastViewIndex());
  DCHECK(IsValidIndex(*drop_target));
}

void AppsGridView::OnReorderTimer() {
  if (drop_attempt_ == DROP_FOR_REORDER) {
    reorder_placeholder_ = reorder_drop_target_;
    AnimateToIdealBounds();
  }
}

void AppsGridView::OnFolderItemReparentTimer() {
  DCHECK(folder_delegate_);
  if (drag_out_of_folder_container_ && drag_view_) {
    bool has_native_drag = drag_and_drop_host_ != nullptr;
    folder_delegate_->ReparentItem(
        drag_view_, last_drag_point_, has_native_drag);

    // Set the flag in the folder's grid view.
    dragging_for_reparent_item_ = true;

    // Do not observe any data change since it is going to be hidden.
    item_list_->RemoveObserver(this);
    item_list_ = NULL;
  }
}

void AppsGridView::OnFolderDroppingTimer() {
  if (drop_attempt_ == DROP_FOR_FOLDER)
    SetAsFolderDroppingTarget(folder_drop_target_, true);
}

void AppsGridView::UpdateDragStateInsideFolder(Pointer pointer,
                                               const gfx::Point& drag_point) {
  if (IsUnderOEMFolder())
    return;

  if (IsDraggingForReparentInHiddenGridView()) {
    // Dispatch drag event to root level grid view for re-parenting folder
    // folder item purpose.
    DispatchDragEventForReparent(pointer, drag_point);
    return;
  }

  // Regular drag and drop in a folder's grid view.
  folder_delegate_->UpdateFolderViewBackground(true);

  // Calculate if the drag_view_ is dragged out of the folder's container
  // ink bubble.
  gfx::Rect bounds_to_folder_view = ConvertRectToParent(drag_view_->bounds());
  gfx::Point pt = bounds_to_folder_view.CenterPoint();
  bool is_item_dragged_out_of_folder =
      folder_delegate_->IsPointOutsideOfFolderBoundary(pt);
  if (is_item_dragged_out_of_folder) {
    if (!drag_out_of_folder_container_) {
      folder_item_reparent_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kFolderItemReparentDelay),
          this,
          &AppsGridView::OnFolderItemReparentTimer);
      drag_out_of_folder_container_ = true;
    }
  } else {
    folder_item_reparent_timer_.Stop();
    drag_out_of_folder_container_ = false;
  }
}

bool AppsGridView::IsDraggingForReparentInRootLevelGridView() const {
  return (!folder_delegate_ && dragging_for_reparent_item_);
}

bool AppsGridView::IsDraggingForReparentInHiddenGridView() const {
  return (folder_delegate_ && dragging_for_reparent_item_);
}

gfx::Rect AppsGridView::GetTargetIconRectInFolder(
    AppListItemView* drag_item_view,
    AppListItemView* folder_item_view) {
  gfx::Rect view_ideal_bounds = view_model_.ideal_bounds(
      view_model_.GetIndexOfView(folder_item_view));
  gfx::Rect icon_ideal_bounds =
      folder_item_view->GetIconBoundsForTargetViewBounds(view_ideal_bounds);
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(folder_item_view->item());
  return folder_item->GetTargetIconRectInFolderForItem(
      drag_item_view->item(), icon_ideal_bounds);
}

bool AppsGridView::IsUnderOEMFolder() {
  if (!folder_delegate_)
    return false;

  return folder_delegate_->IsOEMFolder();
}

void AppsGridView::DispatchDragEventForReparent(Pointer pointer,
                                                const gfx::Point& drag_point) {
  folder_delegate_->DispatchDragEventForReparent(pointer, drag_point);
}

void AppsGridView::EndDragFromReparentItemInRootLevel(
    bool events_forwarded_to_drag_drop_host,
    bool cancel_drag) {
  // EndDrag was called before if |drag_view_| is NULL.
  if (!drag_view_)
    return;

  DCHECK(IsDraggingForReparentInRootLevelGridView());
  bool cancel_reparent = cancel_drag || drop_attempt_ == DROP_FOR_NONE;
  if (!events_forwarded_to_drag_drop_host && !cancel_reparent) {
    CalculateDropTarget();
    if (drop_attempt_ == DROP_FOR_REORDER &&
        IsValidIndex(reorder_drop_target_)) {
      ReparentItemForReorder(drag_view_, reorder_drop_target_);
    } else if (drop_attempt_ == DROP_FOR_FOLDER &&
               IsValidIndex(folder_drop_target_)) {
      cancel_reparent =
          !ReparentItemToAnotherFolder(drag_view_, folder_drop_target_);
    } else {
      NOTREACHED();
    }
    SetViewHidden(drag_view_, false /* show */, true /* no animate */);
  }

  SetAsFolderDroppingTarget(folder_drop_target_, false);
  if (cancel_reparent) {
    CancelFolderItemReparent(drag_view_);
  } else {
    // By setting |drag_view_| to NULL here, we prevent ClearDragState() from
    // cleaning up the newly created AppListItemView, effectively claiming
    // ownership of the newly created drag view.
    drag_view_->OnDragEnded();
    drag_view_ = NULL;
  }
  ClearDragState();
  AnimateToIdealBounds();

  StopPageFlipTimer();
}

void AppsGridView::EndDragForReparentInHiddenFolderGridView() {
  if (drag_and_drop_host_) {
    // If we had a drag and drop proxy icon, we delete it and make the real
    // item visible again.
    drag_and_drop_host_->DestroyDragIconProxy();
  }

  SetAsFolderDroppingTarget(folder_drop_target_, false);
  ClearDragState();
}

void AppsGridView::OnFolderItemRemoved() {
  DCHECK(folder_delegate_);
  if (item_list_)
    item_list_->RemoveObserver(this);
  item_list_ = nullptr;
}

void AppsGridView::StartDragAndDropHostDrag(const gfx::Point& grid_location) {
  // When a drag and drop host is given, the item can be dragged out of the app
  // list window. In that case a proxy widget needs to be used.
  // Note: This code has very likely to be changed for Windows (non metro mode)
  // when a |drag_and_drop_host_| gets implemented.
  if (!drag_view_ || !drag_and_drop_host_)
    return;

  gfx::Point screen_location = grid_location;
  views::View::ConvertPointToScreen(this, &screen_location);

  // Determine the mouse offset to the center of the icon so that the drag and
  // drop host follows this layer.
  gfx::Vector2d delta = drag_view_offset_ -
                        drag_view_->GetLocalBounds().CenterPoint();
  delta.set_y(delta.y() + drag_view_->title()->size().height() / 2);

  // We have to hide the original item since the drag and drop host will do
  // the OS dependent code to "lift off the dragged item".
  DCHECK(!IsDraggingForReparentInRootLevelGridView());
  drag_and_drop_host_->CreateDragIconProxy(screen_location,
                                           drag_view_->item()->icon(),
                                           drag_view_,
                                           delta,
                                           kDragAndDropProxyScale);
  SetViewHidden(drag_view_,
           true /* hide */,
           true /* no animation */);
}

void AppsGridView::DispatchDragEventToDragAndDropHost(
    const gfx::Point& location_in_screen_coordinates) {
  if (!drag_view_ || !drag_and_drop_host_)
    return;

  if (GetLocalBounds().Contains(last_drag_point_)) {
    // The event was issued inside the app menu and we should get all events.
    if (forward_events_to_drag_and_drop_host_) {
      // The DnD host was previously called and needs to be informed that the
      // session returns to the owner.
      forward_events_to_drag_and_drop_host_ = false;
      drag_and_drop_host_->EndDrag(true);
    }
  } else {
    if (IsFolderItem(drag_view_->item()))
      return;

    // The event happened outside our app menu and we might need to dispatch.
    if (forward_events_to_drag_and_drop_host_) {
      // Dispatch since we have already started.
      if (!drag_and_drop_host_->Drag(location_in_screen_coordinates)) {
        // The host is not active any longer and we cancel the operation.
        forward_events_to_drag_and_drop_host_ = false;
        drag_and_drop_host_->EndDrag(true);
      }
    } else {
      if (drag_and_drop_host_->StartDrag(drag_view_->item()->id(),
                                         location_in_screen_coordinates)) {
        // From now on we forward the drag events.
        forward_events_to_drag_and_drop_host_ = true;
        // Any flip operations are stopped.
        StopPageFlipTimer();
      }
    }
  }
}

void AppsGridView::MaybeStartPageFlipTimer(const gfx::Point& drag_point) {
  if (!IsPointWithinDragBuffer(drag_point))
    StopPageFlipTimer();
  int new_page_flip_target = -1;

  // Drag zones are at the edges of the scroll axis.
  if (pagination_controller_->scroll_axis() ==
      PaginationController::SCROLL_AXIS_VERTICAL) {
    if (drag_point.y() < kPageFlipZoneSize)
      new_page_flip_target = pagination_model_.selected_page() - 1;
    else if (drag_point.y() > height() - kPageFlipZoneSize)
      new_page_flip_target = pagination_model_.selected_page() + 1;
  } else {
    if (page_switcher_view_->bounds().Contains(drag_point)) {
      gfx::Point page_switcher_point(drag_point);
      views::View::ConvertPointToTarget(
          this, page_switcher_view_, &page_switcher_point);
      new_page_flip_target =
          page_switcher_view_->GetPageForPoint(page_switcher_point);
    }

    // TODO(xiyuan): Fix this for RTL.
    if (new_page_flip_target == -1 && drag_point.x() < kPageFlipZoneSize)
      new_page_flip_target = pagination_model_.selected_page() - 1;

    if (new_page_flip_target == -1 &&
        drag_point.x() > width() - kPageFlipZoneSize) {
      new_page_flip_target = pagination_model_.selected_page() + 1;
    }
  }

  if (new_page_flip_target == page_flip_target_)
    return;

  StopPageFlipTimer();
  if (pagination_model_.is_valid_page(new_page_flip_target)) {
    page_flip_target_ = new_page_flip_target;

    if (page_flip_target_ != pagination_model_.selected_page()) {
      page_flip_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(page_flip_delay_in_ms_),
          this, &AppsGridView::OnPageFlipTimer);
    }
  }
}

void AppsGridView::OnPageFlipTimer() {
  DCHECK(pagination_model_.is_valid_page(page_flip_target_));
  pagination_model_.SelectPage(page_flip_target_, true);
}

void AppsGridView::MoveItemInModel(AppListItemView* item_view,
                                   const Index& target) {
  int current_model_index = view_model_.GetIndexOfView(item_view);
  DCHECK_GE(current_model_index, 0);

  int target_model_index = GetModelIndexFromIndex(target);
  if (target_model_index == current_model_index)
    return;

  item_list_->RemoveObserver(this);
  item_list_->MoveItem(current_model_index, target_model_index);
  view_model_.Move(current_model_index, target_model_index);
  item_list_->AddObserver(this);

  if (pagination_model_.selected_page() != target.page)
    pagination_model_.SelectPage(target.page, false);
}

void AppsGridView::MoveItemToFolder(AppListItemView* item_view,
                                    const Index& target) {
  const std::string& source_item_id = item_view->item()->id();
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target.slot);
  DCHECK(target_view);
  const std::string& target_view_item_id = target_view->item()->id();

  // Check that the item is not being dropped onto itself; this should not
  // happen, but it can if something allows multiple views to share an
  // item (e.g., if a folder drop does not clean up properly).
  DCHECK_NE(source_item_id, target_view_item_id);

  // Make change to data model.
  item_list_->RemoveObserver(this);
  std::string folder_item_id =
      model_->MergeItems(target_view_item_id, source_item_id);
  item_list_->AddObserver(this);
  if (folder_item_id.empty()) {
    LOG(ERROR) << "Unable to merge into item id: " << target_view_item_id;
    return;
  }
  if (folder_item_id != target_view_item_id) {
    // New folder was created, change the view model to replace the old target
    // view with the new folder item view.
    size_t folder_item_index;
    if (item_list_->FindItemIndex(folder_item_id, &folder_item_index)) {
      int target_view_index = view_model_.GetIndexOfView(target_view);
      gfx::Rect target_view_bounds = target_view->bounds();
      DeleteItemViewAtIndex(target_view_index);
      AppListItemView* target_folder_view =
          CreateViewForItemAtIndex(folder_item_index);
      target_folder_view->SetBoundsRect(target_view_bounds);
      view_model_.Add(target_folder_view, target_view_index);
      AddChildView(target_folder_view);
    } else {
      LOG(ERROR) << "Folder no longer in item_list: " << folder_item_id;
    }
  }

  // Fade out the drag_view_ and delete it when animation ends.
  int drag_view_index = view_model_.GetIndexOfView(drag_view_);
  view_model_.Remove(drag_view_index);
  bounds_animator_.AnimateViewTo(drag_view_, drag_view_->bounds());
  bounds_animator_.SetAnimationDelegate(
      drag_view_, std::unique_ptr<gfx::AnimationDelegate>(
                      new ItemRemoveAnimationDelegate(drag_view_)));
  UpdatePaging();
}

void AppsGridView::ReparentItemForReorder(AppListItemView* item_view,
                                          const Index& target) {
  item_list_->RemoveObserver(this);
  model_->RemoveObserver(this);

  AppListItem* reparent_item = item_view->item();
  DCHECK(reparent_item->IsInFolder());
  const std::string source_folder_id = reparent_item->folder_id();
  AppListFolderItem* source_folder =
      static_cast<AppListFolderItem*>(item_list_->FindItem(source_folder_id));

  int target_model_index = GetModelIndexFromIndex(target);

  // Remove the source folder view if there is only 1 item in it, since the
  // source folder will be deleted after its only child item removed from it.
  if (source_folder->ChildItemCount() == 1u) {
    const int deleted_folder_index =
        view_model_.GetIndexOfView(activated_folder_item_view());
    DeleteItemViewAtIndex(deleted_folder_index);

    // Adjust |target_model_index| if it is beyond the deleted folder index.
    if (target_model_index > deleted_folder_index)
      --target_model_index;
  }

  // Move the item from its parent folder to top level item list.
  // Must move to target_model_index, the location we expect the target item
  // to be, not the item location we want to insert before.
  int current_model_index = view_model_.GetIndexOfView(item_view);
  syncer::StringOrdinal target_position;
  if (target_model_index < static_cast<int>(item_list_->item_count()))
    target_position = item_list_->item_at(target_model_index)->position();
  model_->MoveItemToFolderAt(reparent_item, "", target_position);
  view_model_.Move(current_model_index, target_model_index);

  RemoveLastItemFromReparentItemFolderIfNecessary(source_folder_id);

  item_list_->AddObserver(this);
  model_->AddObserver(this);
  UpdatePaging();
}

bool AppsGridView::ReparentItemToAnotherFolder(AppListItemView* item_view,
                                               const Index& target) {
  DCHECK(IsDraggingForReparentInRootLevelGridView());

  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target.slot);
  if (!target_view)
    return false;

  AppListItem* reparent_item = item_view->item();
  DCHECK(reparent_item->IsInFolder());
  const std::string source_folder_id = reparent_item->folder_id();
  AppListFolderItem* source_folder =
      static_cast<AppListFolderItem*>(item_list_->FindItem(source_folder_id));

  AppListItem* target_item = target_view->item();

  // An app is being reparented to its original folder. Just cancel the
  // reparent.
  if (target_item->id() == reparent_item->folder_id())
    return false;

  // Make change to data model.
  item_list_->RemoveObserver(this);

  // Remove the source folder view if there is only 1 item in it, since the
  // source folder will be deleted after its only child item merged into the
  // target item.
  if (source_folder->ChildItemCount() == 1u)
    DeleteItemViewAtIndex(
        view_model_.GetIndexOfView(activated_folder_item_view()));

  // Move item to the target folder.
  std::string target_id_after_merge =
      model_->MergeItems(target_item->id(), reparent_item->id());
  if (target_id_after_merge.empty()) {
    LOG(ERROR) << "Unable to reparent to item id: " << target_item->id();
    item_list_->AddObserver(this);
    return false;
  }

  if (target_id_after_merge != target_item->id()) {
    // New folder was created, change the view model to replace the old target
    // view with the new folder item view.
    const std::string& new_folder_id = reparent_item->folder_id();
    size_t new_folder_index;
    if (item_list_->FindItemIndex(new_folder_id, &new_folder_index)) {
      int target_view_index = view_model_.GetIndexOfView(target_view);
      DeleteItemViewAtIndex(target_view_index);
      AppListItemView* new_folder_view =
          CreateViewForItemAtIndex(new_folder_index);
      view_model_.Add(new_folder_view, target_view_index);
      AddChildView(new_folder_view);
    } else {
      LOG(ERROR) << "Folder no longer in item_list: " << new_folder_id;
    }
  }

  RemoveLastItemFromReparentItemFolderIfNecessary(source_folder_id);

  item_list_->AddObserver(this);

  // Fade out the drag_view_ and delete it when animation ends.
  int drag_view_index = view_model_.GetIndexOfView(drag_view_);
  view_model_.Remove(drag_view_index);
  bounds_animator_.AnimateViewTo(drag_view_, drag_view_->bounds());
  bounds_animator_.SetAnimationDelegate(
      drag_view_, std::unique_ptr<gfx::AnimationDelegate>(
                      new ItemRemoveAnimationDelegate(drag_view_)));
  UpdatePaging();

  return true;
}

// After moving the re-parenting item out of the folder, if there is only 1 item
// left, remove the last item out of the folder, delete the folder and insert it
// to the data model at the same position. Make the same change to view_model_
// accordingly.
void AppsGridView::RemoveLastItemFromReparentItemFolderIfNecessary(
    const std::string& source_folder_id) {
  AppListFolderItem* source_folder =
      static_cast<AppListFolderItem*>(item_list_->FindItem(source_folder_id));
  if (!source_folder || source_folder->ChildItemCount() != 1u)
    return;

  // Delete view associated with the folder item to be removed.
  DeleteItemViewAtIndex(
      view_model_.GetIndexOfView(activated_folder_item_view()));

  // Now make the data change to remove the folder item in model.
  AppListItem* last_item = source_folder->item_list()->item_at(0);
  model_->MoveItemToFolderAt(last_item, "", source_folder->position());

  // Create a new item view for the last item in folder.
  size_t last_item_index;
  if (!item_list_->FindItemIndex(last_item->id(), &last_item_index) ||
      last_item_index > static_cast<size_t>(view_model_.view_size())) {
    NOTREACHED();
    return;
  }
  AppListItemView* last_item_view = CreateViewForItemAtIndex(last_item_index);
  view_model_.Add(last_item_view, last_item_index);
  AddChildView(last_item_view);
}

void AppsGridView::CancelFolderItemReparent(AppListItemView* drag_item_view) {
  // The icon of the dragged item must target to its final ideal bounds after
  // the animation completes.
  CalculateIdealBounds();

  gfx::Rect target_icon_rect =
      GetTargetIconRectInFolder(drag_item_view, activated_folder_item_view_);

  gfx::Rect drag_view_icon_to_grid =
      drag_item_view->ConvertRectToParent(drag_item_view->GetIconBounds());
  drag_view_icon_to_grid.ClampToCenteredSize(
      gfx::Size(kGridIconDimension, kGridIconDimension));
  TopIconAnimationView* icon_view = new TopIconAnimationView(
      drag_item_view->item()->icon(),
      target_icon_rect,
      false);    /* animate like closing folder */
  AddChildView(icon_view);
  icon_view->SetBoundsRect(drag_view_icon_to_grid);
  icon_view->TransformView();
}

void AppsGridView::CancelContextMenusOnCurrentPage() {
  int start = pagination_model_.selected_page() * tiles_per_page();
  int end = std::min(view_model_.view_size(), start + tiles_per_page());
  for (int i = start; i < end; ++i)
    GetItemViewAt(i)->CancelContextMenu();
}

void AppsGridView::DeleteItemViewAtIndex(int index) {
  AppListItemView* item_view = GetItemViewAt(index);
  view_model_.Remove(index);
  if (item_view == drag_view_)
    drag_view_ = NULL;
  delete item_view;
}

bool AppsGridView::IsPointWithinDragBuffer(const gfx::Point& point) const {
  gfx::Rect rect(GetLocalBounds());
  rect.Inset(-kDragBufferPx, -kDragBufferPx, -kDragBufferPx, -kDragBufferPx);
  return rect.Contains(point);
}

void AppsGridView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  if (dragging())
    return;

  if (strcmp(sender->GetClassName(), AppListItemView::kViewClassName))
    return;
  AppListItemView* pressed_item_view = static_cast<AppListItemView*>(sender);

  if (delegate_) {
    // Always set the previous activated_folder_item_view_ to be visible. This
    // prevents a case where the item would remain hidden due the
    // |activated_folder_item_view_| changing during the animation. We only
    // need to track |activated_folder_item_view_| in the root level grid view.
    if (!folder_delegate_) {
      if (activated_folder_item_view_)
        activated_folder_item_view_->SetVisible(true);
      if (IsFolderItem(pressed_item_view->item()))
        activated_folder_item_view_ = pressed_item_view;
      else
        activated_folder_item_view_ = NULL;
    }
    delegate_->ActivateApp(pressed_item_view->item(), event.flags());
  }
}

void AppsGridView::OnListItemAdded(size_t index, AppListItem* item) {
  EndDrag(true);

  AppListItemView* view = CreateViewForItemAtIndex(index);
  view_model_.Add(view, index);
  AddChildView(view);

  UpdatePaging();
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();
}

void AppsGridView::OnListItemRemoved(size_t index, AppListItem* item) {
  EndDrag(true);

  DeleteItemViewAtIndex(index);

  UpdatePaging();
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();
}

void AppsGridView::OnListItemMoved(size_t from_index,
                                   size_t to_index,
                                   AppListItem* item) {
  EndDrag(true);
  view_model_.Move(from_index, to_index);

  UpdatePaging();
  AnimateToIdealBounds();
}

void AppsGridView::OnAppListItemHighlight(size_t index, bool highlight) {
  AppListItemView* view = GetItemViewAt(index);
  view->SetItemIsHighlighted(highlight);
  if (highlight)
    EnsureViewVisible(GetIndexFromModelIndex(index));
}

void AppsGridView::TotalPagesChanged() {
}

void AppsGridView::SelectedPageChanged(int old_selected, int new_selected) {
  if (dragging()) {
    CalculateDropTarget();
    Layout();
    MaybeStartPageFlipTimer(last_drag_point_);
  } else {
    ClearSelectedView(selected_view_);
    Layout();
  }
}

void AppsGridView::TransitionStarted() {
  CancelContextMenusOnCurrentPage();
}

void AppsGridView::TransitionChanged() {
  // Update layout for valid page transition only since over-scroll no longer
  // animates app icons.
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  if (pagination_model_.is_valid_page(transition.target_page))
    Layout();
}

void AppsGridView::OnAppListModelStatusChanged() {
  UpdatePulsingBlockViews();
  Layout();
  SchedulePaint();
}

void AppsGridView::SetViewHidden(AppListItemView* view,
                                 bool hide,
                                 bool immediate) {
  ui::ScopedLayerAnimationSettings animator(view->layer()->GetAnimator());
  animator.SetPreemptionStrategy(
      immediate ? ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET
                : ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  view->layer()->SetOpacity(hide ? 0 : 1);
}

void AppsGridView::OnImplicitAnimationsCompleted() {
  if (layer()->opacity() == 0.0f)
    SetVisible(false);
}

bool AppsGridView::EnableFolderDragDropUI() {
  // Enable drag and drop folder UI only if it is at the app list root level
  // and the switch is on.
  return model_->folders_enabled() && !folder_delegate_;
}

AppsGridView::Index AppsGridView::GetNearestTileIndexForPoint(
    const gfx::Point& point) const {
  gfx::Rect bounds = GetContentsBounds();
  gfx::Size total_tile_size = GetTotalTileSize();
  int col = ClampToRange(
      (point.x() - bounds.x()) / total_tile_size.width(), 0, cols_ - 1);
  int row = ClampToRange((point.y() - bounds.y()) / total_tile_size.height(),
                         0,
                         rows_per_page_ - 1);
  return Index(pagination_model_.selected_page(), row * cols_ + col);
}

gfx::Size AppsGridView::GetTileGridSize() const {
  gfx::Rect bounds = GetExpectedTileBounds(0, 0);
  bounds.Union(GetExpectedTileBounds(rows_per_page_ - 1, cols_ - 1));
  bounds.Inset(GetTilePadding());
  return bounds.size();
}

gfx::Rect AppsGridView::GetExpectedTileBounds(int slot) const {
  return GetExpectedTileBounds(slot / cols_, slot % cols_);
}

gfx::Rect AppsGridView::GetExpectedTileBounds(int row, int col) const {
  gfx::Rect bounds(GetContentsBounds());
  gfx::Size total_tile_size = GetTotalTileSize();
  gfx::Rect tile_bounds(gfx::Point(bounds.x() + col * total_tile_size.width(),
                                   bounds.y() + row * total_tile_size.height()),
                        total_tile_size);
  tile_bounds.Inset(-GetTilePadding());
  return tile_bounds;
}

AppListItemView* AppsGridView::GetViewDisplayedAtSlotOnCurrentPage(
    int slot) const {
  if (slot < 0)
    return NULL;

  // Calculate the original bound of the tile at |index|.
  int row = slot / cols_;
  int col = slot % cols_;
  gfx::Rect tile_rect = GetExpectedTileBounds(row, col);

  for (int i = 0; i < view_model_.view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view->bounds() == tile_rect && view != drag_view_)
      return view;
  }
  return NULL;
}

void AppsGridView::SetAsFolderDroppingTarget(const Index& target_index,
                                             bool is_target_folder) {
  AppListItemView* target_view =
      GetViewDisplayedAtSlotOnCurrentPage(target_index.slot);
  if (target_view)
    target_view->SetAsAttemptedFolderTarget(is_target_folder);
}

}  // namespace app_list
