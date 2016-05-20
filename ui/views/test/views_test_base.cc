// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"

#include <utility>

#include "base/run_loop.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/views/test/platform_test_helper.h"

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

// static
bool ViewsTestBase::IsMus() {
  return PlatformTestHelper::IsMus();
}

void ViewsTestBase::SetUp() {
  testing::Test::SetUp();
  // ContentTestSuiteBase might have already initialized
  // MaterialDesignController in unit_tests suite.
  ui::test::MaterialDesignControllerTestAPI::Uninitialize();
  ui::MaterialDesignController::Initialize();
  setup_called_ = true;
  if (!views_delegate_for_setup_)
    views_delegate_for_setup_.reset(new TestViewsDelegate());

  test_helper_.reset(
      new ScopedViewsTestHelper(std::move(views_delegate_for_setup_)));
}

void ViewsTestBase::TearDown() {
  ui::Clipboard::DestroyClipboardForCurrentThread();

  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunPendingMessages();
  teardown_called_ = true;
  testing::Test::TearDown();
  test_helper_.reset();
}

void ViewsTestBase::RunPendingMessages() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

Widget::InitParams ViewsTestBase::CreateParams(
    Widget::InitParams::Type type) {
  Widget::InitParams params(type);
  params.context = GetContext();
  return params;
}

gfx::NativeWindow ViewsTestBase::GetContext() {
  return test_helper_->GetContext();
}

}  // namespace views
