// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_stacking_client.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/compound_event_filter.h"
#include "ui/views/corewm/input_method_event_filter.h"
#include "ui/views/widget/desktop_aura/desktop_activation_client.h"

namespace views {

DesktopStackingClient::DesktopStackingClient()
    : window_event_filter_(NULL) {
  aura::client::SetStackingClient(this);
}

DesktopStackingClient::~DesktopStackingClient() {
  if (window_event_filter_)
    window_event_filter_->RemoveFilter(input_method_filter_.get());

  aura::client::SetStackingClient(NULL);
}

aura::Window* DesktopStackingClient::GetDefaultParent(aura::Window* context,
                                                      aura::Window* window,
                                                      const gfx::Rect& bounds) {
  if (!null_parent_.get())
    CreateNULLParent();

  return null_parent_.get();
}

void DesktopStackingClient::CreateNULLParent() {
  focus_manager_.reset(new aura::FocusManager);

  activation_client_.reset(new DesktopActivationClient(focus_manager_.get()));

  null_parent_.reset(new aura::RootWindow(
      aura::RootWindow::CreateParams(gfx::Rect(100, 100))));
  null_parent_->Init();
  null_parent_->set_focus_manager(focus_manager_.get());

  aura::client::SetActivationClient(null_parent_.get(),
                                    activation_client_.get());

  window_event_filter_ = new corewm::CompoundEventFilter;
  null_parent_->SetEventFilter(window_event_filter_);

  input_method_filter_.reset(new corewm::InputMethodEventFilter);
  input_method_filter_->SetInputMethodPropertyInRootWindow(null_parent_.get());
  window_event_filter_->AddFilter(input_method_filter_.get());

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(null_parent_.get()));
}

}  // namespace views
