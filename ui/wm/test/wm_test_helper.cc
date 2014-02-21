// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/test/wm_test_helper.h"

#include "ui/aura/client/default_activation_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/corewm/compound_event_filter.h"
#include "ui/views/corewm/input_method_event_filter.h"

namespace wm {

WMTestHelper::WMTestHelper(const gfx::Size& default_window_size) {
  aura::Env::CreateInstance();
  dispatcher_.reset(new aura::WindowEventDispatcher(
      aura::WindowEventDispatcher::CreateParams(
          gfx::Rect(default_window_size))));
  dispatcher_->host()->InitHost();
  aura::client::SetWindowTreeClient(dispatcher_->window(), this);

  focus_client_.reset(new aura::test::TestFocusClient);
  aura::client::SetFocusClient(dispatcher_->window(), focus_client_.get());

  root_window_event_filter_ = new views::corewm::CompoundEventFilter;
  // Pass ownership of the filter to the root_window.
  dispatcher_->window()->SetEventFilter(root_window_event_filter_);

  input_method_filter_.reset(new views::corewm::InputMethodEventFilter(
      dispatcher_->host()->GetAcceleratedWidget()));
  input_method_filter_->SetInputMethodPropertyInRootWindow(
      dispatcher_->window());
  root_window_event_filter_->AddHandler(input_method_filter_.get());

  activation_client_.reset(
      new aura::client::DefaultActivationClient(dispatcher_->window()));

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(dispatcher_->window()));
}

WMTestHelper::~WMTestHelper() {
  root_window_event_filter_->RemoveHandler(input_method_filter_.get());
}

aura::Window* WMTestHelper::GetDefaultParent(aura::Window* context,
                                             aura::Window* window,
                                             const gfx::Rect& bounds) {
  return dispatcher_->window();
}

}  // namespace wm
