// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run all of our test shell tests.  This is just an entry point
// to kick off gTest's RUN_ALL_TESTS().

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#endif

#include "base/command_line.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/test/test_suite.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_platform_delegate.h"
#include "webkit/tools/test_shell/test_shell_switches.h"
#include "webkit/tools/test_shell/test_shell_test.h"
#include "webkit/tools/test_shell/test_shell_webkit_init.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/path_service.h"
#endif

const char* const TestShellTest::kJavascriptDelayExitScript =
  "<script>"
    "window.layoutTestController.waitUntilDone();"
    "window.addEventListener('load', function() {"
    "  var x = document.body.clientWidth;"  // Force a document layout
    "  window.layoutTestController.notifyDone();"
    "});"
  "</script>";

class TestShellTestSuite : public base::TestSuite {
 public:
  TestShellTestSuite(int argc, char** argv)
      : base::TestSuite(argc, argv),
        test_shell_webkit_init_(true),
        platform_delegate_(*CommandLine::ForCurrentProcess()) {
  }

  virtual void Initialize() {
    // Override DIR_EXE early in case anything in base::TestSuite uses it.
#if defined(OS_MACOSX)
    FilePath path;
    PathService::Get(base::DIR_EXE, &path);
    path = path.AppendASCII("TestShell.app");
    base::mac::SetOverrideAppBundlePath(path);
#endif

    base::TestSuite::Initialize();

    const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

    // Allow tests to analyze GC information from V8 log, and expose GC
    // triggering function.
    std::string js_flags =
        parsed_command_line.GetSwitchValueASCII(test_shell::kJavaScriptFlags);
    js_flags += " --expose_gc";
    webkit_glue::SetJavaScriptFlags(js_flags);

    // Suppress error dialogs and do not show GP fault error box on Windows.
    TestShell::InitLogging(true, false, false);

    // Some of the individual tests wind up calling TestShell::WaitTestFinished
    // which has a timeout in it.  For these tests, we don't care about
    // a timeout so just set it to be really large.  This is necessary because
    // we hit those timeouts under Purify and Valgrind.
    TestShell::SetFileTestTimeout(10 * 60 * 60 * 1000);  // Ten hours.

    // Initialize test shell in layout test mode, which will let us load one
    // request than automatically quit.
    TestShell::InitializeTestShell(true, false);

    platform_delegate_.InitializeGUI();
    platform_delegate_.SelectUnifiedTheme();
  }

  virtual void Shutdown() {
    TestShell::ShutdownTestShell();
    TestShell::CleanupLogging();

    base::TestSuite::Shutdown();
  }

 private:
  // Allocate a message loop for this thread.  Although it is not used
  // directly, its constructor sets up some necessary state.
  MessageLoopForUI main_message_loop_;

  // Initialize WebKit for this scope.
  TestShellWebKitInit test_shell_webkit_init_;

  TestShellPlatformDelegate platform_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestShellTestSuite);
};

int main(int argc, char** argv) {
  base::mac::ScopedNSAutoreleasePool scoped_pool;

  TestShellPlatformDelegate::PreflightArgs(&argc, &argv);
  return TestShellTestSuite(argc, argv).Run();
}
