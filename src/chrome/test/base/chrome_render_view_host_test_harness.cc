// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_host_test_harness.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/test/base/testing_profile.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#endif

using content::RenderViewHostTester;
using content::RenderViewHostTestHarness;

ChromeRenderViewHostTestHarness::~ChromeRenderViewHostTestHarness() = default;

TestingProfile* ChromeRenderViewHostTestHarness::profile() {
  return static_cast<TestingProfile*>(browser_context());
}

void ChromeRenderViewHostTestHarness::TearDown() {
  RenderViewHostTestHarness::TearDown();
#if defined(OS_CHROMEOS)
  ash::Shell::DeleteInstance();
#endif
}

content::BrowserContext*
ChromeRenderViewHostTestHarness::CreateBrowserContext() {
  TestingProfile::Builder builder;
  return builder.Build().release();
}
