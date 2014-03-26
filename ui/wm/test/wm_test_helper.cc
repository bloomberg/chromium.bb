// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/test/wm_test_helper.h"

#include "base/message_loop/message_loop.h"
#include "ui/aura/window.h"
#include "ui/wm/core/default_activation_client.h"

namespace wm {

WMTestHelper::WMTestHelper(const gfx::Size& default_window_size)
    : aura::test::AuraTestHelper(base::MessageLoopForUI::current()) {
  default_window_size_ = default_window_size;
}

WMTestHelper::WMTestHelper()
    : aura::test::AuraTestHelper(base::MessageLoopForUI::current()) {
}

WMTestHelper::~WMTestHelper() {
}

////////////////////////////////////////////////////////////////////////////////
// WMTestHelper, AuraTestHelper overrides:

void WMTestHelper::SetUp() {
  aura::test::AuraTestHelper::SetUp();
  activation_client_.reset(
      new aura::client::DefaultActivationClient(root_window()));
}

void WMTestHelper::TearDown() {
  activation_client_.reset();
  aura::test::AuraTestHelper::TearDown();
}


}  // namespace wm
