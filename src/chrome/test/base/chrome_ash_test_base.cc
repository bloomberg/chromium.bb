// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_ash_test_base.h"

#include "content/public/test/test_browser_thread_bundle.h"

ChromeAshTestBase::ChromeAshTestBase() {
  DestroyScopedTaskEnvironment();
  thread_bundle_ = std::make_unique<content::TestBrowserThreadBundle>();
}

ChromeAshTestBase::~ChromeAshTestBase() = default;
