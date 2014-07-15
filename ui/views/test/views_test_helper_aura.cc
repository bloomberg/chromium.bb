// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_helper_aura.h"

#include "ui/aura/test/aura_test_helper.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/default_activation_client.h"
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
  new wm::DefaultActivationClient(aura_test_helper_->root_window());
  wm_state_.reset(new wm::WMState);
}

void ViewsTestHelperAura::TearDown() {
  aura_test_helper_->TearDown();
  wm_state_.reset();
  CHECK(!wm::ScopedCaptureClient::IsActive());
}

gfx::NativeWindow ViewsTestHelperAura::GetContext() {
  return aura_test_helper_->root_window();
}

}  // namespace views
