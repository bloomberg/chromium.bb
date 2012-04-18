// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_native_widget_helper_aura.h"

#include "ui/views/widget/native_widget_aura.h"
#include "ui/aura/root_window.h"
#include "ui/aura/desktop/desktop_activation_client.h"
#include "ui/aura/desktop/desktop_dispatcher_client.h"
#include "ui/aura/desktop/desktop_root_window_event_filter.h"
#include "ui/aura/client/dispatcher_client.h"

namespace views {

DesktopNativeWidgetHelperAura::DesktopNativeWidgetHelperAura(
    NativeWidgetAura* widget)
    : widget_(widget) {
}

DesktopNativeWidgetHelperAura::~DesktopNativeWidgetHelperAura() {}

void DesktopNativeWidgetHelperAura::PreInitialize(
    const Widget::InitParams& params) {
  gfx::Rect bounds = params.bounds;
  if (bounds.IsEmpty()) {
    // We must pass some non-zero value when we initialize a RootWindow. This
    // will probably be SetBounds()ed soon.
    bounds.set_size(gfx::Size(100, 100));
  }
  root_window_.reset(new aura::RootWindow(bounds));
  root_window_->SetEventFilter(
      new aura::DesktopRootWindowEventFilter(root_window_.get()));
  root_window_->AddRootWindowObserver(this);

  aura::client::SetActivationClient(
      root_window_.get(),
      new aura::DesktopActivationClient(root_window_.get()));
  aura::client::SetDispatcherClient(root_window_.get(),
                                    new aura::DesktopDispatcherClient);
}

void DesktopNativeWidgetHelperAura::ShowRootWindow() {
  if (root_window_.get())
    root_window_->ShowRootWindow();
}

aura::RootWindow* DesktopNativeWidgetHelperAura::GetRootWindow() {
  return root_window_.get();
}

gfx::Rect DesktopNativeWidgetHelperAura::ModifyAndSetBounds(gfx::Rect bounds) {
  if (root_window_.get() && !bounds.IsEmpty()) {
    root_window_->SetHostBounds(bounds);
    bounds.set_x(0);
    bounds.set_y(0);
  }

  return bounds;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetHelperAura, aura::RootWindowObserver implementation:

void DesktopNativeWidgetHelperAura::OnRootWindowResized(
    const aura::RootWindow* root,
    const gfx::Size& old_size) {
  DCHECK_EQ(root, root_window_.get());
  widget_->SetBounds(gfx::Rect(root->GetHostSize()));
}

void DesktopNativeWidgetHelperAura::OnRootWindowHostClosed(
    const aura::RootWindow* root) {
  DCHECK_EQ(root, root_window_.get());
  widget_->GetWidget()->Close();
}

}  // namespace views
