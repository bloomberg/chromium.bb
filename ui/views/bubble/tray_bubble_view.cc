// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/tray_bubble_view.h"

#include <algorithm>

#include "base/macros.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/bubble_window_targeter.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"

namespace views {

namespace {

// The sampling time for mouse position changes in ms - which is roughly a frame
// time.
const int kFrameTimeInMS = 30;

BubbleBorder::Arrow GetArrowAlignment(
    TrayBubbleView::AnchorAlignment alignment) {
  if (alignment == TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM) {
    return base::i18n::IsRTL() ? BubbleBorder::BOTTOM_LEFT
                               : BubbleBorder::BOTTOM_RIGHT;
  }
  if (alignment == TrayBubbleView::ANCHOR_ALIGNMENT_LEFT)
    return BubbleBorder::LEFT_BOTTOM;
  return BubbleBorder::RIGHT_BOTTOM;
}

// Only one TrayBubbleView is visible at a time, but there are cases where the
// lifetimes of two different bubbles can overlap briefly.
int g_current_tray_bubble_showing_count_ = 0;

}  // namespace

namespace internal {

// Detects any mouse movement. This is needed to detect mouse movements by the
// user over the bubble if the bubble got created underneath the cursor.
class MouseMoveDetectorHost : public MouseWatcherHost {
 public:
  MouseMoveDetectorHost();
  ~MouseMoveDetectorHost() override;

  bool Contains(const gfx::Point& screen_point, MouseEventType type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MouseMoveDetectorHost);
};

MouseMoveDetectorHost::MouseMoveDetectorHost() {
}

MouseMoveDetectorHost::~MouseMoveDetectorHost() {
}

bool MouseMoveDetectorHost::Contains(const gfx::Point& screen_point,
                                     MouseEventType type) {
  return false;
}

// This mask layer clips the bubble's content so that it does not overwrite the
// rounded bubble corners.
// TODO(miket): This does not work on Windows. Implement layer masking or
// alternate solutions if the TrayBubbleView is needed there in the future.
class TrayBubbleContentMask : public ui::LayerDelegate {
 public:
  explicit TrayBubbleContentMask(int corner_radius);
  ~TrayBubbleContentMask() override;

  ui::Layer* layer() { return &layer_; }

  // Overridden from LayerDelegate.
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;

 private:
  ui::Layer layer_;
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleContentMask);
};

TrayBubbleContentMask::TrayBubbleContentMask(int corner_radius)
    : layer_(ui::LAYER_TEXTURED),
      corner_radius_(corner_radius) {
  layer_.set_delegate(this);
  layer_.SetFillsBoundsOpaquely(false);
}

TrayBubbleContentMask::~TrayBubbleContentMask() {
  layer_.set_delegate(NULL);
}

void TrayBubbleContentMask::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  cc::PaintFlags flags;
  flags.setAlpha(255);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  gfx::Rect rect(layer()->bounds().size());
  recorder.canvas()->DrawRoundRect(rect, corner_radius_, flags);
}

void TrayBubbleContentMask::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  // Redrawing will take care of scale factor change.
}

// Custom layout for the bubble-view. Does the default box-layout if there is
// enough height. Otherwise, makes sure the bottom rows are visible.
class BottomAlignedBoxLayout : public BoxLayout {
 public:
  explicit BottomAlignedBoxLayout(TrayBubbleView* bubble_view)
      : BoxLayout(BoxLayout::kVertical), bubble_view_(bubble_view) {}

  ~BottomAlignedBoxLayout() override {}

 private:
  void Layout(View* host) override {
    if (host->height() >= host->GetPreferredSize().height() ||
        !bubble_view_->is_gesture_dragging()) {
      BoxLayout::Layout(host);
      return;
    }

    int consumed_height = 0;
    for (int i = host->child_count() - 1;
        i >= 0 && consumed_height < host->height(); --i) {
      View* child = host->child_at(i);
      if (!child->visible())
        continue;
      gfx::Size size = child->GetPreferredSize();
      child->SetBounds(0, host->height() - consumed_height - size.height(),
          host->width(), size.height());
      consumed_height += size.height();
    }
  }

  TrayBubbleView* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(BottomAlignedBoxLayout);
};

}  // namespace internal

using internal::TrayBubbleContentMask;
using internal::BottomAlignedBoxLayout;

TrayBubbleView::InitParams::InitParams() = default;

TrayBubbleView::InitParams::InitParams(const InitParams& other) = default;

TrayBubbleView::TrayBubbleView(const InitParams& init_params)
    : BubbleDialogDelegateView(init_params.anchor_view,
                               GetArrowAlignment(init_params.anchor_alignment)),
      params_(init_params),
      layout_(new BottomAlignedBoxLayout(this)),
      delegate_(init_params.delegate),
      preferred_width_(init_params.min_width),
      bubble_border_(new BubbleBorder(
          arrow(),
          BubbleBorder::NO_ASSETS,
          init_params.bg_color.value_or(gfx::kPlaceholderColor))),
      owned_bubble_border_(bubble_border_),
      is_gesture_dragging_(false),
      mouse_actively_entered_(false) {
  DCHECK(delegate_);
  DCHECK(params_.parent_window);
  DCHECK(anchor_widget());  // Computed by BubbleDialogDelegateView().
  bubble_border_->set_use_theme_background_color(!init_params.bg_color);
  bubble_border_->set_alignment(BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_border_->set_paint_arrow(BubbleBorder::PAINT_NONE);
  set_parent_window(params_.parent_window);
  set_can_activate(false);
  set_notify_enter_exit_on_child(true);
  set_close_on_deactivate(init_params.close_on_deactivate);
  set_margins(gfx::Insets());
  SetPaintToLayer();

  bubble_content_mask_.reset(
      new TrayBubbleContentMask(bubble_border_->GetBorderCornerRadius()));

  layout_->SetDefaultFlex(1);
  SetLayoutManager(layout_);
}

TrayBubbleView::~TrayBubbleView() {
  mouse_watcher_.reset();
  if (delegate_) {
    delegate_->UnregisterAllAccelerators(this);

    // Inform host items (models) that their views are being destroyed.
    delegate_->BubbleViewDestroyed();
  }
}

// static
bool TrayBubbleView::IsATrayBubbleOpen() {
  return g_current_tray_bubble_showing_count_ > 0;
}

void TrayBubbleView::InitializeAndShowBubble() {
  layer()->parent()->SetMaskLayer(bubble_content_mask_->layer());

  GetWidget()->Show();
  GetWidget()->GetNativeWindow()->SetEventTargeter(
      std::unique_ptr<ui::EventTargeter>(new BubbleWindowTargeter(this)));
  UpdateBubble();

  ++g_current_tray_bubble_showing_count_;

  // If TrayBubbleView cannot be activated, register accelerators to capture key
  // events for activating the view or closing it. TrayBubbleView expects that
  // those accelerators are registered at the global level.
  if (delegate_ && !CanActivate()) {
    delegate_->RegisterAccelerators(
        {ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE),
         ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE),
         ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN)},
        this);
  }
}

void TrayBubbleView::UpdateBubble() {
  if (GetWidget()) {
    SizeToContents();
    bubble_content_mask_->layer()->SetBounds(layer()->bounds());
    GetWidget()->GetRootView()->SchedulePaint();

    // When extra keyboard accessibility is enabled, focus the default item if
    // no item is focused.
    if (delegate_ && delegate_->ShouldEnableExtraKeyboardAccessibility())
      FocusDefaultIfNeeded();
  }
}

void TrayBubbleView::SetMaxHeight(int height) {
  params_.max_height = height;
  if (GetWidget())
    SizeToContents();
}

void TrayBubbleView::SetBottomPadding(int padding) {
  layout_->set_inside_border_insets(gfx::Insets(0, 0, padding, 0));
}

void TrayBubbleView::SetWidth(int width) {
  width = std::max(std::min(width, params_.max_width), params_.min_width);
  if (preferred_width_ == width)
    return;
  preferred_width_ = width;
  if (GetWidget())
    SizeToContents();
}

gfx::Insets TrayBubbleView::GetBorderInsets() const {
  return bubble_border_->GetInsets();
}

void TrayBubbleView::ResetDelegate() {
  if (delegate_)
    delegate_->UnregisterAllAccelerators(this);

  delegate_ = nullptr;
}

int TrayBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void TrayBubbleView::SizeToContents() {
  BubbleDialogDelegateView::SizeToContents();
  bubble_content_mask_->layer()->SetBounds(layer()->bounds());
}

void TrayBubbleView::OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                              Widget* bubble_widget) const {
  // Apply a WM-provided shadow (see ui/wm/core/).
  params->shadow_type = Widget::InitParams::SHADOW_TYPE_DROP;
  params->shadow_elevation = wm::ShadowElevation::LARGE;
}

void TrayBubbleView::OnWidgetClosing(Widget* widget) {
  BubbleDialogDelegateView::OnWidgetClosing(widget);
  --g_current_tray_bubble_showing_count_;
  DCHECK_GE(g_current_tray_bubble_showing_count_, 0);
}

NonClientFrameView* TrayBubbleView::CreateNonClientFrameView(Widget* widget) {
  BubbleFrameView* frame = static_cast<BubbleFrameView*>(
      BubbleDialogDelegateView::CreateNonClientFrameView(widget));
  frame->SetBubbleBorder(std::move(owned_bubble_border_));
  return frame;
}

bool TrayBubbleView::WidgetHasHitTestMask() const {
  return true;
}

void TrayBubbleView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(GetBubbleFrameView()->GetContentsBounds()));
}

base::string16 TrayBubbleView::GetAccessibleWindowTitle() const {
  return delegate_->GetAccessibleNameForBubble();
}

gfx::Size TrayBubbleView::CalculatePreferredSize() const {
  return gfx::Size(preferred_width_, GetHeightForWidth(preferred_width_));
}

gfx::Size TrayBubbleView::GetMaximumSize() const {
  gfx::Size size = GetPreferredSize();
  size.set_width(params_.max_width);
  return size;
}

int TrayBubbleView::GetHeightForWidth(int width) const {
  int height = GetInsets().height();
  width = std::max(width - GetInsets().width(), 0);
  for (int i = 0; i < child_count(); ++i) {
    const View* child = child_at(i);
    if (child->visible())
      height += child->GetHeightForWidth(width);
  }

  return (params_.max_height != 0) ?
      std::min(height, params_.max_height) : height;
}

void TrayBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_watcher_.reset();
  if (delegate_ && !(event.flags() & ui::EF_IS_SYNTHESIZED)) {
    // Coming here the user was actively moving the mouse over the bubble and
    // we inform the delegate that we entered. This will prevent the bubble
    // to auto close.
    delegate_->OnMouseEnteredView();
    mouse_actively_entered_ = true;
  } else {
    // Coming here the bubble got shown and the mouse was 'accidentally' over it
    // which is not a reason to prevent the bubble to auto close. As such we
    // do not call the delegate, but wait for the first mouse move within the
    // bubble. The used MouseWatcher will notify use of a movement and call
    // |MouseMovedOutOfHost|.
    mouse_watcher_.reset(new MouseWatcher(
        new views::internal::MouseMoveDetectorHost(),
        this));
    // Set the mouse sampling frequency to roughly a frame time so that the user
    // cannot see a lag.
    mouse_watcher_->set_notify_on_exit_time(
        base::TimeDelta::FromMilliseconds(kFrameTimeInMS));
    mouse_watcher_->Start();
  }
}

void TrayBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  // If there was a mouse watcher waiting for mouse movements we disable it
  // immediately since we now leave the bubble.
  mouse_watcher_.reset();
  // Do not notify the delegate of an exit if we never told it that we entered.
  if (delegate_ && mouse_actively_entered_)
    delegate_->OnMouseExitedView();
}

void TrayBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (delegate_ && CanActivate()) {
    node_data->role = ui::AX_ROLE_WINDOW;
    node_data->SetName(delegate_->GetAccessibleNameForBubble());
  }
}

void TrayBubbleView::MouseMovedOutOfHost() {
  // The mouse was accidentally over the bubble when it opened and the AutoClose
  // logic was not activated. Now that the user did move the mouse we tell the
  // delegate to disable AutoClose.
  delegate_->OnMouseEnteredView();
  mouse_actively_entered_ = true;
  mouse_watcher_->Stop();
}

bool TrayBubbleView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator == ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)) {
    CloseBubbleView();
    return true;
  }

  if (accelerator == ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE) ||
      accelerator == ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN)) {
    ui::KeyEvent key_event(
        accelerator.key_state() == ui::Accelerator::KeyState::PRESSED
            ? ui::EventType::ET_KEY_PRESSED
            : ui::EventType::ET_KEY_RELEASED,
        accelerator.key_code(), accelerator.modifiers());
    ActivateAndStartNavigation(key_event);
    return true;
  }

  return false;
}

void TrayBubbleView::ChildPreferredSizeChanged(View* child) {
  SizeToContents();
}

void TrayBubbleView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    details.parent->SetPaintToLayer();
    details.parent->layer()->SetMasksToBounds(true);
  }
}

void TrayBubbleView::CloseBubbleView() {
  if (!delegate_)
    return;

  delegate_->UnregisterAllAccelerators(this);
  delegate_->HideBubble(this);
}

void TrayBubbleView::ActivateAndStartNavigation(const ui::KeyEvent& key_event) {
  // No need to explicitly activate the widget. FocusManager will activate it if
  // necessary.
  set_can_activate(true);

  if (!GetWidget()->GetFocusManager()->OnKeyEvent(key_event) && delegate_) {
    // No need to handle accelerators by TrayBubbleView after focus has moved to
    // the widget. The focused view will handle focus traversal.
    // FocusManager::OnKeyEvent returns false when it consumes a key event.
    delegate_->UnregisterAllAccelerators(this);
  }
}

void TrayBubbleView::FocusDefaultIfNeeded() {
  views::FocusManager* manager = GetFocusManager();
  if (!manager || manager->GetFocusedView())
    return;

  views::View* view =
      manager->GetNextFocusableView(nullptr, nullptr, false, false);
  if (!view)
    return;

  // No need to explicitly activate the widget. View::RequestFocus will activate
  // it if necessary.
  set_can_activate(true);

  view->RequestFocus();
}

}  // namespace views
