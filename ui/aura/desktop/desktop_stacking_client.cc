// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop/desktop_stacking_client.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/desktop/desktop_activation_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"
#include "ui/aura/shared/input_method_event_filter.h"
#include "ui/aura/window.h"

namespace aura {

DesktopStackingClient::DesktopStackingClient()
    : window_event_filter_(NULL) {
  client::SetStackingClient(this);
}

DesktopStackingClient::~DesktopStackingClient() {
  if (window_event_filter_)
    window_event_filter_->RemoveFilter(input_method_filter_.get());

  client::SetStackingClient(NULL);
}

Window* DesktopStackingClient::GetDefaultParent(Window* window,
                                                const gfx::Rect& bounds) {
  if (!null_parent_.get())
    CreateNULLParent();

  return null_parent_.get();
}

void DesktopStackingClient::CreateNULLParent() {
  focus_manager_.reset(new FocusManager);

  activation_client_.reset(new DesktopActivationClient(focus_manager_.get()));

  null_parent_.reset(new RootWindow(
      RootWindow::CreateParams(gfx::Rect(100, 100))));
  null_parent_->Init();
  null_parent_->set_focus_manager(focus_manager_.get());

  client::SetActivationClient(null_parent_.get(), activation_client_.get());

  window_event_filter_ = new views::corewm::CompoundEventFilter;
  null_parent_->SetEventFilter(window_event_filter_);

  input_method_filter_.reset(new views::corewm::InputMethodEventFilter);
  input_method_filter_->SetInputMethodPropertyInRootWindow(null_parent_.get());
  window_event_filter_->AddFilter(input_method_filter_.get());

  capture_client_.reset(new client::DefaultCaptureClient(null_parent_.get()));
}

}  // namespace aura
