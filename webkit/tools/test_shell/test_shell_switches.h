// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_SWITCHES_H__
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_SWITCHES_H__

namespace test_shell {

extern const char kCrashDumps[];
extern const char kDumpPixels[];
extern const char kLayoutTests[];
extern const char kGenericTheme[];
extern const char kClassicTheme[];
extern const char kUxTheme[];
extern const char kNoErrorDialogs[];
extern const char kNoTree[];
extern const char kTestShellTimeOut[];
extern const char kStartupDialog[];
extern const char kGPFaultErrorBox[];
extern const char kJavaScriptFlags[];
extern const char kRecordMode[];
extern const char kPlaybackMode[];
extern const char kNoEvents[];
extern const char kDumpStatsTable[];
extern const char kCacheDir[];
extern const char kDebugMemoryInUse[];
extern const char kEnableFileCookies[];
extern const char kEnableTracing[];
extern const char kAllowScriptsToCloseWindows[];
extern const char kCheckLayoutTestSystemDeps[];
extern const char kGDB[];
extern const char kProfiler[];
extern const char kHeapProfiler[];

}  // namespace test_shell

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_SWITCHES_H__
