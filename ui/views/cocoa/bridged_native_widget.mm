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
      parent_(nullptr),
      target_fullscreen_state_(false),
      in_fullscreen_transition_(false),
      window_visible_(false),
      wants_to_be_visible_(false) {
  DCHECK(parent);
  window_delegate_.reset(
      [[ViewsNSWindowDelegate alloc] initWithBridgedNativeWidget:this]);
}

BridgedNativeWidget::~BridgedNativeWidget() {
  RemoveOrDestroyChildren();
  DCHECK(child_windows_.empty());
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

  // Register for application hide notifications so that visibility can be
  // properly tracked. This is not done in the delegate so that the lifetime is
  // tied to the C++ object, rather than the delegate (which may be reference
  // counted). This is required since the application hides do not send an
  // orderOut: to individual windows. Unhide, however, does send an order
  // message.
  [[NSNotificationCenter defaultCenter]
      addObserver:window_delegate_
         selector:@selector(onWindowOrderChanged:)
             name:NSApplicationDidHideNotification
           object:nil];

  // Validate the window's initial state, otherwise the bridge's initial
  // tracking state will be incorrect.
  DCHECK(![window_ isVisible]);
  DCHECK_EQ(0u, [window_ styleMask] & NSFullScreenWindowMask);

  if (params.parent) {
    // Disallow creating child windows of views not currently in an NSWindow.
    CHECK([params.parent window]);
    BridgedNativeWidget* parent =
        NativeWidgetMac::GetBridgeForNativeWindow([params.parent window]);
    // The parent could be an NSWindow without an associated Widget. That could
    // work by observing NSWindowWillCloseNotification, but for now it's not
    // supported, and there might not be a use-case for that.
    CHECK(parent);
    parent_ = parent;
    parent->child_windows_.push_back(this);
  }

  // Widgets for UI controls (usually layered above web contents) start visible.
  if (params.type == Widget::InitParams::TYPE_CONTROL)
    SetVisibilityState(SHOW_INACTIVE);
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

void BridgedNativeWidget::SetBounds(const gfx::Rect& new_bounds) {
  [window_ setFrame:gfx::ScreenRectToNSRect(new_bounds)
            display:YES
            animate:NO];
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

void BridgedNativeWidget::SetVisibilityState(WindowVisibilityState new_state) {
  // Ensure that:
  //  - A window with an invisible parent is not made visible.
  //  - A parent changing visibility updates child window visibility.
  //    * But only when changed via this function - ignore changes via the
  //      NSWindow API, or changes propagating out from here.
  wants_to_be_visible_ = new_state != HIDE_WINDOW;

  if (new_state == HIDE_WINDOW) {
    [window_ orderOut:nil];
    DCHECK(!window_visible_);
    NotifyVisibilityChangeDown();
    return;
  }

  DCHECK(wants_to_be_visible_);

  // If there's a hidden ancestor, return and wait for it to become visible.
  for (BridgedNativeWidget* ancestor = parent();
       ancestor;
       ancestor = ancestor->parent()) {
    if (!ancestor->window_visible_)
      return;
  }

  if (new_state == SHOW_AND_ACTIVATE_WINDOW) {
    [window_ makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
  } else {
    // ui::SHOW_STATE_INACTIVE is typically used to avoid stealing focus from a
    // parent window. So, if there's a parent, order above that. Otherwise, this
    // will order above all windows at the same level.
    NSInteger parent_window_number = 0;
    if (parent())
      parent_window_number = [parent()->ns_window() windowNumber];

    [window_ orderWindow:NSWindowAbove
              relativeTo:parent_window_number];
  }
  DCHECK(window_visible_);
  NotifyVisibilityChangeDown();
}

void BridgedNativeWidget::OnWindowWillClose() {
  if (parent_)
    parent_->RemoveChildWindow(this);
  [window_ setDelegate:nil];
  [[NSNotificationCenter defaultCenter] removeObserver:window_delegate_];
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

void BridgedNativeWidget::OnSizeChanged() {
  NSSize new_size = [window_ frame].size;
  native_widget_mac_->GetWidget()->OnNativeWidgetSizeChanged(
      gfx::Size(new_size.width, new_size.height));
  // TODO(tapted): If there's a layer, resize it here.
}

void BridgedNativeWidget::OnVisibilityChanged() {
  if (window_visible_ == [window_ isVisible])
    return;

  window_visible_ = [window_ isVisible];

  // If arriving via SetVisible(), |wants_to_be_visible_| should already be set.
  // If made visible externally (e.g. Cmd+H), just roll with it. Don't try (yet)
  // to distinguish being *hidden* externally from being hidden by a parent
  // window - we might not need that.
  if (window_visible_)
    wants_to_be_visible_ = true;

  native_widget_mac_->GetWidget()->OnNativeWidgetVisibilityChanged(
      window_visible_);
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

////////////////////////////////////////////////////////////////////////////////
// BridgedNativeWidget, FocusChangeListener:

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
  while (!child_windows_.empty()) {
    // The NSWindow can only be destroyed after -[NSWindow close] is complete.
    // Retain the window, otherwise the reference count can reach zero when the
    // child calls back into RemoveChildWindow() via its OnWindowWillClose().
    base::scoped_nsobject<NSWindow> child(
        [child_windows_.back()->ns_window() retain]);
    [child close];
  }
}

void BridgedNativeWidget::RemoveChildWindow(BridgedNativeWidget* child) {
  auto location = std::find(
      child_windows_.begin(), child_windows_.end(), child);
  DCHECK(location != child_windows_.end());
  child_windows_.erase(location);
  child->parent_ = nullptr;
}

void BridgedNativeWidget::NotifyVisibilityChangeDown() {
  // Child windows sometimes like to close themselves in response to visibility
  // changes. That's supported, but only with the asynchronous Widget::Close().
  // Perform a heuristic to detect child removal that would break these loops.
  const size_t child_count = child_windows_.size();
  if (!window_visible_) {
    for (BridgedNativeWidget* child : child_windows_) {
      if (child->window_visible_) {
        [child->ns_window() orderOut:nil];
        child->NotifyVisibilityChangeDown();
        CHECK_EQ(child_count, child_windows_.size());
      }
    }
    return;
  }

  NSInteger parent_window_number = [window_ windowNumber];
  for (BridgedNativeWidget* child: child_windows_) {
    // Note: order the child windows on top, regardless of whether or not they
    // are currently visible. They probably aren't, since the parent was hidden
    // prior to this, but they could have been made visible in other ways.
    if (child->wants_to_be_visible_) {
      [child->ns_window() orderWindow:NSWindowAbove
                           relativeTo:parent_window_number];
      child->NotifyVisibilityChangeDown();
      CHECK_EQ(child_count, child_windows_.size());
    }
  }
}

}  // namespace views
