// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/test/wm_test_helper.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/window.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/wm_state.h"

namespace wm {

WMTestHelper::WMTestHelper(const gfx::Size& default_window_size,
                           ui::ContextFactory* context_factory) {
  wm_state_ = std::make_unique<WMState>();
  aura::Env::GetInstance()->set_context_factory(context_factory);
  host_.reset(aura::WindowTreeHost::Create(gfx::Rect(default_window_size)));
  host_->InitHost();
  aura::client::SetWindowParentingClient(host_->window(), this);

  focus_client_.reset(new aura::test::TestFocusClient);
  aura::client::SetFocusClient(host_->window(), focus_client_.get());

  root_window_event_filter_.reset(new wm::CompoundEventFilter);
  host_->window()->AddPreTargetHandler(root_window_event_filter_.get());

  new wm::DefaultActivationClient(host_->window());

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(host_->window()));
}

WMTestHelper::~WMTestHelper() {
}

aura::Window* WMTestHelper::GetDefaultParent(aura::Window* window,
                                             const gfx::Rect& bounds) {
  return host_->window();
}

}  // namespace wm
