// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_
#define UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/cocoa/bridged_native_widget_host.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_observer.h"

namespace ui {
class RecyclableCompositorMac;
}  // namespace ui

namespace views {

class BridgedNativeWidget;
class BridgedNativeWidgetPublic;
class NativeWidgetMac;

// The portion of NativeWidgetMac that lives in the browser process. This
// communicates to the BridgedNativeWidget, which interacts with the Cocoa
// APIs, and which may live in an app shim process.
class VIEWS_EXPORT BridgedNativeWidgetHostImpl
    : public BridgedNativeWidgetHost,
      public DialogObserver,
      public FocusChangeListener,
      public ui::internal::InputMethodDelegate,
      public ui::LayerDelegate,
      public ui::LayerOwner,
      public ui::AcceleratedWidgetMacNSView {
 public:
  // Creates one side of the bridge. |parent| must not be NULL.
  explicit BridgedNativeWidgetHostImpl(NativeWidgetMac* parent);
  ~BridgedNativeWidgetHostImpl() override;

  // Provide direct access to the BridgedNativeWidget that this is hosting.
  // TODO(ccameron): Remove all accesses to this member, and replace them
  // with methods that may be sent across processes.
  BridgedNativeWidget* bridge_impl() const { return bridge_impl_.get(); }
  BridgedNativeWidgetPublic* bridge() const;

  void InitWindow(const Widget::InitParams& params);

  // Changes the bounds of the window and the hosted layer if present. The
  // origin is a location in screen coordinates except for "child" windows,
  // which are positioned relative to their parent. SetBounds() considers a
  // "child" window to be one initialized with InitParams specifying all of:
  // a |parent| NSWindow, the |child| attribute, and a |type| that
  // views::GetAuraWindowTypeForWidgetType does not consider a "popup" type.
  void SetBounds(const gfx::Rect& bounds);

  // Tell the window to transition to being fullscreen or not-fullscreen.
  void SetFullscreen(bool fullscreen);

  // The ultimate fullscreen state that is being targeted (irrespective of any
  // active transitions).
  bool target_fullscreen_state() const { return target_fullscreen_state_; }

  // Set the root view (set during initialization and un-set during teardown).
  void SetRootView(views::View* root_view);

  // Initialize the ui::Compositor and ui::Layer.
  void CreateCompositor(const Widget::InitParams& params);

  // Sets or clears the focus manager to use for tracking focused views.
  // This does NOT take ownership of |focus_manager|.
  void SetFocusManager(FocusManager* focus_manager);

  // Called when the owning Widget's Init method has completed.
  void OnWidgetInitDone();

  // See widget.h for documentation.
  ui::InputMethod* GetInputMethod();

  // Geometry of the window, in DIPs.
  const gfx::Rect& GetWindowBoundsInScreen() const {
    DCHECK(has_received_window_geometry_);
    return window_bounds_in_screen_;
  }

  // Geometry of the content area of the window, in DIPs. Note that this is not
  // necessarily the same as the views::View's size.
  const gfx::Rect& GetContentBoundsInScreen() const {
    DCHECK(has_received_window_geometry_);
    return content_bounds_in_screen_;
  }

  // The display that the window is currently on (or best guess thereof).
  const display::Display& GetCurrentDisplay() const { return display_; }

  // The restored bounds will be derived from the current NSWindow frame unless
  // fullscreen or transitioning between fullscreen states.
  gfx::Rect GetRestoredBounds() const;

 private:
  gfx::Vector2d GetBoundsOffsetForParent() const;
  void DestroyCompositor();

  // views::BridgedNativeWidgetHost:
  void SetCompositorVisibility(bool visible) override;
  void SetViewSize(const gfx::Size& new_size) override;
  void SetKeyboardAccessible(bool enabled) override;
  void SetIsFirstResponder(bool is_first_responder) override;
  void OnScrollEvent(const ui::ScrollEvent& const_event) override;
  void OnMouseEvent(const ui::MouseEvent& const_event) override;
  void OnGestureEvent(const ui::GestureEvent& const_event) override;
  void GetIsDraggableBackgroundAt(const gfx::Point& location_in_content,
                                  bool* is_draggable_background) override;
  void GetTooltipTextAt(const gfx::Point& location_in_content,
                        base::string16* new_tooltip_text) override;
  void GetWordAt(const gfx::Point& location_in_content,
                 bool* found_word,
                 gfx::DecoratedText* decorated_word,
                 gfx::Point* baseline_point) override;
  void OnWindowGeometryChanged(
      const gfx::Rect& window_bounds_in_screen_dips,
      const gfx::Rect& content_bounds_in_screen_dips) override;
  void OnWindowFullscreenTransitionStart(bool target_fullscreen_state) override;
  void OnWindowFullscreenTransitionComplete(
      bool target_fullscreen_state) override;
  void OnWindowDisplayChanged(const display::Display& display) override;
  void OnWindowWillClose() override;
  void OnWindowHasClosed() override;

  // DialogObserver:
  void OnDialogModelChanged() override;

  // FocusChangeListener:
  void OnWillChangeFocus(View* focused_before, View* focused_now) override;
  void OnDidChangeFocus(View* focused_before, View* focused_now) override;

  // ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(ui::KeyEvent* key) override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // ui::AcceleratedWidgetMacNSView:
  void AcceleratedWidgetCALayerParamsUpdated() override;

  views::NativeWidgetMac* const native_widget_mac_;  // Weak. Owns |this_|.

  Widget::InitParams::Type widget_type_ = Widget::InitParams::TYPE_WINDOW;

  views::View* root_view_ = nullptr;  // Weak. Owned by |native_widget_mac_|.

  // TODO(ccameron): Rather than instantiate a BridgedNativeWidget here,
  // we will instantiate a mojo BridgedNativeWidget interface to a Cocoa
  // instance that may be in another process.
  std::unique_ptr<BridgedNativeWidget> bridge_impl_;

  std::unique_ptr<ui::InputMethod> input_method_;
  FocusManager* focus_manager_ = nullptr;  // Weak. Owned by our Widget.

  // The display that the window is currently on.
  display::Display display_;

  // The geometry of the window and its contents view, in screen coordinates.
  bool has_received_window_geometry_ = false;
  gfx::Rect window_bounds_in_screen_;
  gfx::Rect content_bounds_in_screen_;
  bool target_fullscreen_state_ = false;
  bool in_fullscreen_transition_ = false;
  gfx::Rect window_bounds_before_fullscreen_;

  std::unique_ptr<ui::RecyclableCompositorMac> compositor_;

  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetHostImpl);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_HOST_IMPL_H_
