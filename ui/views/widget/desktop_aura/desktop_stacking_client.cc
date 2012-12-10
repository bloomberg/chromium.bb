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
    window_event_filter_->RemoveHandler(input_method_filter_.get());

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
  focus_client_.reset(new aura::FocusManager);

  null_parent_.reset(new aura::RootWindow(
      aura::RootWindow::CreateParams(gfx::Rect(100, 100))));
  null_parent_->Init();
  aura::client::SetFocusClient(null_parent_.get(), focus_client_.get());

  activation_client_.reset(new DesktopActivationClient(null_parent_.get()));

  window_event_filter_ = new corewm::CompoundEventFilter;
  null_parent_->SetEventFilter(window_event_filter_);

  input_method_filter_.reset(new corewm::InputMethodEventFilter(
                                 null_parent_->GetAcceleratedWidget()));
  input_method_filter_->SetInputMethodPropertyInRootWindow(null_parent_.get());
  window_event_filter_->AddHandler(input_method_filter_.get());

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(null_parent_.get()));

  // Hide the window so we don't attempt to draw to it and what not.
  null_parent_->Hide();
}

}  // namespace views
