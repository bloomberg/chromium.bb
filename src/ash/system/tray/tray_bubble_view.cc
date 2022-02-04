// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_view.h"

#include <algorithm>
#include <numeric>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/views_delegate.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

using views::BubbleBorder;
using views::BubbleFrameView;
using views::NonClientFrameView;
using views::View;
using views::ViewsDelegate;
using views::Widget;

namespace ash {

namespace {

BubbleBorder::Arrow GetArrowAlignment(ash::ShelfAlignment alignment) {
  // The tray bubble is in a corner. In this case, we want the arrow to be
  // flush with one side instead of centered on the bubble.
  switch (alignment) {
    case ash::ShelfAlignment::kBottom:
    case ash::ShelfAlignment::kBottomLocked:
      return base::i18n::IsRTL() ? BubbleBorder::BOTTOM_LEFT
                                 : BubbleBorder::BOTTOM_RIGHT;
    case ash::ShelfAlignment::kLeft:
      return BubbleBorder::LEFT_BOTTOM;
    case ash::ShelfAlignment::kRight:
      return BubbleBorder::RIGHT_BOTTOM;
  }
}

// Detects any mouse movement. This is needed to detect mouse movements by the
// user over the bubble if the bubble got created underneath the cursor.
class MouseMoveDetectorHost : public views::MouseWatcherHost {
 public:
  MouseMoveDetectorHost();

  MouseMoveDetectorHost(const MouseMoveDetectorHost&) = delete;
  MouseMoveDetectorHost& operator=(const MouseMoveDetectorHost&) = delete;

  ~MouseMoveDetectorHost() override;

  bool Contains(const gfx::Point& screen_point, EventType type) override;
};

MouseMoveDetectorHost::MouseMoveDetectorHost() {}

MouseMoveDetectorHost::~MouseMoveDetectorHost() {}

bool MouseMoveDetectorHost::Contains(const gfx::Point& screen_point,
                                     EventType type) {
  return false;
}

// Custom layout for the bubble-view. Does the default box-layout if there is
// enough height. Otherwise, makes sure the bottom rows are visible.
class BottomAlignedBoxLayout : public views::BoxLayout {
 public:
  explicit BottomAlignedBoxLayout(TrayBubbleView* bubble_view)
      : BoxLayout(BoxLayout::Orientation::kVertical),
        bubble_view_(bubble_view) {}

  BottomAlignedBoxLayout(const BottomAlignedBoxLayout&) = delete;
  BottomAlignedBoxLayout& operator=(const BottomAlignedBoxLayout&) = delete;

  ~BottomAlignedBoxLayout() override {}

 private:
  void Layout(View* host) override {
    if (host->height() >= host->GetPreferredSize().height() ||
        !bubble_view_->is_gesture_dragging()) {
      BoxLayout::Layout(host);
      return;
    }

    int consumed_height = 0;
    for (auto i = host->children().rbegin();
         i != host->children().rend() && consumed_height < host->height();
         ++i) {
      View* child = *i;
      if (!child->GetVisible())
        continue;
      gfx::Size size = child->GetPreferredSize();
      child->SetBounds(0, host->height() - consumed_height - size.height(),
                       host->width(), size.height());
      consumed_height += size.height();
    }
  }

  TrayBubbleView* bubble_view_;
};

}  // namespace

TrayBubbleView::Delegate::~Delegate() {}

void TrayBubbleView::Delegate::BubbleViewDestroyed() {}

void TrayBubbleView::Delegate::OnMouseEnteredView() {}

void TrayBubbleView::Delegate::OnMouseExitedView() {}

std::u16string TrayBubbleView::Delegate::GetAccessibleNameForBubble() {
  return std::u16string();
}

bool TrayBubbleView::Delegate::ShouldEnableExtraKeyboardAccessibility() {
  return false;
}

void TrayBubbleView::Delegate::HideBubble(const TrayBubbleView* bubble_view) {}

absl::optional<AcceleratorAction>
TrayBubbleView::Delegate::GetAcceleratorAction() const {
  // TODO(crbug/1234891) Make this a pure virtual function so all
  // bubble delegates need to specify accelerator actions.
  return absl::nullopt;
}

TrayBubbleView::InitParams::InitParams() = default;

TrayBubbleView::InitParams::InitParams(const InitParams& other) = default;

TrayBubbleView::RerouteEventHandler::RerouteEventHandler(
    TrayBubbleView* tray_bubble_view)
    : tray_bubble_view_(tray_bubble_view) {
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);
}

TrayBubbleView::RerouteEventHandler::~RerouteEventHandler() {
  aura::Env::GetInstance()->RemovePreTargetHandler(this);
}

void TrayBubbleView::RerouteEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  // Do not handle a key event if it is targeted to the tray or its descendants,
  // or if the target has the tray as a transient ancestor. RerouteEventHandler
  // is for rerouting events which are not targetted to the tray. Those events
  // should be handled by the target.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* tray_window = tray_bubble_view_->GetWidget()->GetNativeView();
  if (target && (tray_window->Contains(target) ||
                 wm::HasTransientAncestor(target, tray_window))) {
    return;
  }

  // Only passes Tab, Shift+Tab, Esc to the widget as it can consume more key
  // events. e.g. Alt+Tab can be consumed as focus traversal by FocusManager.
  ui::KeyboardCode key_code = event->key_code();
  int flags = event->flags() &
              (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
               ui::EF_COMMAND_DOWN | ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN);
  if ((key_code == ui::VKEY_TAB && flags == ui::EF_NONE) ||
      (key_code == ui::VKEY_TAB && flags == ui::EF_SHIFT_DOWN) ||
      (key_code == ui::VKEY_ESCAPE && flags == ui::EF_NONE) ||
      // Do not dismiss the bubble immediately when a user triggers a feedback
      // report; if they're reporting an issue with the bubble we want the
      // screenshot to contain it.
      (key_code == ui::VKEY_I &&
       flags == (ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN))) {
    // Make TrayBubbleView activatable as the following Widget::OnKeyEvent might
    // try to activate it.
    tray_bubble_view_->SetCanActivate(true);

    tray_bubble_view_->GetWidget()->OnKeyEvent(event);

    if (event->handled())
      return;
  }

  // Always consumes key event not to pass it to other widgets. Calling
  // StopPropagation here to make this consistent with
  // MenuController::OnWillDispatchKeyEvent.
  event->StopPropagation();

  // To provide consistent behavior with a menu, process accelerator as a menu
  // is open if the event is not handled by the widget.
  ui::Accelerator accelerator(*event);

  // crbug/1212857 Immediately close the bubble if the accelerator action
  // is going to do it and do not process the accelerator. If the accelerator
  // action is executed asynchronously it will execute after the bubble has
  // already been closed and it will result in the accelerator action reopening
  // the bubble.
  if (tray_bubble_view_->GetAcceleratorAction().has_value() &&
      AcceleratorControllerImpl::Get()->DoesAcceleratorMatchAction(
          ui::Accelerator(*event),
          tray_bubble_view_->GetAcceleratorAction().value())) {
    tray_bubble_view_->CloseBubbleView();
  } else {
    ViewsDelegate::GetInstance()->ProcessAcceleratorWhileMenuShowing(
        accelerator);
  }
}

TrayBubbleView::TrayBubbleView(const InitParams& init_params)
    : BubbleDialogDelegateView(init_params.anchor_view,
                               GetArrowAlignment(init_params.shelf_alignment)),
      params_(init_params),
      layout_(nullptr),
      delegate_(init_params.delegate),
      preferred_width_(init_params.preferred_width),
      bubble_border_(new BubbleBorder(
          arrow(),
          BubbleBorder::NO_SHADOW,
          init_params.bg_color.value_or(gfx::kPlaceholderColor))),
      owned_bubble_border_(bubble_border_),
      is_gesture_dragging_(false),
      mouse_actively_entered_(false) {
  // We set the dialog role because views::BubbleDialogDelegate defaults this to
  // an alert dialog. This would make screen readers announce the whole of the
  // system tray which is undesirable.
  SetAccessibleRole(ax::mojom::Role::kDialog);
  // Bubbles that use transparent colors should not paint their ClientViews to a
  // layer as doing so could result in visual artifacts.
  SetPaintClientToLayer(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  DCHECK(delegate_);
  DCHECK(params_.parent_window);
  // anchor_widget() is computed by BubbleDialogDelegateView().
  DCHECK((init_params.anchor_mode != TrayBubbleView::AnchorMode::kView) ||
         anchor_widget());
  bubble_border_->set_use_theme_background_color(!init_params.bg_color);
  if (init_params.corner_radius)
    bubble_border_->SetCornerRadius(init_params.corner_radius.value());
  bubble_border_->set_avoid_shadow_overlap(true);
  set_parent_window(params_.parent_window);
  SetCanActivate(
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled());
  SetNotifyEnterExitOnChild(true);
  set_close_on_deactivate(init_params.close_on_deactivate);
  set_margins(init_params.margin.has_value() ? init_params.margin.value()
                                             : gfx::Insets());

  if (init_params.translucent) {
    // The following code will not work with bubble's shadow.
    DCHECK(!init_params.has_shadow);
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);

    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kBubbleCornerRadius});
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetIsFastRoundedCorner(true);
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  } else {
    // Create a layer so that the layer for FocusRing stays in this view's
    // layer. Without it, the layer for FocusRing goes above the
    // NativeViewHost and may steal events.
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  }

  auto layout = std::make_unique<BottomAlignedBoxLayout>(this);
  layout->SetDefaultFlex(1);
  layout_ = SetLayoutManager(std::move(layout));

  if (init_params.anchor_mode == AnchorMode::kRect) {
    SetAnchorView(nullptr);
    SetAnchorRect(init_params.anchor_rect);
    if (init_params.insets.has_value())
      bubble_border_->set_insets(init_params.insets.value());
  }
}

TrayBubbleView::~TrayBubbleView() {
  mouse_watcher_.reset();

  if (delegate_) {
    // Inform host items (models) that their views are being destroyed.
    delegate_->BubbleViewDestroyed();
  }
}

void TrayBubbleView::InitializeAndShowBubble() {
  GetWidget()->Show();
  UpdateBubble();

  if (IsAnchoredToStatusArea()) {
    tray_bubble_counter_.emplace(
        StatusAreaWidget::ForWindow(GetWidget()->GetNativeView()));
  }

  // Register pre target event handler to reroute key
  // events to the widget for activating the view or closing it.
  if (!CanActivate() && params_.reroute_event_handler)
    reroute_event_handler_ = std::make_unique<RerouteEventHandler>(this);
}

void TrayBubbleView::UpdateBubble() {
  if (GetWidget()) {
    SizeToContents();
    GetWidget()->GetRootView()->SchedulePaint();
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

void TrayBubbleView::SetPreferredWidth(int width) {
  if (preferred_width_ == width)
    return;
  preferred_width_ = width;
  if (GetWidget())
    SizeToContents();
}

gfx::Insets TrayBubbleView::GetBorderInsets() const {
  return bubble_border_ ? bubble_border_->GetInsets() : gfx::Insets();
}

absl::optional<AcceleratorAction> TrayBubbleView::GetAcceleratorAction() const {
  return delegate_->GetAcceleratorAction();
}

void TrayBubbleView::ResetDelegate() {
  reroute_event_handler_.reset();

  delegate_ = nullptr;
}

void TrayBubbleView::ChangeAnchorView(views::View* anchor_view) {
  DCHECK_EQ(AnchorMode::kView, params_.anchor_mode);
  BubbleDialogDelegateView::SetAnchorView(anchor_view);
}

void TrayBubbleView::ChangeAnchorRect(const gfx::Rect& rect) {
  DCHECK_EQ(AnchorMode::kRect, params_.anchor_mode);
  BubbleDialogDelegateView::SetAnchorRect(rect);
}

void TrayBubbleView::ChangeAnchorAlignment(ShelfAlignment alignment) {
  SetArrow(GetArrowAlignment(alignment));
}

bool TrayBubbleView::IsAnchoredToStatusArea() const {
  return params_.is_anchored_to_status_area;
}

void TrayBubbleView::StopReroutingEvents() {
  reroute_event_handler_.reset();
}

void TrayBubbleView::OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                              Widget* bubble_widget) const {
  if (params_.has_shadow) {
    // Apply a WM-provided shadow (see ui/wm/core/).
    params->shadow_type = Widget::InitParams::ShadowType::kDrop;
    params->shadow_elevation = wm::kShadowElevationActiveWindow;
  }
}

void TrayBubbleView::OnWidgetClosing(Widget* widget) {
  // We no longer need to watch key events for activation if the widget is
  // closing.
  reroute_event_handler_.reset();

  BubbleDialogDelegateView::OnWidgetClosing(widget);

  if (IsAnchoredToStatusArea())
    tray_bubble_counter_.reset();
}

void TrayBubbleView::OnWidgetActivationChanged(Widget* widget, bool active) {
  // We no longer need to watch key events for activation if the widget is
  // activated.
  reroute_event_handler_.reset();

  BubbleDialogDelegateView::OnWidgetActivationChanged(widget, active);
}

ui::LayerType TrayBubbleView::GetLayerType() const {
  if (params_.translucent)
    return ui::LAYER_NOT_DRAWN;
  return ui::LAYER_TEXTURED;
}

std::unique_ptr<NonClientFrameView> TrayBubbleView::CreateNonClientFrameView(
    Widget* widget) {
  auto frame = BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(owned_bubble_border_));
  return frame;
}

bool TrayBubbleView::WidgetHasHitTestMask() const {
  return true;
}

void TrayBubbleView::GetWidgetHitTestMask(SkPath* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(GetBubbleFrameView()->GetContentsBounds()));
}

std::u16string TrayBubbleView::GetAccessibleWindowTitle() const {
  if (delegate_)
    return delegate_->GetAccessibleNameForBubble();
  else
    return std::u16string();
}

gfx::Size TrayBubbleView::CalculatePreferredSize() const {
  return gfx::Size(preferred_width_, GetHeightForWidth(preferred_width_));
}

int TrayBubbleView::GetHeightForWidth(int width) const {
  width = std::max(width - GetInsets().width(), 0);
  const auto visible_height = [width](int height, const views::View* child) {
    return height + (child->GetVisible() ? child->GetHeightForWidth(width) : 0);
  };
  const int height = std::accumulate(children().cbegin(), children().cend(),
                                     GetInsets().height(), visible_height);
  return (params_.max_height != 0) ? std::min(height, params_.max_height)
                                   : height;
}

void TrayBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  mouse_watcher_.reset();
  if (delegate_ && !(event.flags() & ui::EF_IS_SYNTHESIZED)) {
    // The user actively moved the mouse over the bubble; inform the delegate.
    delegate_->OnMouseEnteredView();
    mouse_actively_entered_ = true;
  } else {
    // The mouse was located over the bubble when it was first shown; use
    // MouseWatcher to wait for user interaction before signaling the delegate.
    mouse_watcher_ = std::make_unique<views::MouseWatcher>(
        std::make_unique<MouseMoveDetectorHost>(), this);
    mouse_watcher_->set_notify_on_exit_time(base::TimeDelta());
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }
}

void TrayBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  // Disable any MouseWatcher waiting for user interaction inside the bubble.
  mouse_watcher_.reset();
  // Only notify the delegate on exit if it was notified on enter.
  if (delegate_ && mouse_actively_entered_)
    delegate_->OnMouseExitedView();
}

void TrayBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (delegate_ && CanActivate()) {
    node_data->role = ax::mojom::Role::kWindow;
    node_data->SetName(delegate_->GetAccessibleNameForBubble());
  }
}

void TrayBubbleView::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  DCHECK(layer());
  if (layer()->type() != ui::LAYER_SOLID_COLOR)
    return;
  layer()->SetColor(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));
}

void TrayBubbleView::MouseMovedOutOfHost() {
  // The user moved the mouse that was over the bubble when it was first shown.
  if (delegate_)
    delegate_->OnMouseEnteredView();
  mouse_actively_entered_ = true;
  mouse_watcher_.reset();
}

void TrayBubbleView::ChildPreferredSizeChanged(View* child) {
  SizeToContents();
}

void TrayBubbleView::SetBubbleBorderInsets(gfx::Insets insets) {
  if (bubble_border_)
    bubble_border_->set_insets(insets);
}

void TrayBubbleView::CloseBubbleView() {
  if (!delegate_)
    return;

  delegate_->HideBubble(this);
}

BEGIN_METADATA(TrayBubbleView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
