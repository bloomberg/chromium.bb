// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper_aura.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/default_screen_position_client.h"
#include "ui/wm/core/wm_state.h"

namespace views {

// static
ViewsTestHelper* ViewsTestHelper::Create(base::MessageLoopForUI* message_loop,
                                         ui::ContextFactory* context_factory) {
  return new ViewsTestHelperAura(message_loop, context_factory);
}

ViewsTestHelperAura::ViewsTestHelperAura(base::MessageLoopForUI* message_loop,
                                         ui::ContextFactory* context_factory)
    : context_factory_(context_factory) {
  aura_test_helper_.reset(new aura::test::AuraTestHelper(message_loop));
}

ViewsTestHelperAura::~ViewsTestHelperAura() {
}

void ViewsTestHelperAura::SetUp() {
  aura_test_helper_->SetUp(context_factory_);
  gfx::NativeWindow root_window = GetContext();
  new wm::DefaultActivationClient(root_window);
  wm_state_.reset(new wm::WMState);

  if (!aura::client::GetScreenPositionClient(root_window)) {
    screen_position_client_.reset(new wm::DefaultScreenPositionClient);
    aura::client::SetScreenPositionClient(root_window,
                                          screen_position_client_.get());
  }
}

void ViewsTestHelperAura::TearDown() {
  // Ensure all Widgets (and windows) are closed in unit tests. This is done
  // automatically when the RootWindow is torn down, but is an error on
  // platforms that must ensure no Compositors are alive when the ContextFactory
  // is torn down.
  // So, although it's optional, check the root window to detect failures before
  // they hit the CQ on other platforms.
  DCHECK(aura_test_helper_->root_window()->children().empty())
      << "Not all windows were closed.";

  if (screen_position_client_.get() ==
      aura::client::GetScreenPositionClient(GetContext()))
    aura::client::SetScreenPositionClient(GetContext(), nullptr);

  aura_test_helper_->TearDown();
  wm_state_.reset();
  CHECK(!wm::ScopedCaptureClient::IsActive());
}

gfx::NativeWindow ViewsTestHelperAura::GetContext() {
  return aura_test_helper_->root_window();
}

}  // namespace views
