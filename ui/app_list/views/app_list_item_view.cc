// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_item_view.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/cached_label.h"
#include "ui/app_list/views/progress_bar_view.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/point.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/transform_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/drag_controller.h"

namespace app_list {

namespace {

const int kTopPadding = 20;
const int kIconTitleSpacing = 7;
const int kProgressBarHorizontalPadding = 12;

// Radius of the folder dropping preview circle.
const int kFolderPreviewRadius = 40;

const int kLeftRightPaddingChars = 1;

// Scale to transform the icon when a drag starts.
const float kDraggingIconScale = 1.5f;

// Delay in milliseconds of when the dragging UI should be shown for mouse drag.
const int kMouseDragUIDelayInMs = 200;

const gfx::ShadowValues& GetIconShadows() {
  CR_DEFINE_STATIC_LOCAL(
      const gfx::ShadowValues,
      icon_shadows,
      (1,
       gfx::ShadowValue(gfx::Point(0, 2), 2, SkColorSetARGB(0x24, 0, 0, 0))));
  return icon_shadows;
}

gfx::FontList GetFontList() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& font_list = rb.GetFontList(kItemTextFontStyle);
// The font is different on each platform. The font size is adjusted on some
// platforms to keep a consistent look.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Reducing the font size by 2 makes it the same as the Windows font size.
  const int kFontSizeDelta = -2;
  return font_list.DeriveWithSizeDelta(kFontSizeDelta);
#else
  return font_list;
#endif
}

}  // namespace

// static
const char AppListItemView::kViewClassName[] = "ui/app_list/AppListItemView";

AppListItemView::AppListItemView(AppsGridView* apps_grid_view,
                                 AppListItem* item)
    : CustomButton(apps_grid_view),
      is_folder_(item->GetItemType() == AppListFolderItem::kItemType),
      is_in_folder_(item->IsInFolder()),
      item_weak_(item),
      apps_grid_view_(apps_grid_view),
      icon_(new views::ImageView),
      title_(new CachedLabel),
      progress_bar_(new ProgressBarView),
      ui_state_(UI_STATE_NORMAL),
      touch_dragging_(false),
      is_installing_(false),
      is_highlighted_(false) {
  icon_->set_interactive(false);

  title_->SetBackgroundColor(0);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColor(kGridTitleColor);

  static const gfx::FontList font_list = GetFontList();
  title_->SetFontList(font_list);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->Invalidate();
  SetTitleSubpixelAA();

  AddChildView(icon_);
  AddChildView(title_);
  AddChildView(progress_bar_);

  SetIcon(item->icon(), item->has_shadow());
  SetItemName(base::UTF8ToUTF16(item->GetDisplayName()),
              base::UTF8ToUTF16(item->name()));
  SetItemIsInstalling(item->is_installing());
  SetItemIsHighlighted(item->highlighted());
  item->AddObserver(this);

  set_context_menu_controller(this);
  set_request_focus_on_press(false);

  SetAnimationDuration(0);
}

AppListItemView::~AppListItemView() {
  if (item_weak_)
    item_weak_->RemoveObserver(this);
}

void AppListItemView::SetIcon(const gfx::ImageSkia& icon, bool has_shadow) {
  // Clear icon and bail out if item icon is empty.
  if (icon.isNull()) {
    icon_->SetImage(NULL);
    return;
  }

  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      icon,
      skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kGridIconDimension, kGridIconDimension)));
  if (has_shadow) {
    gfx::ImageSkia shadow(gfx::ImageSkiaOperations::CreateImageWithDropShadow(
        resized, GetIconShadows()));
    icon_->SetImage(shadow);
    return;
  }

  icon_->SetImage(resized);
}

void AppListItemView::SetUIState(UIState state) {
  if (ui_state_ == state)
    return;

  ui_state_ = state;

  switch (ui_state_) {
    case UI_STATE_NORMAL:
      title_->SetVisible(!is_installing_);
      progress_bar_->SetVisible(is_installing_);
      break;
    case UI_STATE_DRAGGING:
      title_->SetVisible(false);
      progress_bar_->SetVisible(false);
      break;
    case UI_STATE_DROPPING_IN_FOLDER:
      break;
  }
#if !defined(OS_WIN)
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  switch (ui_state_) {
    case UI_STATE_NORMAL:
      layer()->SetTransform(gfx::Transform());
      break;
    case UI_STATE_DRAGGING: {
      const gfx::Rect bounds(layer()->bounds().size());
      layer()->SetTransform(gfx::GetScaleTransform(
          bounds.CenterPoint(),
          kDraggingIconScale));
      break;
    }
    case UI_STATE_DROPPING_IN_FOLDER:
      break;
  }
#endif  // !OS_WIN

  SchedulePaint();
}

void AppListItemView::SetTouchDragging(bool touch_dragging) {
  if (touch_dragging_ == touch_dragging)
    return;

  touch_dragging_ = touch_dragging;
  SetUIState(touch_dragging_ ? UI_STATE_DRAGGING : UI_STATE_NORMAL);
}

void AppListItemView::OnMouseDragTimer() {
  DCHECK(apps_grid_view_->IsDraggedView(this));
  SetUIState(UI_STATE_DRAGGING);
}

void AppListItemView::SetTitleSubpixelAA() {
  // TODO(tapted): Enable AA for folders as well, taking care to play nice with
  // the folder bubble animation.
  bool enable_aa = !is_in_folder_ && ui_state_ == UI_STATE_NORMAL &&
                   !is_highlighted_ && !apps_grid_view_->IsSelectedView(this) &&
                   !apps_grid_view_->IsAnimatingView(this);

  bool currently_enabled = title_->background() != NULL;
  if (currently_enabled == enable_aa)
    return;

  if (enable_aa) {
    title_->SetBackgroundColor(app_list::kContentsBackgroundColor);
    title_->set_background(views::Background::CreateSolidBackground(
        app_list::kContentsBackgroundColor));
  } else {
    // In other cases, keep the background transparent to ensure correct
    // interactions with animations. This will temporarily disable subpixel AA.
    title_->SetBackgroundColor(0);
    title_->set_background(NULL);
  }
  title_->Invalidate();
  title_->SchedulePaint();
}

void AppListItemView::Prerender() {
  title_->PaintToBackingImage();
}

void AppListItemView::CancelContextMenu() {
  if (context_menu_runner_)
    context_menu_runner_->Cancel();
}

gfx::ImageSkia AppListItemView::GetDragImage() {
  return icon_->GetImage();
}

void AppListItemView::OnDragEnded() {
  mouse_drag_timer_.Stop();
  SetUIState(UI_STATE_NORMAL);
}

gfx::Point AppListItemView::GetDragImageOffset() {
  gfx::Point image = icon_->GetImageBounds().origin();
  return gfx::Point(icon_->x() + image.x(), icon_->y() + image.y());
}

void AppListItemView::SetAsAttemptedFolderTarget(bool is_target_folder) {
  if (is_target_folder)
    SetUIState(UI_STATE_DROPPING_IN_FOLDER);
  else
    SetUIState(UI_STATE_NORMAL);
}

void AppListItemView::SetItemName(const base::string16& display_name,
                                  const base::string16& full_name) {
  title_->SetText(display_name);
  title_->Invalidate();

  title_->SetTooltipText(display_name == full_name ? base::string16()
                                                   : full_name);

  // Use full name for accessibility.
  SetAccessibleName(
      is_folder_ ? l10n_util::GetStringFUTF16(
                       IDS_APP_LIST_FOLDER_BUTTON_ACCESSIBILE_NAME, full_name)
                 : full_name);
  Layout();
}

void AppListItemView::SetItemIsHighlighted(bool is_highlighted) {
  is_highlighted_ = is_highlighted;
  apps_grid_view_->EnsureViewVisible(this);
  SchedulePaint();
}

void AppListItemView::SetItemIsInstalling(bool is_installing) {
  is_installing_ = is_installing;
  if (is_installing_)
    apps_grid_view_->EnsureViewVisible(this);

  if (ui_state_ == UI_STATE_NORMAL) {
    title_->SetVisible(!is_installing);
    progress_bar_->SetVisible(is_installing);
  }
  SchedulePaint();
}

void AppListItemView::SetItemPercentDownloaded(int percent_downloaded) {
  // A percent_downloaded() of -1 can mean it's not known how much percent is
  // completed, or the download hasn't been marked complete, as is the case
  // while an extension is being installed after being downloaded.
  if (percent_downloaded == -1)
    return;
  progress_bar_->SetValue(percent_downloaded / 100.0);
}

const char* AppListItemView::GetClassName() const {
  return kViewClassName;
}

void AppListItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());

  const int left_right_padding =
      title_->font_list().GetExpectedTextWidth(kLeftRightPaddingChars);
  rect.Inset(left_right_padding, kTopPadding, left_right_padding, 0);
  const int y = rect.y();

  icon_->SetBoundsRect(GetIconBoundsForTargetViewBounds(GetContentsBounds()));
  const gfx::Size title_size = title_->GetPreferredSize();
  gfx::Rect title_bounds(rect.x() + (rect.width() - title_size.width()) / 2,
                         y + kGridIconDimension + kIconTitleSpacing,
                         title_size.width(),
                         title_size.height());
  title_bounds.Intersect(rect);
  title_->SetBoundsRect(title_bounds);

  gfx::Rect progress_bar_bounds(progress_bar_->GetPreferredSize());
  progress_bar_bounds.set_x(GetContentsBounds().x() +
                            kProgressBarHorizontalPadding);
  progress_bar_bounds.set_y(title_bounds.y());
  progress_bar_->SetBoundsRect(progress_bar_bounds);
}

void AppListItemView::SchedulePaintInRect(const gfx::Rect& r) {
  SetTitleSubpixelAA();
  views::CustomButton::SchedulePaintInRect(r);
}

void AppListItemView::OnPaint(gfx::Canvas* canvas) {
  if (apps_grid_view_->IsDraggedView(this))
    return;

  gfx::Rect rect(GetContentsBounds());
  if (is_highlighted_ && !is_installing_) {
    canvas->FillRect(rect, kHighlightedColor);
    return;
  }
  if (apps_grid_view_->IsSelectedView(this))
    canvas->FillRect(rect, kSelectedColor);

  if (ui_state_ == UI_STATE_DROPPING_IN_FOLDER) {
    DCHECK(apps_grid_view_->model()->folders_enabled());

    // Draw folder dropping preview circle.
    gfx::Point center = gfx::Point(icon_->x() + icon_->size().width() / 2,
                                   icon_->y() + icon_->size().height() / 2);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setColor(kFolderBubbleColor);
    canvas->DrawCircle(center, kFolderPreviewRadius, paint);
  }
}

void AppListItemView::ShowContextMenuForView(views::View* source,
                                             const gfx::Point& point,
                                             ui::MenuSourceType source_type) {
  ui::MenuModel* menu_model =
      item_weak_ ? item_weak_->GetContextMenuModel() : NULL;
  if (!menu_model)
    return;

  context_menu_runner_.reset(
      new views::MenuRunner(menu_model, views::MenuRunner::HAS_MNEMONICS));
  if (context_menu_runner_->RunMenuAt(GetWidget(),
                                      NULL,
                                      gfx::Rect(point, gfx::Size()),
                                      views::MENU_ANCHOR_TOPLEFT,
                                      source_type) ==
      views::MenuRunner::MENU_DELETED) {
    return;
  }
}

void AppListItemView::StateChanged() {
  const bool is_folder_ui_enabled = apps_grid_view_->model()->folders_enabled();
  if (is_folder_ui_enabled)
    apps_grid_view_->ClearAnySelectedView();

  if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    if (!is_folder_ui_enabled)
      apps_grid_view_->SetSelectedView(this);
    title_->SetEnabledColor(kGridTitleHoverColor);
  } else {
    if (!is_folder_ui_enabled)
      apps_grid_view_->ClearSelectedView(this);
    is_highlighted_ = false;
    if (item_weak_)
      item_weak_->SetHighlighted(false);
    title_->SetEnabledColor(kGridTitleColor);
  }
  title_->Invalidate();
}

bool AppListItemView::ShouldEnterPushedState(const ui::Event& event) {
  // Don't enter pushed state for ET_GESTURE_TAP_DOWN so that hover gray
  // background does not show up during scroll.
  if (event.type() == ui::ET_GESTURE_TAP_DOWN)
    return false;

  return views::CustomButton::ShouldEnterPushedState(event);
}

bool AppListItemView::OnMousePressed(const ui::MouseEvent& event) {
  CustomButton::OnMousePressed(event);

  if (!ShouldEnterPushedState(event))
    return true;

  apps_grid_view_->InitiateDrag(this, AppsGridView::MOUSE, event);

  if (apps_grid_view_->IsDraggedView(this)) {
    mouse_drag_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMouseDragUIDelayInMs),
        this, &AppListItemView::OnMouseDragTimer);
  }
  return true;
}

bool AppListItemView::OnKeyPressed(const ui::KeyEvent& event) {
  // Disable space key to press the button. The keyboard events received
  // by this view are forwarded from a Textfield (SearchBoxView) and key
  // released events are not forwarded. This leaves the button in pressed
  // state.
  if (event.key_code() == ui::VKEY_SPACE)
    return false;

  return CustomButton::OnKeyPressed(event);
}

void AppListItemView::OnMouseReleased(const ui::MouseEvent& event) {
  CustomButton::OnMouseReleased(event);
  apps_grid_view_->EndDrag(false);
}

void AppListItemView::OnMouseCaptureLost() {
  // We don't cancel the dag on mouse capture lost for windows as entering a
  // synchronous drag causes mouse capture to be lost and pressing escape
  // dismisses the app list anyway.
#if !defined(OS_WIN)
  CustomButton::OnMouseCaptureLost();
  apps_grid_view_->EndDrag(true);
#endif
}

bool AppListItemView::OnMouseDragged(const ui::MouseEvent& event) {
  CustomButton::OnMouseDragged(event);
  if (apps_grid_view_->IsDraggedView(this)) {
    // If the drag is no longer happening, it could be because this item
    // got removed, in which case this item has been destroyed. So, bail out
    // now as there will be nothing else to do anyway as
    // apps_grid_view_->dragging() will be false.
    if (!apps_grid_view_->UpdateDragFromItem(AppsGridView::MOUSE, event))
      return true;
  }

  // Shows dragging UI when it's confirmed without waiting for the timer.
  if (ui_state_ != UI_STATE_DRAGGING &&
      apps_grid_view_->dragging() &&
      apps_grid_view_->IsDraggedView(this)) {
    mouse_drag_timer_.Stop();
    SetUIState(UI_STATE_DRAGGING);
  }
  return true;
}

void AppListItemView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (touch_dragging_) {
        apps_grid_view_->InitiateDrag(this, AppsGridView::TOUCH, *event);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (touch_dragging_ && apps_grid_view_->IsDraggedView(this)) {
        apps_grid_view_->UpdateDragFromItem(AppsGridView::TOUCH, *event);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      if (touch_dragging_) {
        SetTouchDragging(false);
        apps_grid_view_->EndDrag(false);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      if (!apps_grid_view_->has_dragged_view())
        SetTouchDragging(true);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_END:
      if (touch_dragging_)
        SetTouchDragging(false);
      break;
    default:
      break;
  }
  if (!event->handled())
    CustomButton::OnGestureEvent(event);
}

void AppListItemView::OnSyncDragEnd() {
  SetUIState(UI_STATE_NORMAL);
}

const gfx::Rect& AppListItemView::GetIconBounds() const {
  return icon_->bounds();
}

void AppListItemView::SetDragUIState() {
  SetUIState(UI_STATE_DRAGGING);
}

gfx::Rect AppListItemView::GetIconBoundsForTargetViewBounds(
    const gfx::Rect& target_bounds) {
  gfx::Rect rect(target_bounds);

  const int left_right_padding =
      title_->font_list().GetExpectedTextWidth(kLeftRightPaddingChars);
  rect.Inset(left_right_padding, kTopPadding, left_right_padding, 0);

  gfx::Rect icon_bounds(rect.x(), rect.y(), rect.width(), kGridIconDimension);
  icon_bounds.Inset(gfx::ShadowValue::GetMargin(GetIconShadows()));
  return icon_bounds;
}

void AppListItemView::ItemIconChanged() {
  SetIcon(item_weak_->icon(), item_weak_->has_shadow());
}

void AppListItemView::ItemNameChanged() {
  SetItemName(base::UTF8ToUTF16(item_weak_->GetDisplayName()),
              base::UTF8ToUTF16(item_weak_->name()));
}

void AppListItemView::ItemHighlightedChanged() {
  SetItemIsHighlighted(item_weak_->highlighted());
}

void AppListItemView::ItemIsInstallingChanged() {
  SetItemIsInstalling(item_weak_->is_installing());
}

void AppListItemView::ItemPercentDownloadedChanged() {
  SetItemPercentDownloaded(item_weak_->percent_downloaded());
}

void AppListItemView::ItemBeingDestroyed() {
  DCHECK(item_weak_);
  item_weak_->RemoveObserver(this);
  item_weak_ = NULL;
}

}  // namespace app_list
