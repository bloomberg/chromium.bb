// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_native_widget.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ui_base_switches_util.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/ime/null_input_method.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

BridgedNativeWidget::BridgedNativeWidget(NativeWidgetMac* parent)
    : native_widget_mac_(parent),
      focus_manager_(NULL),
      target_fullscreen_state_(false),
      in_fullscreen_transition_(false) {
  DCHECK(parent);
  window_delegate_.reset(
      [[ViewsNSWindowDelegate alloc] initWithBridgedNativeWidget:this]);
}

BridgedNativeWidget::~BridgedNativeWidget() {
  RemoveOrDestroyChildren();
  SetFocusManager(NULL);
  SetRootView(NULL);
  if ([window_ delegate]) {
    // If the delegate is still set, it means OnWindowWillClose has not been
    // called and the window is still open. Calling -[NSWindow close] will
    // synchronously call OnWindowWillClose and notify NativeWidgetMac.
    [window_ close];
  }
  DCHECK(![window_ delegate]);
}

void BridgedNativeWidget::Init(base::scoped_nsobject<NSWindow> window,
                               const Widget::InitParams& params) {
  DCHECK(!window_);
  window_.swap(window);
  [window_ setDelegate:window_delegate_];

  // Validate the window's initial state, otherwise the bridge's initial
  // tracking state will be incorrect.
  DCHECK(![window_ isVisible]);
  DCHECK_EQ(0u, [window_ styleMask] & NSFullScreenWindowMask);

  if (params.parent) {
    // Use NSWindow to manage child windows. This won't automatically close them
    // but it will maintain relative positioning of the window layer and origin.
    [[params.parent window] addChildWindow:window_ ordered:NSWindowAbove];
  }
}

void BridgedNativeWidget::SetFocusManager(FocusManager* focus_manager) {
  if (focus_manager_ == focus_manager)
    return;

  if (focus_manager_)
    focus_manager_->RemoveFocusChangeListener(this);

  if (focus_manager)
    focus_manager->AddFocusChangeListener(this);

  focus_manager_ = focus_manager;
}

void BridgedNativeWidget::SetRootView(views::View* view) {
  if (view == [bridged_view_ hostedView])
    return;

  [bridged_view_ clearView];
  bridged_view_.reset();
  // Note that there can still be references to the old |bridged_view_|
  // floating around in Cocoa libraries at this point. However, references to
  // the old views::View will be gone, so any method calls will become no-ops.

  if (view) {
    bridged_view_.reset([[BridgedContentView alloc] initWithView:view]);
    // Objective C initializers can return nil. However, if |view| is non-NULL
    // this should be treated as an error and caught early.
    CHECK(bridged_view_);
  }
  [window_ setContentView:bridged_view_];
}

void BridgedNativeWidget::OnWindowWillClose() {
  [[window_ parentWindow] removeChildWindow:window_];
  [window_ setDelegate:nil];
  native_widget_mac_->OnWindowWillClose();
}

void BridgedNativeWidget::OnFullscreenTransitionStart(
    bool target_fullscreen_state) {
  DCHECK_NE(target_fullscreen_state, target_fullscreen_state_);
  target_fullscreen_state_ = target_fullscreen_state;
  in_fullscreen_transition_ = true;

  // If going into fullscreen, store an answer for GetRestoredBounds().
  if (target_fullscreen_state)
    bounds_before_fullscreen_ = gfx::ScreenRectFromNSRect([window_ frame]);
}

void BridgedNativeWidget::OnFullscreenTransitionComplete(
    bool actual_fullscreen_state) {
  in_fullscreen_transition_ = false;
  if (target_fullscreen_state_ == actual_fullscreen_state)
    return;

  // First update to reflect reality so that OnTargetFullscreenStateChanged()
  // expects the change.
  target_fullscreen_state_ = actual_fullscreen_state;
  ToggleDesiredFullscreenState();

  // Usually ToggleDesiredFullscreenState() sets |in_fullscreen_transition_| via
  // OnFullscreenTransitionStart(). When it does not, it means Cocoa ignored the
  // toggleFullScreen: request. This can occur when the fullscreen transition
  // fails and Cocoa is *about* to send windowDidFailToEnterFullScreen:.
  // Annoyingly, for this case, Cocoa first sends windowDidExitFullScreen:.
  if (in_fullscreen_transition_)
    DCHECK_NE(target_fullscreen_state_, actual_fullscreen_state);
}

void BridgedNativeWidget::ToggleDesiredFullscreenState() {
  if (base::mac::IsOSSnowLeopard()) {
    NOTIMPLEMENTED();
    return;  // TODO(tapted): Implement this for Snow Leopard.
  }

  // If there is currently an animation into or out of fullscreen, then AppKit
  // emits the string "not in fullscreen state" to stdio and does nothing. For
  // this case, schedule a transition back into the desired state when the
  // animation completes.
  if (in_fullscreen_transition_) {
    target_fullscreen_state_ = !target_fullscreen_state_;
    return;
  }

  // Since fullscreen requests are ignored if the collection behavior does not
  // allow it, save the collection behavior and restore it after.
  NSWindowCollectionBehavior behavior = [window_ collectionBehavior];
  [window_ setCollectionBehavior:behavior |
                                 NSWindowCollectionBehaviorFullScreenPrimary];
  [window_ toggleFullScreen:nil];
  [window_ setCollectionBehavior:behavior];
}

InputMethod* BridgedNativeWidget::CreateInputMethod() {
  if (switches::IsTextInputFocusManagerEnabled())
    return new NullInputMethod();

  return new InputMethodBridge(this, GetHostInputMethod(), true);
}

ui::InputMethod* BridgedNativeWidget::GetHostInputMethod() {
  if (!input_method_) {
    // Delegate is NULL because Mac IME does not need DispatchKeyEventPostIME
    // callbacks.
    input_method_ = ui::CreateInputMethod(NULL, nil);
  }
  return input_method_.get();
}

gfx::Rect BridgedNativeWidget::GetRestoredBounds() const {
  if (target_fullscreen_state_ || in_fullscreen_transition_)
    return bounds_before_fullscreen_;

  return gfx::ScreenRectFromNSRect([window_ frame]);
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, internal::InputMethodDelegate:

void BridgedNativeWidget::DispatchKeyEventPostIME(const ui::KeyEvent& key) {
  // Mac key events don't go through this, but some unit tests that use
  // MockInputMethod do.
  DCHECK(focus_manager_);
  native_widget_mac_->GetWidget()->OnKeyEvent(const_cast<ui::KeyEvent*>(&key));
  if (!key.handled())
    focus_manager_->OnKeyEvent(key);
}

void BridgedNativeWidget::OnWillChangeFocus(View* focused_before,
                                            View* focused_now) {
}

void BridgedNativeWidget::OnDidChangeFocus(View* focused_before,
                                           View* focused_now) {
  ui::TextInputClient* input_client =
      focused_now ? focused_now->GetTextInputClient() : NULL;
  [bridged_view_ setTextInputClient:input_client];
}

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, private:

void BridgedNativeWidget::RemoveOrDestroyChildren() {
  // TODO(tapted): Implement unowned child windows if required.
  base::scoped_nsobject<NSArray> child_windows(
      [[NSArray alloc] initWithArray:[window_ childWindows]]);
  [child_windows makeObjectsPerformSelector:@selector(close)];
}

}  // namespace views
