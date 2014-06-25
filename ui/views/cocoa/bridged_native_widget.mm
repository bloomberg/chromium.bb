// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_native_widget.h"

#include "base/logging.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ui_base_switches_util.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/ime/null_input_method.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

BridgedNativeWidget::BridgedNativeWidget(NativeWidgetMac* parent)
    : native_widget_mac_(parent), focus_manager_(NULL) {
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
