// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/apps_grid_view.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/guid.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_drag_and_drop_host.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/apps_grid_view_delegate.h"
#include "ui/app_list/views/page_switcher.h"
#include "ui/app_list/views/pulsing_block_view.h"
#include "ui/app_list/views/top_icon_animation_view.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/views/border.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif  // defined(OS_WIN)
#endif  // defined(USE_AURA)

#if defined(OS_WIN)
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/win/shortcut.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/drop_target_win.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/gfx/win/dpi.h"
#endif

namespace app_list {

namespace {

// Distance a drag needs to be from the app grid to be considered 'outside', at
// which point we rearrange the apps to their pre-drag configuration, as a drop
// then would be canceled. We have a buffer to make it easier to drag apps to
// other pages.
const int kDragBufferPx = 20;

// Padding space in pixels for fixed layout.
const int kLeftRightPadding = 20;
const int kTopPadding = 1;

// Padding space in pixels between pages.
const int kPagePadding = 40;

// Preferred tile size when showing in fixed layout.
const int kPreferredTileWidth = 88;
const int kPreferredTileHeight = 98;

// Width in pixels of the area on the sides that triggers a page flip.
const int kPageFlipZoneSize = 40;

// Delay in milliseconds to do the page flip.
const int kPageFlipDelayInMs = 1000;

// How many pages on either side of the selected one we prerender.
const int kPrerenderPages = 1;

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
const int kFolderDroppingCircleRadius = 15;


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
  virtual ~RowMoveAnimationDelegate() {}

  // gfx::AnimationDelegate overrides:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();

    if (layer_) {
      layer_->SetOpacity(1 - animation->GetCurrentValue());
      layer_->SetBounds(animation->CurrentValueBetween(layer_start_,
                                                       layer_target_));
      layer_->ScheduleDraw();
    }
  }
  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1.0f);
    view_->SchedulePaint();
  }
  virtual void AnimationCanceled(const gfx::Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1.0f);
    view_->SchedulePaint();
  }

 private:
  // The view that needs to be wrapped. Owned by views hierarchy.
  views::View* view_;

  scoped_ptr<ui::Layer> layer_;
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

  virtual ~ItemRemoveAnimationDelegate() {
  }

  // gfx::AnimationDelegate overrides:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }

 private:
  scoped_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(ItemRemoveAnimationDelegate);
};

// ItemMoveAnimationDelegate observes when an item finishes animating when it is
// not moving between rows. This is to ensure an item is repainted for the
// "zoom out" case when releasing an item being dragged.
class ItemMoveAnimationDelegate : public gfx::AnimationDelegate {
 public:
  ItemMoveAnimationDelegate(views::View* view) : view_(view) {}

  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE {
    view_->SchedulePaint();
  }
  virtual void AnimationCanceled(const gfx::Animation* animation) OVERRIDE {
    view_->SchedulePaint();
  }

 private:
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(ItemMoveAnimationDelegate);
};

// Gets the distance between the centers of the |rect_1| and |rect_2|.
int GetDistanceBetweenRects(gfx::Rect rect_1,
                            gfx::Rect rect_2) {
  return (rect_1.CenterPoint() - rect_2.CenterPoint()).Length();
}

// Returns true if the |item| is an folder item.
bool IsFolderItem(AppListItem* item) {
  return (item->GetItemType() == AppListFolderItem::kItemType);
}

bool IsOEMFolderItem(AppListItem* item) {
  return IsFolderItem(item) &&
         (static_cast<AppListFolderItem*>(item))->folder_type() ==
             AppListFolderItem::FOLDER_TYPE_OEM;
}

}  // namespace

#if defined(OS_WIN)
// Interprets drag events sent from Windows via the drag/drop API and forwards
// them to AppsGridView.
// On Windows, in order to have the OS perform the drag properly we need to
// provide it with a shortcut file which may or may not exist at the time the
// drag is started. Therefore while waiting for that shortcut to be located we
// just do a regular "internal" drag and transition into the synchronous drag
// when the shortcut is found/created. Hence a synchronous drag is an optional
// phase of a regular drag and non-Windows platforms drags are equivalent to a
// Windows drag that never enters the synchronous drag phase.
class SynchronousDrag : public ui::DragSourceWin {
 public:
  SynchronousDrag(AppsGridView* grid_view,
                  AppListItemView* drag_view,
                  const gfx::Point& drag_view_offset)
      : grid_view_(grid_view),
        drag_view_(drag_view),
        drag_view_offset_(drag_view_offset),
        has_shortcut_path_(false),
        running_(false),
        canceled_(false) {}

  void set_shortcut_path(const base::FilePath& shortcut_path) {
    has_shortcut_path_ = true;
    shortcut_path_ = shortcut_path;
  }

  bool running() { return running_; }

  bool CanRun() {
    return has_shortcut_path_ && !running_;
  }

  void Run() {
    DCHECK(CanRun());

    // Prevent the synchronous dragger being destroyed while the drag is
    // running.
    scoped_refptr<SynchronousDrag> this_ref = this;
    running_ = true;

    ui::OSExchangeData data;
    SetupExchangeData(&data);

    // Hide the dragged view because the OS is going to create its own.
    drag_view_->SetVisible(false);

    // Blocks until the drag is finished. Calls into the ui::DragSourceWin
    // methods.
    DWORD effects;
    DoDragDrop(ui::OSExchangeDataProviderWin::GetIDataObject(data),
               this, DROPEFFECT_MOVE | DROPEFFECT_LINK, &effects);

    // If |drag_view_| is NULL the drag was ended by some reentrant code.
    if (drag_view_) {
      // Make the drag view visible again.
      drag_view_->SetVisible(true);
      drag_view_->OnSyncDragEnd();

      grid_view_->EndDrag(canceled_ || !IsCursorWithinGridView());
    }
  }

  void EndDragExternally() {
    CancelDrag();
    DCHECK(drag_view_);
    drag_view_->SetVisible(true);
    drag_view_ = NULL;
  }

 private:
  // Overridden from ui::DragSourceWin.
  virtual void OnDragSourceCancel() OVERRIDE {
    canceled_ = true;
  }

  virtual void OnDragSourceDrop() OVERRIDE {
  }

  virtual void OnDragSourceMove() OVERRIDE {
    grid_view_->UpdateDrag(AppsGridView::MOUSE, GetCursorInGridViewCoords());
  }

  void SetupExchangeData(ui::OSExchangeData* data) {
    data->SetFilename(shortcut_path_);
    gfx::ImageSkia image(drag_view_->GetDragImage());
    gfx::Size image_size(image.size());
    drag_utils::SetDragImageOnDataObject(
        image,
        drag_view_offset_ - drag_view_->GetDragImageOffset(),
        data);
  }

  HWND GetGridViewHWND() {
    return views::HWNDForView(grid_view_);
  }

  bool IsCursorWithinGridView() {
    POINT p;
    GetCursorPos(&p);
    return GetGridViewHWND() == WindowFromPoint(p);
  }

  gfx::Point GetCursorInGridViewCoords() {
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(GetGridViewHWND(), &p);
    gfx::Point grid_view_pt(p.x, p.y);
    grid_view_pt = gfx::win::ScreenToDIPPoint(grid_view_pt);
    views::View::ConvertPointFromWidget(grid_view_, &grid_view_pt);
    return grid_view_pt;
  }

  AppsGridView* grid_view_;
  AppListItemView* drag_view_;
  gfx::Point drag_view_offset_;
  bool has_shortcut_path_;
  base::FilePath shortcut_path_;
  bool running_;
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousDrag);
};
#endif  // defined(OS_WIN)

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
#if defined(OS_WIN)
      use_synchronous_drag_(true),
#endif
      drag_pointer_(NONE),
      drop_attempt_(DROP_FOR_NONE),
      drag_and_drop_host_(NULL),
      forward_events_to_drag_and_drop_host_(false),
      page_flip_target_(-1),
      page_flip_delay_in_ms_(kPageFlipDelayInMs),
      bounds_animator_(this),
      activated_folder_item_view_(NULL),
      dragging_for_reparent_item_(false) {
  SetPaintToLayer(true);
  // Clip any icons that are outside the grid view's bounds. These icons would
  // otherwise be visible to the user when the grid view is off screen.
  layer()->SetMasksToBounds(true);
  SetFillsBoundsOpaquely(false);

  pagination_model_.SetTransitionDurations(kPageTransitionDurationInMs,
                                           kOverscrollPageTransitionDurationMs);

  pagination_model_.AddObserver(this);
  page_switcher_view_ = new PageSwitcher(&pagination_model_);
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

void AppsGridView::SetLayout(int icon_size, int cols, int rows_per_page) {
  icon_size_.SetSize(icon_size, icon_size);
  cols_ = cols;
  rows_per_page_ = rows_per_page;

  SetBorder(views::Border::CreateEmptyBorder(
      kTopPadding, kLeftRightPadding, 0, kLeftRightPadding));
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

void AppsGridView::SetSelectedView(views::View* view) {
  if (IsSelectedView(view) || IsDraggedView(view))
    return;

  Index index = GetIndexOfView(view);
  if (IsValidIndex(index))
    SetSelectedItemByIndex(index);
}

void AppsGridView::ClearSelectedView(views::View* view) {
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

bool AppsGridView::IsSelectedView(const views::View* view) const {
  return selected_view_ == view;
}

void AppsGridView::EnsureViewVisible(const views::View* view) {
  if (pagination_model_.has_transition())
    return;

  Index index = GetIndexOfView(view);
  if (IsValidIndex(index))
    pagination_model_.SelectPage(index.page, false);
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
  ExtractDragLocation(event, &drag_start_grid_view_);
  drag_view_start_ = gfx::Point(drag_view_->x(), drag_view_->y());
}

void AppsGridView::StartSettingUpSynchronousDrag() {
#if defined(OS_WIN)
  if (!delegate_ || !use_synchronous_drag_)
    return;

  // Folders and downloading items can't be integrated with the OS.
  if (IsFolderItem(drag_view_->item()) || drag_view_->item()->is_installing())
    return;

  // Favor the drag and drop host over native win32 drag. For the Win8/ash
  // launcher we want to have ashes drag and drop over win32's.
  if (drag_and_drop_host_)
    return;

  // Never create a second synchronous drag if the drag started in a folder.
  if (IsDraggingForReparentInRootLevelGridView())
    return;

  synchronous_drag_ = new SynchronousDrag(this, drag_view_, drag_view_offset_);
  delegate_->GetShortcutPathForApp(drag_view_->item()->id(),
                                   base::Bind(&AppsGridView::OnGotShortcutPath,
                                              base::Unretained(this),
                                              synchronous_drag_));
#endif
}

bool AppsGridView::RunSynchronousDrag() {
#if defined(OS_WIN)
  if (!synchronous_drag_)
    return false;

  if (synchronous_drag_->CanRun()) {
    if (IsDraggingForReparentInHiddenGridView())
      folder_delegate_->SetRootLevelDragViewVisible(false);
    synchronous_drag_->Run();
    synchronous_drag_ = NULL;
    return true;
  } else if (!synchronous_drag_->running()) {
    // The OS drag is not ready yet. If the root grid has a drag view because
    // a reparent has started, ensure it is visible.
    if (IsDraggingForReparentInHiddenGridView())
      folder_delegate_->SetRootLevelDragViewVisible(true);
  }
#endif
  return false;
}

void AppsGridView::CleanUpSynchronousDrag() {
#if defined(OS_WIN)
  if (synchronous_drag_)
    synchronous_drag_->EndDragExternally();

  synchronous_drag_ = NULL;
#endif
}

#if defined(OS_WIN)
void AppsGridView::OnGotShortcutPath(
    scoped_refptr<SynchronousDrag> synchronous_drag,
    const base::FilePath& path) {
  // Drag may have ended before we get the shortcut path or a new drag may have
  // begun.
  if (synchronous_drag_ != synchronous_drag)
    return;
  // Setting the shortcut path here means the next time we hit UpdateDrag()
  // we'll enter the synchronous drag.
  // NOTE we don't Run() the drag here because that causes animations not to
  // update for some reason.
  synchronous_drag_->set_shortcut_path(path);
  DCHECK(synchronous_drag_->CanRun());
}
#endif

bool AppsGridView::UpdateDragFromItem(Pointer pointer,
                                      const ui::LocatedEvent& event) {
  DCHECK(drag_view_);

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

  // EndDrag was called before if |drag_view_| is NULL.
  if (!drag_view_)
    return;

  if (RunSynchronousDrag())
    return;

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

    StartSettingUpSynchronousDrag();
    if (!dragging_for_reparent_item_)
      StartDragAndDropHostDrag(point);
  }

  if (drag_pointer_ != pointer)
    return;

  last_drag_point_ = point;
  const Index last_drop_target = drop_target_;
  DropAttempt last_drop_attempt = drop_attempt_;
  CalculateDropTarget(last_drag_point_, false);

  if (IsPointWithinDragBuffer(last_drag_point_))
    MaybeStartPageFlipTimer(last_drag_point_);
  else
    StopPageFlipTimer();

  gfx::Point page_switcher_point(last_drag_point_);
  views::View::ConvertPointToTarget(this, page_switcher_view_,
                                    &page_switcher_point);
  page_switcher_view_->UpdateUIForDragPoint(page_switcher_point);

  if (!EnableFolderDragDropUI()) {
    if (last_drop_target != drop_target_)
      AnimateToIdealBounds();
    drag_view_->SetPosition(drag_view_start_ + drag_vector);
    return;
  }

  // Update drag with folder UI enabled.
  if (last_drop_target != drop_target_ ||
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
    SetAsFolderDroppingTarget(last_drop_target, false);
  }

  drag_view_->SetPosition(drag_view_start_ + drag_vector);
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

    if (!cancel && dragging()) {
      // Regular drag ending path, ie, not for reparenting.
      CalculateDropTarget(last_drag_point_, true);
      if (IsValidIndex(drop_target_)) {
        if (!EnableFolderDragDropUI()) {
            MoveItemInModel(drag_view_, drop_target_);
        } else {
          if (drop_attempt_ == DROP_FOR_REORDER)
            MoveItemInModel(drag_view_, drop_target_);
          else if (drop_attempt_ == DROP_FOR_FOLDER)
            MoveItemToFolder(drag_view_, drop_target_);
        }
      }
    }
  }

  if (drag_and_drop_host_) {
    // If we had a drag and drop proxy icon, we delete it and make the real
    // item visible again.
    drag_and_drop_host_->DestroyDragIconProxy();
    if (landed_in_drag_and_drop_host) {
      // Move the item directly to the target location, avoiding the "zip back"
      // animation if the user was pinning it to the shelf.
      int i = drop_target_.slot;
      gfx::Rect bounds = view_model_.ideal_bounds(i);
      drag_view_->SetBoundsRect(bounds);
    }
    // Fade in slowly if it landed in the shelf.
    SetViewHidden(drag_view_,
                  false /* show */,
                  !landed_in_drag_and_drop_host /* animate */);
  }

  // The drag can be ended after the synchronous drag is created but before it
  // is Run().
  CleanUpSynchronousDrag();

  SetAsFolderDroppingTarget(drop_target_, false);
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
  DCHECK(index >= 0 && index < view_model_.view_size());
  return static_cast<AppListItemView*>(view_model_.view_at(index));
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
    const gfx::Point& drag_point) {
  DCHECK(original_drag_view && !drag_view_);
  DCHECK(!dragging_for_reparent_item_);

  // Create a new AppListItemView to duplicate the original_drag_view in the
  // folder's grid view.
  AppListItemView* view = new AppListItemView(this, original_drag_view->item());
  AddChildView(view);
  drag_view_ = view;
  drag_view_->SetPaintToLayer(true);
  // Note: For testing purpose, SetFillsBoundsOpaquely can be set to true to
  // show the gray background.
  drag_view_->SetFillsBoundsOpaquely(false);
  drag_view_->SetIconSize(icon_size_);
  drag_view_->SetBoundsRect(drag_view_rect);
  drag_view_->SetDragUIState();  // Hide the title of the drag_view_.

  // Hide the drag_view_ for drag icon proxy.
  SetViewHidden(drag_view_,
                true /* hide */,
                true /* no animate */);

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
  DCHECK(drag_view_);
  DCHECK(IsDraggingForReparentInRootLevelGridView());

  UpdateDrag(pointer, drag_point);
}

bool AppsGridView::IsDraggedView(const views::View* view) const {
  return drag_view_ == view;
}

void AppsGridView::ClearDragState() {
  drop_attempt_ = DROP_FOR_NONE;
  drag_pointer_ = NONE;
  drop_target_ = Index();
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

void AppsGridView::Prerender(int page_index) {
  Layout();
  int start = std::max(0, (page_index - kPrerenderPages) * tiles_per_page());
  int end = std::min(view_model_.view_size(),
                     (page_index + 1 + kPrerenderPages) * tiles_per_page());
  for (int i = start; i < end; i++) {
    AppListItemView* v = static_cast<AppListItemView*>(view_model_.view_at(i));
    v->Prerender();
  }
}

bool AppsGridView::IsAnimatingView(views::View* view) {
  return bounds_animator_.IsAnimating(view);
}

gfx::Size AppsGridView::GetPreferredSize() const {
  const gfx::Insets insets(GetInsets());
  const gfx::Size tile_size = gfx::Size(kPreferredTileWidth,
                                        kPreferredTileHeight);
  const int page_switcher_height =
      page_switcher_view_->GetPreferredSize().height();
  return gfx::Size(
      tile_size.width() * cols_ + insets.width(),
      tile_size.height() * rows_per_page_ +
          page_switcher_height + insets.height());
}

bool AppsGridView::GetDropFormats(
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
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
    views::View* view = view_model_.view_at(i);
    if (view != drag_view_)
      view->SetBoundsRect(view_model_.ideal_bounds(i));
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model_);

  const int page_switcher_height =
      page_switcher_view_->GetPreferredSize().height();
  gfx::Rect rect(GetContentsBounds());
  rect.set_y(rect.bottom() - page_switcher_height);
  rect.set_height(page_switcher_height);
  page_switcher_view_->SetBoundsRect(rect);
}

bool AppsGridView::OnKeyPressed(const ui::KeyEvent& event) {
  bool handled = false;
  if (selected_view_)
    handled = selected_view_->OnKeyPressed(event);

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

void AppsGridView::Update() {
  DCHECK(!selected_view_ && !drag_view_);
  view_model_.Clear();
  if (!item_list_ || !item_list_->item_count())
    return;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    views::View* view = CreateViewForItemAtIndex(i);
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
    views::View* view = pulsing_blocks_model_.view_at(0);
    pulsing_blocks_model_.Remove(0);
    delete view;
  }

  while (pulsing_blocks_model_.view_size() < desired) {
    views::View* view = new PulsingBlockView(
        gfx::Size(kPreferredTileWidth, kPreferredTileHeight), true);
    pulsing_blocks_model_.Add(view, 0);
    AddChildView(view);
  }
}

views::View* AppsGridView::CreateViewForItemAtIndex(size_t index) {
  // The drag_view_ might be pending for deletion, therefore view_model_
  // may have one more item than item_list_.
  DCHECK_LE(index, item_list_->item_count());
  AppListItemView* view = new AppListItemView(this,
                                              item_list_->item_at(index));
  view->SetIconSize(icon_size_);
  view->SetPaintToLayer(true);
  view->SetFillsBoundsOpaquely(false);
  return view;
}

AppsGridView::Index AppsGridView::GetIndexFromModelIndex(
    int model_index) const {
  return Index(model_index / tiles_per_page(), model_index % tiles_per_page());
}

int AppsGridView::GetModelIndexFromIndex(const Index& index) const {
  return index.page * tiles_per_page() + index.slot;
}

void AppsGridView::SetSelectedItemByIndex(const Index& index) {
  if (GetIndexOfView(selected_view_) == index)
    return;

  views::View* new_selection = GetViewAtIndex(index);
  if (!new_selection)
    return;  // Keep current selection.

  if (selected_view_)
    selected_view_->SchedulePaint();

  EnsureViewVisible(new_selection);
  selected_view_ = new_selection;
  selected_view_->SchedulePaint();
  selected_view_->NotifyAccessibilityEvent(
      ui::AX_EVENT_FOCUS, true);
}

bool AppsGridView::IsValidIndex(const Index& index) const {
  return index.page >= 0 && index.page < pagination_model_.total_pages() &&
         index.slot >= 0 && index.slot < tiles_per_page() &&
         GetModelIndexFromIndex(index) < view_model_.view_size();
}

AppsGridView::Index AppsGridView::GetIndexOfView(
    const views::View* view) const {
  const int model_index = view_model_.GetIndexOfView(view);
  if (model_index == -1)
    return Index();

  return GetIndexFromModelIndex(model_index);
}

views::View* AppsGridView::GetViewAtIndex(const Index& index) const {
  if (!IsValidIndex(index))
    return NULL;

  const int model_index = GetModelIndexFromIndex(index);
  return view_model_.view_at(model_index);
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
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Size tile_size(kPreferredTileWidth, kPreferredTileHeight);

  gfx::Rect grid_rect(gfx::Size(tile_size.width() * cols_,
                                tile_size.height() * rows_per_page_));
  grid_rect.Intersect(rect);

  // Page width including padding pixels. A tile.x + page_width means the same
  // tile slot in the next page.
  const int page_width = grid_rect.width() + kPagePadding;

  // If there is a transition, calculates offset for current and target page.
  const int current_page = pagination_model_.selected_page();
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  const bool is_valid = pagination_model_.is_valid_page(transition.target_page);

  // Transition to right means negative offset.
  const int dir = transition.target_page > current_page ? -1 : 1;
  const int transition_offset = is_valid ?
      transition.progress * page_width * dir : 0;

  const int total_views =
      view_model_.view_size() + pulsing_blocks_model_.view_size();
  int slot_index = 0;
  for (int i = 0; i < total_views; ++i) {
    if (i < view_model_.view_size() && view_model_.view_at(i) == drag_view_) {
      if (EnableFolderDragDropUI() && drop_attempt_ == DROP_FOR_FOLDER)
        ++slot_index;
      continue;
    }

    Index view_index = GetIndexFromModelIndex(slot_index);

    if (drop_target_ == view_index) {
      if (EnableFolderDragDropUI() && drop_attempt_ == DROP_FOR_FOLDER) {
        view_index = GetIndexFromModelIndex(slot_index);
      } else if (!EnableFolderDragDropUI() ||
                 drop_attempt_ == DROP_FOR_REORDER) {
        ++slot_index;
        view_index = GetIndexFromModelIndex(slot_index);
      }
    }

    // Decides an x_offset for current item.
    int x_offset = 0;
    if (view_index.page < current_page)
      x_offset = -page_width;
    else if (view_index.page > current_page)
      x_offset = page_width;

    if (is_valid) {
      if (view_index.page == current_page ||
          view_index.page == transition.target_page) {
        x_offset += transition_offset;
      }
    }

    const int row = view_index.slot / cols_;
    const int col = view_index.slot % cols_;
    gfx::Rect tile_slot(
        gfx::Point(grid_rect.x() + col * tile_size.width() + x_offset,
                   grid_rect.y() + row * tile_size.height()),
        tile_size);
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
    views::View* view = view_model_.view_at(i);
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
    if (visible && y_diff && y_diff % kPreferredTileHeight == 0) {
      AnimationBetweenRows(view,
                           current_visible,
                           current,
                           target_visible,
                           target);
    } else if (visible || bounds_animator_.IsAnimating(view)) {
      bounds_animator_.AnimateViewTo(view, target);
      bounds_animator_.SetAnimationDelegate(
          view,
          scoped_ptr<gfx::AnimationDelegate>(
              new ItemMoveAnimationDelegate(view)));
    } else {
      view->SetBoundsRect(target);
    }
  }
}

void AppsGridView::AnimationBetweenRows(views::View* view,
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

  scoped_ptr<ui::Layer> layer;
  if (animate_current) {
    layer = view->RecreateLayer();
    layer->SuppressPaint();

    view->SetFillsBoundsOpaquely(false);
    view->layer()->SetOpacity(0.f);
  }

  gfx::Rect current_out(current);
  current_out.Offset(dir * kPreferredTileWidth, 0);

  gfx::Rect target_in(target);
  if (animate_target)
    target_in.Offset(-dir * kPreferredTileWidth, 0);
  view->SetBoundsRect(target_in);
  bounds_animator_.AnimateViewTo(view, target);

  bounds_animator_.SetAnimationDelegate(
      view,
      scoped_ptr<gfx::AnimationDelegate>(
          new RowMoveAnimationDelegate(view, layer.release(), current_out)));
}

void AppsGridView::ExtractDragLocation(const ui::LocatedEvent& event,
                                       gfx::Point* drag_point) {
#if defined(USE_AURA) && !defined(OS_WIN)
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
#else
  // For non-aura, root location is not clearly defined but |drag_view_| does
  // not have the scale transform. So no round error would be introduced and
  // it's okay to use View::ConvertPointToTarget.
  *drag_point = event.location();
  views::View::ConvertPointToTarget(drag_view_, this, drag_point);
#endif
}

void AppsGridView::CalculateDropTarget(const gfx::Point& drag_point,
                                       bool use_page_button_hovering) {
  if (EnableFolderDragDropUI()) {
    CalculateDropTargetWithFolderEnabled(drag_point, use_page_button_hovering);
    return;
  }

  int current_page = pagination_model_.selected_page();
  gfx::Point point(drag_point);
  if (!IsPointWithinDragBuffer(drag_point)) {
    point = drag_start_grid_view_;
    current_page = drag_start_page_;
  }

  if (use_page_button_hovering &&
      page_switcher_view_->bounds().Contains(point)) {
    gfx::Point page_switcher_point(point);
    views::View::ConvertPointToTarget(this, page_switcher_view_,
                                      &page_switcher_point);
    int page = page_switcher_view_->GetPageForPoint(page_switcher_point);
    if (pagination_model_.is_valid_page(page)) {
      drop_target_.page = page;
      drop_target_.slot = tiles_per_page() - 1;
    }
  } else {
    gfx::Rect bounds(GetContentsBounds());
    const int drop_row = (point.y() - bounds.y()) / kPreferredTileHeight;
    const int drop_col = std::min(cols_ - 1,
        (point.x() - bounds.x()) / kPreferredTileWidth);

    drop_target_.page = current_page;
    drop_target_.slot = std::max(0, std::min(
        tiles_per_page() - 1,
        drop_row * cols_ + drop_col));
  }

  // Limits to the last possible slot on last page.
  if (drop_target_.page == pagination_model_.total_pages() - 1) {
    drop_target_.slot = std::min(
        (view_model_.view_size() - 1) % tiles_per_page(),
        drop_target_.slot);
  }
}


void AppsGridView::CalculateDropTargetWithFolderEnabled(
    const gfx::Point& drag_point,
    bool use_page_button_hovering) {
  gfx::Point point(drag_point);
  if (!IsPointWithinDragBuffer(drag_point)) {
    point = drag_start_grid_view_;
  }

  if (use_page_button_hovering &&
      page_switcher_view_->bounds().Contains(point)) {
    gfx::Point page_switcher_point(point);
    views::View::ConvertPointToTarget(this, page_switcher_view_,
                                      &page_switcher_point);
    int page = page_switcher_view_->GetPageForPoint(page_switcher_point);
    if (pagination_model_.is_valid_page(page))
      drop_attempt_ = DROP_FOR_NONE;
  } else {
    DCHECK(drag_view_);
    // Try to find the nearest target for folder dropping or re-ordering.
    drop_target_ = GetNearestTileForDragView();
  }
}

void AppsGridView::OnReorderTimer() {
  if (drop_attempt_ == DROP_FOR_REORDER)
    AnimateToIdealBounds();
}

void AppsGridView::OnFolderItemReparentTimer() {
  DCHECK(folder_delegate_);
  if (drag_out_of_folder_container_ && drag_view_) {
    folder_delegate_->ReparentItem(drag_view_, last_drag_point_);

    // Set the flag in the folder's grid view.
    dragging_for_reparent_item_ = true;

    // Do not observe any data change since it is going to be hidden.
    item_list_->RemoveObserver(this);
    item_list_ = NULL;
  }
}

void AppsGridView::OnFolderDroppingTimer() {
  if (drop_attempt_ == DROP_FOR_FOLDER)
    SetAsFolderDroppingTarget(drop_target_, true);
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
    CalculateDropTarget(last_drag_point_, true);
    if (IsValidIndex(drop_target_)) {
      if (drop_attempt_ == DROP_FOR_REORDER) {
        ReparentItemForReorder(drag_view_, drop_target_);
      } else if (drop_attempt_ == DROP_FOR_FOLDER) {
        ReparentItemToAnotherFolder(drag_view_, drop_target_);
      }
    }
    SetViewHidden(drag_view_, false /* show */, true /* no animate */);
  }

  // The drag can be ended after the synchronous drag is created but before it
  // is Run().
  CleanUpSynchronousDrag();

  SetAsFolderDroppingTarget(drop_target_, false);
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

  // The drag can be ended after the synchronous drag is created but before it
  // is Run().
  CleanUpSynchronousDrag();

  SetAsFolderDroppingTarget(drop_target_, false);
  ClearDragState();
}

void AppsGridView::OnFolderItemRemoved() {
  DCHECK(folder_delegate_);
  item_list_ = NULL;
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

  if (page_switcher_view_->bounds().Contains(drag_point)) {
    gfx::Point page_switcher_point(drag_point);
    views::View::ConvertPointToTarget(this, page_switcher_view_,
                                      &page_switcher_point);
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

void AppsGridView::MoveItemInModel(views::View* item_view,
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

void AppsGridView::MoveItemToFolder(views::View* item_view,
                                    const Index& target) {
  const std::string& source_item_id =
      static_cast<AppListItemView*>(item_view)->item()->id();
  AppListItemView* target_view =
      static_cast<AppListItemView*>(GetViewAtSlotOnCurrentPage(target.slot));
  const std::string&  target_view_item_id = target_view->item()->id();

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
      views::View* target_folder_view =
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
      drag_view_,
      scoped_ptr<gfx::AnimationDelegate>(
          new ItemRemoveAnimationDelegate(drag_view_)));
  UpdatePaging();
}

void AppsGridView::ReparentItemForReorder(views::View* item_view,
                                          const Index& target) {
  item_list_->RemoveObserver(this);
  model_->RemoveObserver(this);

  AppListItem* reparent_item = static_cast<AppListItemView*>(item_view)->item();
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

void AppsGridView::ReparentItemToAnotherFolder(views::View* item_view,
                                               const Index& target) {
  DCHECK(IsDraggingForReparentInRootLevelGridView());

  AppListItemView* target_view =
      static_cast<AppListItemView*>(GetViewAtSlotOnCurrentPage(target.slot));
  if (!target_view)
    return;

  // Make change to data model.
  item_list_->RemoveObserver(this);

  AppListItem* reparent_item = static_cast<AppListItemView*>(item_view)->item();
  DCHECK(reparent_item->IsInFolder());
  const std::string source_folder_id = reparent_item->folder_id();
  AppListFolderItem* source_folder =
      static_cast<AppListFolderItem*>(item_list_->FindItem(source_folder_id));

  // Remove the source folder view if there is only 1 item in it, since the
  // source folder will be deleted after its only child item merged into the
  // target item.
  if (source_folder->ChildItemCount() == 1u)
    DeleteItemViewAtIndex(
        view_model_.GetIndexOfView(activated_folder_item_view()));

  AppListItem* target_item = target_view->item();

  // Move item to the target folder.
  std::string target_id_after_merge =
      model_->MergeItems(target_item->id(), reparent_item->id());
  if (target_id_after_merge.empty()) {
    LOG(ERROR) << "Unable to reparent to item id: " << target_item->id();
    item_list_->AddObserver(this);
    return;
  }

  if (target_id_after_merge != target_item->id()) {
    // New folder was created, change the view model to replace the old target
    // view with the new folder item view.
    const std::string& new_folder_id = reparent_item->folder_id();
    size_t new_folder_index;
    if (item_list_->FindItemIndex(new_folder_id, &new_folder_index)) {
      int target_view_index = view_model_.GetIndexOfView(target_view);
      DeleteItemViewAtIndex(target_view_index);
      views::View* new_folder_view =
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
      drag_view_,
      scoped_ptr<gfx::AnimationDelegate>(
          new ItemRemoveAnimationDelegate(drag_view_)));
  UpdatePaging();
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
  views::View* last_item_view = CreateViewForItemAtIndex(last_item_index);
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
        gfx::Size(kPreferredIconDimension, kPreferredIconDimension));
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
  for (int i = start; i < end; ++i) {
    AppListItemView* view =
        static_cast<AppListItemView*>(view_model_.view_at(i));
    view->CancelContextMenu();
  }
}

void AppsGridView::DeleteItemViewAtIndex(int index) {
  views::View* item_view = view_model_.view_at(index);
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

  if (delegate_) {
    // Always set the previous activated_folder_item_view_ to be visible. This
    // prevents a case where the item would remain hidden due the
    // |activated_folder_item_view_| changing during the animation. We only
    // need to track |activated_folder_item_view_| in the root level grid view.
    if (!folder_delegate_) {
      if (activated_folder_item_view_)
        activated_folder_item_view_->SetVisible(true);
      AppListItemView* pressed_item_view =
          static_cast<AppListItemView*>(sender);
      if (IsFolderItem(pressed_item_view->item()))
        activated_folder_item_view_ = pressed_item_view;
      else
        activated_folder_item_view_ = NULL;
    }
    delegate_->ActivateApp(static_cast<AppListItemView*>(sender)->item(),
                           event.flags());
  }
}

void AppsGridView::OnListItemAdded(size_t index, AppListItem* item) {
  EndDrag(true);

  views::View* view = CreateViewForItemAtIndex(index);
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

void AppsGridView::TotalPagesChanged() {
}

void AppsGridView::SelectedPageChanged(int old_selected, int new_selected) {
  if (dragging()) {
    CalculateDropTarget(last_drag_point_, true);
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

void AppsGridView::SetViewHidden(views::View* view, bool hide, bool immediate) {
  ui::ScopedLayerAnimationSettings animator(view->layer()->GetAnimator());
  animator.SetPreemptionStrategy(
      immediate ? ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET :
                  ui::LayerAnimator::BLEND_WITH_CURRENT_ANIMATION);
  view->layer()->SetOpacity(hide ? 0 : 1);
}

void AppsGridView::OnImplicitAnimationsCompleted() {
  if (layer()->opacity() == 0.0f)
    SetVisible(false);
}

bool AppsGridView::EnableFolderDragDropUI() {
  // Enable drag and drop folder UI only if it is at the app list root level
  // and the switch is on and the target folder can still accept new items.
  return model_->folders_enabled() && !folder_delegate_ &&
      CanDropIntoTarget(drop_target_);
}

bool AppsGridView::CanDropIntoTarget(const Index& drop_target) {
  views::View* target_view = GetViewAtSlotOnCurrentPage(drop_target.slot);
  if (!target_view)
    return true;

  AppListItem* target_item =
      static_cast<AppListItemView*>(target_view)->item();
  // Items can be dropped into non-folders (which have no children) or folders
  // that have fewer than the max allowed items.
  // OEM folder does not allow to drag/drop other items in it.
  return target_item->ChildItemCount() < kMaxFolderItems &&
         !IsOEMFolderItem(target_item);
}

// TODO(jennyz): Optimize the calculation for finding nearest tile.
AppsGridView::Index AppsGridView::GetNearestTileForDragView() {
  Index nearest_tile;
  nearest_tile.page = -1;
  nearest_tile.slot = -1;
  int d_min = -1;

  // Calculate the top left tile |drag_view| intersects.
  gfx::Point pt = drag_view_->bounds().origin();
  CalculateNearestTileForVertex(pt, &nearest_tile, &d_min);

  // Calculate the top right tile |drag_view| intersects.
  pt = drag_view_->bounds().top_right();
  CalculateNearestTileForVertex(pt, &nearest_tile, &d_min);

  // Calculate the bottom left tile |drag_view| intersects.
  pt = drag_view_->bounds().bottom_left();
  CalculateNearestTileForVertex(pt, &nearest_tile, &d_min);

  // Calculate the bottom right tile |drag_view| intersects.
  pt = drag_view_->bounds().bottom_right();
  CalculateNearestTileForVertex(pt, &nearest_tile, &d_min);

  const int d_folder_dropping =
      kFolderDroppingCircleRadius + kPreferredIconDimension / 2;
  const int d_reorder =
      kReorderDroppingCircleRadius + kPreferredIconDimension / 2;

  // If user drags an item across pages to the last page, and targets it
  // to the last empty slot on it, push the last item for re-ordering.
  if (IsLastPossibleDropTarget(nearest_tile) && d_min < d_reorder) {
    drop_attempt_ = DROP_FOR_REORDER;
    nearest_tile.slot = nearest_tile.slot - 1;
    return nearest_tile;
  }

  if (IsValidIndex(nearest_tile)) {
    if (d_min < d_folder_dropping) {
      views::View* target_view = GetViewAtSlotOnCurrentPage(nearest_tile.slot);
      if (target_view &&
          !IsFolderItem(static_cast<AppListItemView*>(drag_view_)->item())) {
        // If a non-folder item is dragged to the target slot with an item
        // sitting on it, attempt to drop the dragged item into the folder
        // containing the item on nearest_tile.
        drop_attempt_ = DROP_FOR_FOLDER;
        return nearest_tile;
      } else {
        // If the target slot is blank, or the dragged item is a folder, attempt
        // to re-order.
        drop_attempt_ = DROP_FOR_REORDER;
        return nearest_tile;
      }
    } else if (d_min < d_reorder) {
      // Entering the re-order circle of the slot.
      drop_attempt_ = DROP_FOR_REORDER;
      return nearest_tile;
    }
  }

  // If |drag_view| is not entering the re-order or fold dropping region of
  // any items, cancel any previous re-order or folder dropping timer, and
  // return itself.
  drop_attempt_ = DROP_FOR_NONE;
  reorder_timer_.Stop();
  folder_dropping_timer_.Stop();

  // When dragging for reparent a folder item, it should go back to its parent
  // folder item if there is no drop target.
  if (IsDraggingForReparentInRootLevelGridView()) {
    DCHECK(activated_folder_item_view_);
    return GetIndexOfView(activated_folder_item_view_);
  }

  return GetIndexOfView(drag_view_);
}

void AppsGridView::CalculateNearestTileForVertex(const gfx::Point& vertex,
                                                 Index* nearest_tile,
                                                 int* d_min) {
  Index target_index;
  gfx::Rect target_bounds = GetTileBoundsForPoint(vertex, &target_index);

  if (target_bounds.IsEmpty() || target_index == *nearest_tile)
    return;

  // Do not count the tile, where drag_view_ used to sit on and is still moving
  // on top of it, in calculating nearest tile for drag_view_.
  views::View* target_view = GetViewAtSlotOnCurrentPage(target_index.slot);
  if (target_index == drag_view_init_index_ && !target_view &&
      !IsDraggingForReparentInRootLevelGridView()) {
    return;
  }

  int d_center = GetDistanceBetweenRects(drag_view_->bounds(), target_bounds);
  if (*d_min < 0 || d_center < *d_min) {
    *d_min = d_center;
    *nearest_tile = target_index;
  }
}

gfx::Rect AppsGridView::GetTileBoundsForPoint(const gfx::Point& point,
                                              Index *tile_index) {
  // Check if |point| is outside of contents bounds.
  gfx::Rect bounds(GetContentsBounds());
  if (!bounds.Contains(point))
    return gfx::Rect();

  // Calculate which tile |point| is enclosed in.
  int x = point.x();
  int y = point.y();
  int col = (x - bounds.x()) / kPreferredTileWidth;
  int row = (y - bounds.y()) / kPreferredTileHeight;
  gfx::Rect tile_rect = GetTileBounds(row, col);

  // Check if |point| is outside a valid item's tile.
  Index index(pagination_model_.selected_page(), row * cols_ + col);
  *tile_index = index;
  return tile_rect;
}

gfx::Rect AppsGridView::GetTileBounds(int row, int col) const {
  gfx::Rect bounds(GetContentsBounds());
  gfx::Size tile_size(kPreferredTileWidth, kPreferredTileHeight);
  gfx::Rect grid_rect(gfx::Size(tile_size.width() * cols_,
                                tile_size.height() * rows_per_page_));
  grid_rect.Intersect(bounds);
  gfx::Rect tile_rect(
      gfx::Point(grid_rect.x() + col * tile_size.width(),
                 grid_rect.y() + row * tile_size.height()),
      tile_size);
  return tile_rect;
}

bool AppsGridView::IsLastPossibleDropTarget(const Index& index) const {
  int last_possible_slot = view_model_.view_size() % tiles_per_page();
  return (index.page == pagination_model_.total_pages() - 1 &&
          index.slot == last_possible_slot + 1);
}

views::View* AppsGridView::GetViewAtSlotOnCurrentPage(int slot) {
  if (slot < 0)
    return NULL;

  // Calculate the original bound of the tile at |index|.
  int row = slot / cols_;
  int col = slot % cols_;
  gfx::Rect tile_rect = GetTileBounds(row, col);

  for (int i = 0; i < view_model_.view_size(); ++i) {
    views::View* view = view_model_.view_at(i);
    if (view->bounds() == tile_rect && view != drag_view_)
      return view;
  }
  return NULL;
}

void AppsGridView::SetAsFolderDroppingTarget(const Index& target_index,
                                             bool is_target_folder) {
  AppListItemView* target_view =
      static_cast<AppListItemView*>(
          GetViewAtSlotOnCurrentPage(target_index.slot));
  if (target_view)
    target_view->SetAsAttemptedFolderTarget(is_target_folder);
}

}  // namespace app_list
