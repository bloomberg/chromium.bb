// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"

#include "base/run_loop.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/wm_state.h"

namespace views {

ViewsTestBase::ViewsTestBase()
    : setup_called_(false),
      teardown_called_(false) {
}

ViewsTestBase::~ViewsTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";
}

void ViewsTestBase::SetUp() {
  testing::Test::SetUp();
  setup_called_ = true;
  if (!views_delegate_.get())
    views_delegate_.reset(new TestViewsDelegate());
  // The ContextFactory must exist before any Compositors are created.
  bool enable_pixel_output = false;
  ui::InitializeContextFactoryForTests(enable_pixel_output);

  aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
  aura_test_helper_->SetUp();
  wm_state_.reset(new ::wm::WMState);
  ui::InitializeInputMethodForTesting();
}

void ViewsTestBase::TearDown() {
  ui::Clipboard::DestroyClipboardForCurrentThread();

  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunPendingMessages();
  teardown_called_ = true;
  views_delegate_.reset();
  testing::Test::TearDown();
  ui::ShutdownInputMethodForTesting();
  aura_test_helper_->TearDown();
  ui::TerminateContextFactoryForTests();
  wm_state_.reset();
  CHECK(!wm::ScopedCaptureClient::IsActive());
}

void ViewsTestBase::RunPendingMessages() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

Widget::InitParams ViewsTestBase::CreateParams(
    Widget::InitParams::Type type) {
  Widget::InitParams params(type);
  params.context = aura_test_helper_->root_window();
  return params;
}

ui::EventProcessor* ViewsTestBase::event_processor() {
  return aura_test_helper_->event_processor();
}

aura::WindowTreeHost* ViewsTestBase::host() {
  return aura_test_helper_->host();
}

gfx::NativeView ViewsTestBase::GetContext() {
  return aura_test_helper_->root_window();
}

}  // namespace views
