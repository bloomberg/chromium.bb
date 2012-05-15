// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_native_widget_helper_aura.h"

#include "ui/aura/root_window.h"
#include "ui/aura/desktop/desktop_activation_client.h"
#include "ui/aura/desktop/desktop_dispatcher_client.h"
#include "ui/aura/client/dispatcher_client.h"
#include "ui/aura/shared/input_method_event_filter.h"
#include "ui/aura/shared/root_window_event_filter.h"
#include "ui/views/widget/native_widget_aura.h"

#if defined(OS_WIN)
#include "ui/base/win/hwnd_subclass.h"
#include "ui/views/widget/widget_message_filter.h"
#endif

namespace views {

DesktopNativeWidgetHelperAura::DesktopNativeWidgetHelperAura(
    NativeWidgetAura* widget)
    : widget_(widget),
      root_window_event_filter_(NULL),
      is_embedded_window_(false) {
}

DesktopNativeWidgetHelperAura::~DesktopNativeWidgetHelperAura() {
  if (root_window_event_filter_)
    root_window_event_filter_->RemoveFilter(input_method_filter_.get());
}

void DesktopNativeWidgetHelperAura::PreInitialize(
    const Widget::InitParams& params) {
  // We don't want the status bubble or the omnibox to get their own root
  // window on the desktop; on Linux
  //
  // TODO(erg): This doesn't map perfectly to what I want to do. TYPE_POPUP is
  // used for lots of stuff, like dragged tabs, and I only want this to trigger
  // for the status bubble and the omnibox.
  if (params.type == Widget::InitParams::TYPE_POPUP) {
    is_embedded_window_ = true;
    return;
  } else if (params.type == Widget::InitParams::TYPE_CONTROL) {
    return;
  }

  gfx::Rect bounds = params.bounds;
  if (bounds.IsEmpty()) {
    // We must pass some non-zero value when we initialize a RootWindow. This
    // will probably be SetBounds()ed soon.
    bounds.set_size(gfx::Size(100, 100));
  }
  root_window_.reset(new aura::RootWindow(bounds));
  root_window_->Init();

  root_window_event_filter_ =
      new aura::shared::RootWindowEventFilter(root_window_.get());
  root_window_->SetEventFilter(root_window_event_filter_);

  input_method_filter_.reset(
      new aura::shared::InputMethodEventFilter(root_window_.get()));
  root_window_event_filter_->AddFilter(input_method_filter_.get());

  root_window_->AddRootWindowObserver(this);

  aura::client::SetActivationClient(
      root_window_.get(),
      new aura::DesktopActivationClient(root_window_.get()));
  aura::client::SetDispatcherClient(root_window_.get(),
                                    new aura::DesktopDispatcherClient);
}

void DesktopNativeWidgetHelperAura::PostInitialize() {
#if defined(OS_WIN)
  subclass_.reset(new ui::HWNDSubclass(root_window_->GetAcceleratedWidget()));
  subclass_->SetFilter(new WidgetMessageFilter(root_window_.get(),
                                               widget_->GetWidget()));
#endif
}

void DesktopNativeWidgetHelperAura::ShowRootWindow() {
  if (root_window_.get())
    root_window_->ShowRootWindow();
}

aura::RootWindow* DesktopNativeWidgetHelperAura::GetRootWindow() {
  return root_window_.get();
}

gfx::Rect DesktopNativeWidgetHelperAura::ModifyAndSetBounds(
    const gfx::Rect& bounds) {
  gfx::Rect out_bounds = bounds;
  if (root_window_.get() && !out_bounds.IsEmpty()) {
    root_window_->SetHostBounds(out_bounds);
    out_bounds.set_x(0);
    out_bounds.set_y(0);
  } else if (is_embedded_window_) {
    // The caller expects windows we consider "embedded" to be placed in the
    // screen coordinate system. So we need to offset the root window's
    // position (which is in screen coordinates) from these bounds.
    aura::RootWindow* root =
        widget_->GetNativeWindow()->GetRootWindow()->GetRootWindow();
    gfx::Point point = root->GetHostOrigin();
    out_bounds.set_x(out_bounds.x() - point.x());
    out_bounds.set_y(out_bounds.y() - point.y());
  }

  return out_bounds;
}

gfx::Rect DesktopNativeWidgetHelperAura::ChangeRootWindowBoundsToScreenBounds(
    const gfx::Rect& bounds) {
  gfx::Rect out_bounds = bounds;
  if (root_window_.get()) {
    out_bounds.Offset(root_window_->GetHostOrigin());
  } else if (is_embedded_window_) {
    aura::RootWindow* root =
        widget_->GetNativeWindow()->GetRootWindow()->GetRootWindow();
    out_bounds.Offset(root->GetHostOrigin());
  }
  return out_bounds;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetHelperAura, aura::RootWindowObserver implementation:

void DesktopNativeWidgetHelperAura::OnRootWindowResized(
    const aura::RootWindow* root,
    const gfx::Size& old_size) {
  DCHECK_EQ(root, root_window_.get());
  widget_->SetBounds(gfx::Rect(root->GetHostOrigin(),
                               root->GetHostSize()));
}

void DesktopNativeWidgetHelperAura::OnRootWindowHostClosed(
    const aura::RootWindow* root) {
  DCHECK_EQ(root, root_window_.get());
  widget_->GetWidget()->Close();
}

}  // namespace views
