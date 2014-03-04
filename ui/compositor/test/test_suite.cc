// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_suite.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_WIN)
#include "ui/gfx/win/dpi.h"
#endif

namespace ui {
namespace test {

CompositorTestSuite::CompositorTestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {}

CompositorTestSuite::~CompositorTestSuite() {}

void CompositorTestSuite::Initialize() {
  base::TestSuite::Initialize();
  gfx::GLSurface::InitializeOneOffForTests();

  gfx::RegisterPathProvider();

#if defined(OS_WIN)
  gfx::InitDeviceScaleFactor(1.0f);
#endif

  message_loop_.reset(new base::MessageLoopForUI);
}

void CompositorTestSuite::Shutdown() {
  message_loop_.reset();

  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace ui
