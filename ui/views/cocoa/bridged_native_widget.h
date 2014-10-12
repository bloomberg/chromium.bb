// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_H_
#define UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "ui/views/focus/focus_manager.h"
#include "ui/views/ime/input_method_delegate.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"

@class BridgedContentView;
@class ViewsNSWindowDelegate;

namespace ui {
class InputMethod;
}

namespace views {

class InputMethod;
class NativeWidgetMac;
class View;

// A bridge to an NSWindow managed by an instance of NativeWidgetMac or
// DesktopNativeWidgetMac. Serves as a helper class to bridge requests from the
// NativeWidgetMac to the Cocoa window. Behaves a bit like an aura::Window.
class VIEWS_EXPORT BridgedNativeWidget : public internal::InputMethodDelegate,
                                         public FocusChangeListener {
 public:
  // Creates one side of the bridge. |parent| must not be NULL.
  explicit BridgedNativeWidget(NativeWidgetMac* parent);
  virtual ~BridgedNativeWidget();

  // Initialize the bridge, "retains" ownership of |window|.
  void Init(base::scoped_nsobject<NSWindow> window,
            const Widget::InitParams& params);

  // Sets or clears the focus manager to use for tracking focused views.
  // This does NOT take ownership of |focus_manager|.
  void SetFocusManager(FocusManager* focus_manager);

  // Set or clears the views::View bridged by the content view. This does NOT
  // take ownership of |view|.
  void SetRootView(views::View* view);

  // Called internally by the NSWindowDelegate when the window is closing.
  void OnWindowWillClose();

  // Called by the NSWindowDelegate when a fullscreen operation begins. If
  // |target_fullscreen_state| is true, the target state is fullscreen.
  // Otherwise, a transition has begun to come out of fullscreen.
  void OnFullscreenTransitionStart(bool target_fullscreen_state);

  // Called when a fullscreen transition completes. If target_fullscreen_state()
  // does not match |actual_fullscreen_state|, a new transition will begin.
  void OnFullscreenTransitionComplete(bool actual_fullscreen_state);

  // Transition the window into or out of fullscreen. This will immediately
  // invert the value of target_fullscreen_state().
  void ToggleDesiredFullscreenState();

  // See widget.h for documentation.
  InputMethod* CreateInputMethod();
  ui::InputMethod* GetHostInputMethod();

  // The restored bounds will be derived from the current NSWindow frame unless
  // fullscreen or transitioning between fullscreen states.
  gfx::Rect GetRestoredBounds() const;

  NativeWidgetMac* native_widget_mac() { return native_widget_mac_; }
  BridgedContentView* ns_view() { return bridged_view_; }
  NSWindow* ns_window() { return window_; }

  bool target_fullscreen_state() const { return target_fullscreen_state_; }

  // Overridden from internal::InputMethodDelegate:
  virtual void DispatchKeyEventPostIME(const ui::KeyEvent& key) override;

 private:
  // Closes all child windows. BridgedNativeWidget children will be destroyed.
  void RemoveOrDestroyChildren();

  views::NativeWidgetMac* native_widget_mac_;  // Weak. Owns this.
  base::scoped_nsobject<NSWindow> window_;
  base::scoped_nsobject<ViewsNSWindowDelegate> window_delegate_;
  base::scoped_nsobject<BridgedContentView> bridged_view_;
  scoped_ptr<ui::InputMethod> input_method_;
  FocusManager* focus_manager_;  // Weak. Owned by our Widget.

  // Tracks the bounds when the window last started entering fullscreen. Used to
  // provide an answer for GetRestoredBounds(), but not ever sent to Cocoa (it
  // has its own copy, but doesn't provide access to it).
  gfx::Rect bounds_before_fullscreen_;

  // Whether this window wants to be fullscreen. If a fullscreen animation is in
  // progress then it might not be actually fullscreen.
  bool target_fullscreen_state_;

  // Whether this window is in a fullscreen transition, and the fullscreen state
  // can not currently be changed.
  bool in_fullscreen_transition_;

  // Overridden from FocusChangeListener:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) override;
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) override;

  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidget);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_H_
