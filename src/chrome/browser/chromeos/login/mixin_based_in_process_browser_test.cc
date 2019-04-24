// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"

#include <utility>

#include "base/containers/adapters.h"

namespace chromeos {

InProcessBrowserTestMixin::InProcessBrowserTestMixin(
    InProcessBrowserTestMixinHost* host) {
  host->mixins_.push_back(this);
}

InProcessBrowserTestMixin::~InProcessBrowserTestMixin() = default;

void InProcessBrowserTestMixin::SetUp() {}

void InProcessBrowserTestMixin::SetUpCommandLine(
    base::CommandLine* command_line) {}

void InProcessBrowserTestMixin::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {}

void InProcessBrowserTestMixin::SetUpInProcessBrowserTestFixture() {}

void InProcessBrowserTestMixin::SetUpOnMainThread() {}

void InProcessBrowserTestMixin::TearDownOnMainThread() {}

void InProcessBrowserTestMixin::TearDownInProcessBrowserTestFixture() {}

void InProcessBrowserTestMixin::TearDown() {}

InProcessBrowserTestMixinHost::InProcessBrowserTestMixinHost() = default;

InProcessBrowserTestMixinHost::~InProcessBrowserTestMixinHost() = default;

void InProcessBrowserTestMixinHost::SetUp() {
  for (InProcessBrowserTestMixin* mixin : mixins_)
    mixin->SetUp();
}

void InProcessBrowserTestMixinHost::SetUpCommandLine(
    base::CommandLine* command_line) {
  for (InProcessBrowserTestMixin* mixin : mixins_)
    mixin->SetUpCommandLine(command_line);
}

void InProcessBrowserTestMixinHost::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  for (InProcessBrowserTestMixin* mixin : mixins_)
    mixin->SetUpDefaultCommandLine(command_line);
}

void InProcessBrowserTestMixinHost::SetUpInProcessBrowserTestFixture() {
  for (InProcessBrowserTestMixin* mixin : mixins_)
    mixin->SetUpInProcessBrowserTestFixture();
}

void InProcessBrowserTestMixinHost::SetUpOnMainThread() {
  for (InProcessBrowserTestMixin* mixin : mixins_)
    mixin->SetUpOnMainThread();
}

void InProcessBrowserTestMixinHost::TearDownOnMainThread() {
  for (InProcessBrowserTestMixin* mixin : base::Reversed(mixins_))
    mixin->TearDownOnMainThread();
}

void InProcessBrowserTestMixinHost::TearDownInProcessBrowserTestFixture() {
  for (InProcessBrowserTestMixin* mixin : base::Reversed(mixins_))
    mixin->TearDownInProcessBrowserTestFixture();
}

void InProcessBrowserTestMixinHost::TearDown() {
  for (InProcessBrowserTestMixin* mixin : base::Reversed(mixins_))
    mixin->TearDown();
}

MixinBasedInProcessBrowserTest::MixinBasedInProcessBrowserTest() = default;

MixinBasedInProcessBrowserTest::~MixinBasedInProcessBrowserTest() = default;

void MixinBasedInProcessBrowserTest::SetUp() {
  mixin_host_.SetUp();
  InProcessBrowserTest::SetUp();
}

void MixinBasedInProcessBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  mixin_host_.SetUpCommandLine(command_line);
  InProcessBrowserTest::SetUpCommandLine(command_line);
}

void MixinBasedInProcessBrowserTest::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  mixin_host_.SetUpDefaultCommandLine(command_line);
  InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
}

void MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture() {
  mixin_host_.SetUpInProcessBrowserTestFixture();
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
}

void MixinBasedInProcessBrowserTest::SetUpOnMainThread() {
  mixin_host_.SetUpOnMainThread();
  InProcessBrowserTest::SetUpOnMainThread();
}

void MixinBasedInProcessBrowserTest::TearDownOnMainThread() {
  mixin_host_.TearDownOnMainThread();
  InProcessBrowserTest::TearDownOnMainThread();
}

void MixinBasedInProcessBrowserTest::TearDownInProcessBrowserTestFixture() {
  mixin_host_.TearDownInProcessBrowserTestFixture();
  InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
}

void MixinBasedInProcessBrowserTest::TearDown() {
  mixin_host_.TearDown();
  InProcessBrowserTest::TearDown();
}

}  // namespace chromeos
