// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/bridged_native_widget_host_impl.h"

#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/models/dialog_model.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/views/cocoa/bridged_native_widget.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/word_lookup_client.h"

using views_bridge_mac::mojom::BridgedNativeWidgetInitParams;
using views_bridge_mac::mojom::WindowVisibilityState;

namespace views {

namespace {

// Returns true if bounds passed to window in SetBounds should be treated as
// though they are in screen coordinates.
bool PositionWindowInScreenCoordinates(Widget* widget,
                                       Widget::InitParams::Type type) {
  // Replicate the logic in desktop_aura/desktop_screen_position_client.cc.
  if (GetAuraWindowTypeForWidgetType(type) == aura::client::WINDOW_TYPE_POPUP)
    return true;

  return widget && widget->is_top_level();
}

}  // namespace

BridgedNativeWidgetHostImpl::BridgedNativeWidgetHostImpl(
    NativeWidgetMac* parent)
    : native_widget_mac_(parent),
      bridge_impl_(new BridgedNativeWidget(this, parent)) {}

BridgedNativeWidgetHostImpl::~BridgedNativeWidgetHostImpl() {
  // Destroy the bridge first to prevent any calls back into this during
  // destruction.
  // TODO(ccameron): When all communication from |bridge_| to this goes through
  // the BridgedNativeWidgetHost, this can be replaced with closing that pipe.
  bridge_impl_.reset();
  SetFocusManager(nullptr);
  DestroyCompositor();
}

views_bridge_mac::mojom::BridgedNativeWidget*
BridgedNativeWidgetHostImpl::bridge() const {
  return bridge_impl_.get();
}

void BridgedNativeWidgetHostImpl::InitWindow(const Widget::InitParams& params) {
  // Tooltip Widgets shouldn't have their own tooltip manager, but tooltips are
  // native on Mac, so nothing should ever want one in Widget form.
  DCHECK_NE(params.type, Widget::InitParams::TYPE_TOOLTIP);
  widget_type_ = params.type;

  bridge_impl_->SetParent(params.parent);

  // Initialize the window.
  {
    auto bridge_params = BridgedNativeWidgetInitParams::New();
    bridge_params->modal_type =
        native_widget_mac_->GetWidget()->widget_delegate()->GetModalType();
    bridge_params->is_translucent =
        params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW;

    // OSX likes to put shadows on most things. However, frameless windows (with
    // styleMask = NSBorderlessWindowMask) default to no shadow. So change that.
    // SHADOW_TYPE_DROP is used for Menus, which get the same shadow style on
    // Mac.
    switch (params.shadow_type) {
      case Widget::InitParams::SHADOW_TYPE_NONE:
        bridge_params->has_window_server_shadow = false;
        break;
      case Widget::InitParams::SHADOW_TYPE_DEFAULT:
        // Controls should get views shadows instead of native shadows.
        bridge_params->has_window_server_shadow =
            params.type != Widget::InitParams::TYPE_CONTROL;
        break;
      case Widget::InitParams::SHADOW_TYPE_DROP:
        bridge_params->has_window_server_shadow = true;
        break;
    }  // No default case, to pick up new types.

    // Include "regular" windows without the standard frame in the window cycle.
    // These use NSBorderlessWindowMask so do not get it by default.
    bridge_params->force_into_collection_cycle =
        widget_type_ == Widget::InitParams::TYPE_WINDOW &&
        params.remove_standard_frame;

    bridge()->InitWindow(std::move(bridge_params));
  }

  // Set a meaningful initial bounds. Note that except for frameless widgets
  // with no WidgetDelegate, the bounds will be set again by Widget after
  // initializing the non-client view. In the former case, if bounds were not
  // set at all, the creator of the Widget is expected to call SetBounds()
  // before calling Widget::Show() to avoid a kWindowSizeDeterminedLater-sized
  // (i.e. 1x1) window appearing.
  bridge()->SetInitialBounds(params.bounds,
                             native_widget_mac_->GetWidget()->GetMinimumSize(),
                             GetBoundsOffsetForParent());

  // Widgets for UI controls (usually layered above web contents) start visible.
  if (widget_type_ == Widget::InitParams::TYPE_CONTROL)
    bridge()->SetVisibilityState(WindowVisibilityState::kShowInactive);
}

void BridgedNativeWidgetHostImpl::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect adjusted_bounds = bounds;
  adjusted_bounds.Offset(GetBoundsOffsetForParent());
  bridge()->SetBounds(adjusted_bounds,
                      native_widget_mac_->GetWidget()->GetMinimumSize());
}

gfx::Vector2d BridgedNativeWidgetHostImpl::GetBoundsOffsetForParent() const {
  gfx::Vector2d offset;
  Widget* widget = native_widget_mac_->GetWidget();
  BridgedNativeWidgetOwner* parent = bridge_impl_->parent();
  if (parent && !PositionWindowInScreenCoordinates(widget, widget_type_))
    offset = parent->GetChildWindowOffset();
  return offset;
}

void BridgedNativeWidgetHostImpl::SetFullscreen(bool fullscreen) {
  // Note that when the NSWindow begins a fullscreen transition, the value of
  // |target_fullscreen_state_| updates via OnWindowFullscreenTransitionStart.
  // The update here is necessary for the case where we are currently in
  // transition (and therefore OnWindowFullscreenTransitionStart will not be
  // called until the current transition completes).
  target_fullscreen_state_ = fullscreen;
  bridge()->SetFullscreen(target_fullscreen_state_);
}

void BridgedNativeWidgetHostImpl::SetRootView(views::View* root_view) {
  root_view_ = root_view;
}

void BridgedNativeWidgetHostImpl::CreateCompositor(
    const Widget::InitParams& params) {
  DCHECK(!compositor_);
  DCHECK(!layer());
  DCHECK(ViewsDelegate::GetInstance());

  // "Infer" must be handled by ViewsDelegate::OnBeforeWidgetInit().
  DCHECK_NE(Widget::InitParams::INFER_OPACITY, params.opacity);
  bool translucent = params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW;

  // Create the layer.
  SetLayer(std::make_unique<ui::Layer>(params.layer_type));
  layer()->set_delegate(this);
  layer()->SetFillsBoundsOpaquely(!translucent);

  // Create the compositor and attach the layer to it.
  ui::ContextFactory* context_factory =
      ViewsDelegate::GetInstance()->GetContextFactory();
  DCHECK(context_factory);
  ui::ContextFactoryPrivate* context_factory_private =
      ViewsDelegate::GetInstance()->GetContextFactoryPrivate();
  compositor_ = ui::RecyclableCompositorMacFactory::Get()->CreateCompositor(
      context_factory, context_factory_private);
  compositor_->widget()->SetNSView(this);
  compositor_->compositor()->SetBackgroundColor(
      translucent ? SK_ColorTRANSPARENT : SK_ColorWHITE);
  compositor_->compositor()->SetRootLayer(layer());

  // The compositor is locked (prevented from producing frames) until the widget
  // is made visible.
  UpdateCompositorProperties();
  layer()->SetVisible(is_visible_);
  if (is_visible_)
    compositor_->Unsuspend();

  bridge()->InitCompositorView();
}

void BridgedNativeWidgetHostImpl::UpdateCompositorProperties() {
  if (!compositor_)
    return;
  gfx::Size surface_size_in_dip = content_bounds_in_screen_.size();
  layer()->SetBounds(gfx::Rect(surface_size_in_dip));
  compositor_->UpdateSurface(
      ConvertSizeToPixel(display_.device_scale_factor(), surface_size_in_dip),
      display_.device_scale_factor());
}

void BridgedNativeWidgetHostImpl::DestroyCompositor() {
  if (layer()) {
    // LayerOwner supports a change in ownership, e.g., to animate a closing
    // window, but that won't work as expected for the root layer in
    // BridgedNativeWidget.
    DCHECK_EQ(this, layer()->owner());
    layer()->CompleteAllAnimations();
    layer()->SuppressPaint();
    layer()->set_delegate(nullptr);
  }
  DestroyLayer();
  if (!compositor_)
    return;
  compositor_->widget()->ResetNSView();
  compositor_->compositor()->SetRootLayer(nullptr);
  ui::RecyclableCompositorMacFactory::Get()->RecycleCompositor(
      std::move(compositor_));
}

void BridgedNativeWidgetHostImpl::SetFocusManager(FocusManager* focus_manager) {
  if (focus_manager_ == focus_manager)
    return;

  if (focus_manager_) {
    // Only the destructor can replace the focus manager (and it passes null).
    DCHECK(!focus_manager);
    if (View* old_focus = focus_manager_->GetFocusedView())
      OnDidChangeFocus(old_focus, nullptr);
    focus_manager_->RemoveFocusChangeListener(this);
    focus_manager_ = nullptr;
    return;
  }

  focus_manager_ = focus_manager;
  focus_manager_->AddFocusChangeListener(this);
  if (View* new_focus = focus_manager_->GetFocusedView())
    OnDidChangeFocus(nullptr, new_focus);
}

bool BridgedNativeWidgetHostImpl::SetWindowTitle(const base::string16& title) {
  if (window_title_ == title)
    return false;
  window_title_ = title;
  bridge()->SetWindowTitle(window_title_);
  return true;
}

void BridgedNativeWidgetHostImpl::OnWidgetInitDone() {
  Widget* widget = native_widget_mac_->GetWidget();
  if (DialogDelegate* dialog = widget->widget_delegate()->AsDialogDelegate())
    dialog->AddObserver(this);
}

ui::InputMethod* BridgedNativeWidgetHostImpl::GetInputMethod() {
  if (!input_method_) {
    input_method_ = ui::CreateInputMethod(this, gfx::kNullAcceleratedWidget);
    // For now, use always-focused mode on Mac for the input method.
    // TODO(tapted): Move this to OnWindowKeyStatusChangedTo() and balance.
    input_method_->OnFocus();
  }
  return input_method_.get();
}

gfx::Rect BridgedNativeWidgetHostImpl::GetRestoredBounds() const {
  if (target_fullscreen_state_ || in_fullscreen_transition_)
    return window_bounds_before_fullscreen_;
  return window_bounds_in_screen_;
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, views::BridgedNativeWidgetHost:

NSView* BridgedNativeWidgetHostImpl::GetNativeViewAccessible() {
  return root_view_ ? root_view_->GetNativeViewAccessible() : nil;
}

void BridgedNativeWidgetHostImpl::OnVisibilityChanged(bool window_visible) {
  is_visible_ = window_visible;
  if (compositor_) {
    layer()->SetVisible(window_visible);
    if (window_visible) {
      compositor_->Unsuspend();
      layer()->SchedulePaint(layer()->bounds());
    } else {
      compositor_->Suspend();
    }
  }
  native_widget_mac_->GetWidget()->OnNativeWidgetVisibilityChanged(
      window_visible);
}

void BridgedNativeWidgetHostImpl::OnScrollEvent(
    const ui::ScrollEvent& const_event) {
  ui::ScrollEvent event = const_event;
  root_view_->GetWidget()->OnScrollEvent(&event);
}

void BridgedNativeWidgetHostImpl::OnMouseEvent(
    const ui::MouseEvent& const_event) {
  ui::MouseEvent event = const_event;
  root_view_->GetWidget()->OnMouseEvent(&event);
}

void BridgedNativeWidgetHostImpl::OnGestureEvent(
    const ui::GestureEvent& const_event) {
  ui::GestureEvent event = const_event;
  root_view_->GetWidget()->OnGestureEvent(&event);
}

void BridgedNativeWidgetHostImpl::DispatchKeyEvent(
    const ui::KeyEvent& const_event,
    bool* event_handled) {
  ui::KeyEvent event = const_event;
  ignore_result(
      root_view_->GetWidget()->GetInputMethod()->DispatchKeyEvent(&event));
  *event_handled = event.handled();
}

void BridgedNativeWidgetHostImpl::DispatchKeyEventToMenuController(
    const ui::KeyEvent& const_event,
    bool* event_swallowed,
    bool* event_handled) {
  ui::KeyEvent event = const_event;
  MenuController* menu_controller = MenuController::GetActiveInstance();
  if (menu_controller && root_view_ &&
      menu_controller->owner() == root_view_->GetWidget()) {
    *event_swallowed = menu_controller->OnWillDispatchKeyEvent(&event) ==
                       ui::POST_DISPATCH_NONE;
  } else {
    *event_swallowed = false;
  }
  *event_handled = event.handled();
}

void BridgedNativeWidgetHostImpl::GetHasMenuController(
    bool* has_menu_controller) {
  MenuController* menu_controller = MenuController::GetActiveInstance();
  *has_menu_controller = menu_controller && root_view_ &&
                         menu_controller->owner() == root_view_->GetWidget();
}

void BridgedNativeWidgetHostImpl::SetViewSize(const gfx::Size& new_size) {
  root_view_->SetSize(new_size);
}

void BridgedNativeWidgetHostImpl::SetKeyboardAccessible(bool enabled) {
  views::FocusManager* focus_manager =
      root_view_->GetWidget()->GetFocusManager();
  if (focus_manager)
    focus_manager->SetKeyboardAccessible(enabled);
}

void BridgedNativeWidgetHostImpl::SetIsFirstResponder(bool is_first_responder) {
  if (is_first_responder)
    root_view_->GetWidget()->GetFocusManager()->RestoreFocusedView();
  else
    root_view_->GetWidget()->GetFocusManager()->StoreFocusedView(true);
}

void BridgedNativeWidgetHostImpl::OnMouseCaptureActiveChanged(bool is_active) {
  DCHECK_NE(is_mouse_capture_active_, is_active);
  is_mouse_capture_active_ = is_active;
  if (!is_mouse_capture_active_)
    native_widget_mac_->GetWidget()->OnMouseCaptureLost();
}

void BridgedNativeWidgetHostImpl::GetIsDraggableBackgroundAt(
    const gfx::Point& location_in_content,
    bool* is_draggable_background) {
  int component =
      root_view_->GetWidget()->GetNonClientComponent(location_in_content);
  *is_draggable_background = component == HTCAPTION;
}

void BridgedNativeWidgetHostImpl::GetTooltipTextAt(
    const gfx::Point& location_in_content,
    base::string16* new_tooltip_text) {
  views::View* view =
      root_view_->GetTooltipHandlerForPoint(location_in_content);
  if (view) {
    gfx::Point view_point = location_in_content;
    views::View::ConvertPointToScreen(root_view_, &view_point);
    views::View::ConvertPointFromScreen(view, &view_point);
    if (!view->GetTooltipText(view_point, new_tooltip_text))
      DCHECK(new_tooltip_text->empty());
  }
}

void BridgedNativeWidgetHostImpl::GetWordAt(
    const gfx::Point& location_in_content,
    bool* found_word,
    gfx::DecoratedText* decorated_word,
    gfx::Point* baseline_point) {
  *found_word = false;

  views::View* target =
      root_view_->GetEventHandlerForPoint(location_in_content);
  if (!target)
    return;

  views::WordLookupClient* word_lookup_client = target->GetWordLookupClient();
  if (!word_lookup_client)
    return;

  gfx::Point location_in_target = location_in_content;
  views::View::ConvertPointToTarget(root_view_, target, &location_in_target);
  if (!word_lookup_client->GetWordLookupDataAtPoint(
          location_in_target, decorated_word, baseline_point)) {
    return;
  }

  // Convert |baselinePoint| to the coordinate system of |root_view_|.
  views::View::ConvertPointToTarget(target, root_view_, baseline_point);
  *found_word = true;
}

void BridgedNativeWidgetHostImpl::GetWidgetIsModal(bool* widget_is_modal) {
  *widget_is_modal = native_widget_mac_->GetWidget()->IsModal();
}

void BridgedNativeWidgetHostImpl::GetIsFocusedViewTextual(bool* is_textual) {
  views::FocusManager* focus_manager =
      root_view_ ? root_view_->GetWidget()->GetFocusManager() : nullptr;
  *is_textual = focus_manager && focus_manager->GetFocusedView() &&
                focus_manager->GetFocusedView()->GetClassName() ==
                    views::Label::kViewClassName;
}

void BridgedNativeWidgetHostImpl::OnWindowGeometryChanged(
    const gfx::Rect& new_window_bounds_in_screen,
    const gfx::Rect& new_content_bounds_in_screen) {
  has_received_window_geometry_ = true;

  bool window_has_moved =
      new_window_bounds_in_screen.origin() != window_bounds_in_screen_.origin();
  bool content_has_resized =
      new_content_bounds_in_screen.size() != content_bounds_in_screen_.size();

  window_bounds_in_screen_ = new_window_bounds_in_screen;
  content_bounds_in_screen_ = new_content_bounds_in_screen;

  // When a window grows vertically, the AppKit origin changes, but as far as
  // tookit-views is concerned, the window hasn't moved. Suppress these.
  if (window_has_moved)
    native_widget_mac_->GetWidget()->OnNativeWidgetMove();

  // Note we can't use new_window_bounds_in_screen.size(), since it includes the
  // titlebar for the purposes of detecting a window move.
  if (content_has_resized)
    native_widget_mac_->GetWidget()->OnNativeWidgetSizeChanged(
        content_bounds_in_screen_.size());

  // Update the compositor surface and layer size.
  UpdateCompositorProperties();
}

void BridgedNativeWidgetHostImpl::OnWindowFullscreenTransitionStart(
    bool target_fullscreen_state) {
  target_fullscreen_state_ = target_fullscreen_state;
  in_fullscreen_transition_ = true;

  // If going into fullscreen, store an answer for GetRestoredBounds().
  if (target_fullscreen_state)
    window_bounds_before_fullscreen_ = window_bounds_in_screen_;

  // Notify that fullscreen state changed.
  native_widget_mac_->OnWindowFullscreenStateChange();
}

void BridgedNativeWidgetHostImpl::OnWindowFullscreenTransitionComplete(
    bool actual_fullscreen_state) {
  in_fullscreen_transition_ = false;

  // Ensure constraints are re-applied when completing a transition.
  native_widget_mac_->OnSizeConstraintsChanged();
}

void BridgedNativeWidgetHostImpl::OnWindowMiniaturizedChanged(
    bool miniaturized) {
  is_miniaturized_ = miniaturized;
}

void BridgedNativeWidgetHostImpl::OnWindowDisplayChanged(
    const display::Display& new_display) {
  bool scale_factor_changed =
      display_.device_scale_factor() != new_display.device_scale_factor();
  display_ = new_display;
  if (scale_factor_changed && compositor_ && has_received_window_geometry_) {
    compositor_->UpdateSurface(
        ConvertSizeToPixel(display_.device_scale_factor(),
                           content_bounds_in_screen_.size()),
        display_.device_scale_factor());
  }
}

void BridgedNativeWidgetHostImpl::OnWindowWillClose() {
  Widget* widget = native_widget_mac_->GetWidget();
  if (DialogDelegate* dialog = widget->widget_delegate()->AsDialogDelegate())
    dialog->RemoveObserver(this);
  native_widget_mac_->WindowDestroying();
}

void BridgedNativeWidgetHostImpl::OnWindowHasClosed() {
  native_widget_mac_->WindowDestroyed();
}

void BridgedNativeWidgetHostImpl::OnWindowKeyStatusChanged(
    bool is_key,
    bool is_content_first_responder,
    bool full_keyboard_access_enabled) {
  is_window_key_ = is_key;
  Widget* widget = native_widget_mac_->GetWidget();
  if (!widget->OnNativeWidgetActivationChanged(is_key))
    return;
  // The contentView is the BridgedContentView hosting the views::RootView. The
  // focus manager will already know if a native subview has focus.
  if (is_content_first_responder) {
    if (is_key) {
      widget->OnNativeFocus();
      // Explicitly set the keyboard accessibility state on regaining key
      // window status.
      SetKeyboardAccessible(full_keyboard_access_enabled);
      widget->GetFocusManager()->RestoreFocusedView();
    } else {
      widget->OnNativeBlur();
      widget->GetFocusManager()->StoreFocusedView(true);
    }
  }
}

void BridgedNativeWidgetHostImpl::DoDialogButtonAction(
    ui::DialogButton button) {
  views::DialogDelegate* dialog =
      root_view_->GetWidget()->widget_delegate()->AsDialogDelegate();
  DCHECK(dialog);
  views::DialogClientView* client = dialog->GetDialogClientView();
  if (button == ui::DIALOG_BUTTON_OK) {
    client->AcceptWindow();
  } else {
    DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
    client->CancelWindow();
  }
}

void BridgedNativeWidgetHostImpl::GetDialogButtonInfo(
    ui::DialogButton button,
    bool* button_exists,
    base::string16* button_label,
    bool* is_button_enabled,
    bool* is_button_default) {
  *button_exists = false;
  ui::DialogModel* model =
      root_view_->GetWidget()->widget_delegate()->AsDialogDelegate();
  if (!model || !(model->GetDialogButtons() & button))
    return;
  *button_exists = true;
  *button_label = model->GetDialogButtonLabel(button);
  *is_button_enabled = model->IsDialogButtonEnabled(button);
  *is_button_default = button == model->GetDefaultDialogButton();
}

void BridgedNativeWidgetHostImpl::GetDoDialogButtonsExist(bool* buttons_exist) {
  ui::DialogModel* model =
      root_view_->GetWidget()->widget_delegate()->AsDialogDelegate();
  *buttons_exist = model && model->GetDialogButtons();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, DialogObserver:

void BridgedNativeWidgetHostImpl::OnDialogModelChanged() {
  // Note it's only necessary to clear the TouchBar. If the OS needs it again,
  // a new one will be created.
  bridge()->ClearTouchBar();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, FocusChangeListener:

void BridgedNativeWidgetHostImpl::OnWillChangeFocus(View* focused_before,
                                                    View* focused_now) {}

void BridgedNativeWidgetHostImpl::OnDidChangeFocus(View* focused_before,
                                                   View* focused_now) {
  ui::InputMethod* input_method =
      native_widget_mac_->GetWidget()->GetInputMethod();
  if (input_method) {
    ui::TextInputClient* input_client = input_method->GetTextInputClient();
    // Sanity check: When focus moves away from the widget (i.e. |focused_now|
    // is nil), then the textInputClient will be cleared.
    DCHECK(!!focused_now || !input_client);
    bridge_impl_->SetTextInputClient(input_client);
  }
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, internal::InputMethodDelegate:

ui::EventDispatchDetails BridgedNativeWidgetHostImpl::DispatchKeyEventPostIME(
    ui::KeyEvent* key) {
  DCHECK(focus_manager_);
  if (!focus_manager_->OnKeyEvent(*key))
    key->StopPropagation();
  else
    native_widget_mac_->GetWidget()->OnKeyEvent(key);
  return ui::EventDispatchDetails();
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, LayerDelegate:

void BridgedNativeWidgetHostImpl::OnPaintLayer(
    const ui::PaintContext& context) {
  native_widget_mac_->GetWidget()->OnNativeWidgetPaint(context);
}

void BridgedNativeWidgetHostImpl::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  native_widget_mac_->GetWidget()->DeviceScaleFactorChanged(
      old_device_scale_factor, new_device_scale_factor);
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidgetHostImpl, AcceleratedWidgetMac:

void BridgedNativeWidgetHostImpl::AcceleratedWidgetCALayerParamsUpdated() {
  const gfx::CALayerParams* ca_layer_params =
      compositor_->widget()->GetCALayerParams();
  if (ca_layer_params)
    bridge()->SetCALayerParams(*ca_layer_params);
}

}  // namespace views
