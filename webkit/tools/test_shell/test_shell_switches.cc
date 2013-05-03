// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_switches.h"

namespace test_shell {

// Suppresses all error dialogs when present.
const char kNoErrorDialogs[]                = "noerrdialogs";

const char kCrashDumps[] = "crash-dumps";  // Enable crash dumps
// Dumps the full-heap instead of only stack. Used with kCrashDumps.
const char kCrashDumpsFulldump[] = "crash-dumps-fulldump";

// Causes the test_shell to run with a generic theme (part of layout_tests).
const char kGenericTheme[] = "generic-theme";

// This causes the test_shell to run with the classic theme.
// Passing --layout-tests enables this by default.
const char kClassicTheme[] = "classic-theme";

// This causes the test_shell to run with the new windows theming engine
// enabled. This is the default unless --layout-tests is specified.
const char kUxTheme[] = "ux-theme";

const char kStartupDialog[] = "testshell-startup-dialog";

// Enable the Windows dialogs for GP faults in the test shell. This allows makes
// it possible to attach a crashed test shell to a debugger.
const char kGPFaultErrorBox[] = "gp-fault-error-box";

// JavaScript flags passed to engine. If multiple loads has been specified this
// can be a list separated by commas. Each set of flags are passed to the engine
// in the corresponding load.
const char kJavaScriptFlags[] = "js-flags";

// Dump stats table on exit.
const char kDumpStatsTable[] = "stats";

// Use a specified cache directory.
const char kCacheDir[] = "cache-dir";

// Enable cookies on the file:// scheme.  --layout-tests also enables this.
const char kEnableFileCookies[] = "enable-file-cookies";

// Allow scripts to close windows in all cases.
const char kAllowScriptsToCloseWindows[] = "allow-scripts-to-close-windows";

// Test the system dependencies (themes, fonts, ...). When this flag is
// specified, the test shell will exit immediately with either 0 (success) or
// 1 (failure). Combining with other flags has no effect.
const char kCheckLayoutTestSystemDeps[] = "check-layout-test-sys-deps";

// If set, we are running under GDB so allow a certain class of errors
// to happen even if in layout test mode.
const char kGDB[] = "gdb";

const char kAllowExternalPages[] = "allow-external-pages";

const char kEnableAccel2DCanvas[] = "enable-accelerated-2d-canvas";

const char kEnableAccelCompositing[] = "enable-accelerated-compositing";

const char kEnableSmoothScrolling[] = "enable-smooth-scrolling";

}  // namespace test_shell
