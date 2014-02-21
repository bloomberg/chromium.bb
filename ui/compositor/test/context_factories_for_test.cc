// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/context_factories_for_test.h"

#include "base/command_line.h"
#include "base/sys_info.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/compositor/test/test_context_factory.h"
#include "ui/gl/gl_implementation.h"

namespace {

static ui::ContextFactory* g_implicit_factory = NULL;
static gfx::DisableNullDrawGLBindings* g_disable_null_draw = NULL;

}  // namespace

namespace ui {

// static
void InitializeContextFactoryForTests(bool enable_pixel_output) {
  DCHECK(!g_implicit_factory) <<
      "ContextFactory for tests already initialized.";
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnablePixelOutputInTests))
    enable_pixel_output = true;

  bool use_test_contexts = true;

  if (gfx::HasInitializedNullDrawGLBindings())
    use_test_contexts = false;

  // Always use test contexts unless the disable command line flag is used.
  if (command_line->HasSwitch(switches::kDisableTestCompositor))
    use_test_contexts = false;

#if defined(OS_CHROMEOS)
  // If the test is running on the chromeos envrionment (such as
  // device or vm bots), always use real contexts.
  if (base::SysInfo::IsRunningOnChromeOS())
    use_test_contexts = false;
#endif

  if (enable_pixel_output)
    use_test_contexts = false;

  if (use_test_contexts) {
    g_implicit_factory = new ui::TestContextFactory;
  } else {
    DCHECK_NE(gfx::kGLImplementationNone, gfx::GetGLImplementation());
    DVLOG(1) << "Using InProcessContextFactory";
    if (enable_pixel_output && gfx::HasInitializedNullDrawGLBindings())
      g_disable_null_draw = new gfx::DisableNullDrawGLBindings;
    g_implicit_factory = new ui::InProcessContextFactory();
  }
  ContextFactory::SetInstance(g_implicit_factory);
}

void TerminateContextFactoryForTests() {
  ContextFactory::SetInstance(NULL);
  delete g_implicit_factory;
  g_implicit_factory = NULL;
  delete g_disable_null_draw;
  g_disable_null_draw = NULL;
}

}  // namespace ui
