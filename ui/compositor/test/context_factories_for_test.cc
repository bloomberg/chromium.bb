// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/context_factories_for_test.h"

#include "base/command_line.h"
#include "base/sys_info.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/test/default_context_factory.h"
#include "ui/compositor/test/test_context_factory.h"
#include "ui/gl/gl_implementation.h"

namespace ui {

static ContextFactory* g_implicit_factory = NULL;

// static
void InitializeContextFactoryForTests(bool allow_test_contexts) {
  DCHECK(!g_implicit_factory) <<
      "ContextFactory for tests already initialized.";

  bool use_test_contexts = true;

  // Always use test contexts unless the disable command line flag is used.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableTestCompositor))
    use_test_contexts = false;

#if defined(OS_CHROMEOS)
  // If the test is running on the chromeos envrionment (such as
  // device or vm bots), always use real contexts.
  if (base::SysInfo::IsRunningOnChromeOS())
    use_test_contexts = false;
#endif

  if (!allow_test_contexts)
    use_test_contexts = false;

  if (use_test_contexts) {
    g_implicit_factory = new ui::TestContextFactory;
  } else {
    DCHECK_NE(gfx::kGLImplementationNone, gfx::GetGLImplementation());
    DVLOG(1) << "Using DefaultContextFactory";
    g_implicit_factory = new ui::DefaultContextFactory();
  }
  ContextFactory::SetInstance(g_implicit_factory);
}

void TerminateContextFactoryForTests() {
  ContextFactory::SetInstance(NULL);
  delete g_implicit_factory;
  g_implicit_factory = NULL;
}

}  // namespace ui
