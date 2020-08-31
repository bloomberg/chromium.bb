// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_APP_HEADLESS_SHELL_SWITCHES_H_
#define HEADLESS_APP_HEADLESS_SHELL_SWITCHES_H_

#include "content/public/common/content_switches.h"
#include "headless/public/headless_export.h"

namespace headless {
namespace switches {

HEADLESS_EXPORT extern const char kCrashDumpsDir[];
HEADLESS_EXPORT extern const char kDefaultBackgroundColor[];
HEADLESS_EXPORT extern const char kDeterministicMode[];
HEADLESS_EXPORT extern const char kDisableCookieEncryption[];
HEADLESS_EXPORT extern const char kDisableCrashReporter[];
HEADLESS_EXPORT extern const char kDiskCacheDir[];
HEADLESS_EXPORT extern const char kDumpDom[];
HEADLESS_EXPORT extern const char kEnableBeginFrameControl[];
HEADLESS_EXPORT extern const char kEnableCrashReporter[];
HEADLESS_EXPORT extern const char kExportTaggedPDF[];
HEADLESS_EXPORT extern const char kHideScrollbars[];
HEADLESS_EXPORT extern const char kPasswordStore[];
HEADLESS_EXPORT extern const char kPrintToPDF[];
HEADLESS_EXPORT extern const char kPrintToPDFNoHeader[];
HEADLESS_EXPORT extern const char kProxyBypassList[];
HEADLESS_EXPORT extern const char kProxyServer[];
HEADLESS_EXPORT extern const char kRemoteDebuggingAddress[];
HEADLESS_EXPORT extern const char kRepl[];
HEADLESS_EXPORT extern const char kScreenshot[];
HEADLESS_EXPORT extern const char kSSLKeyLogFile[];
HEADLESS_EXPORT extern const char kTimeout[];
HEADLESS_EXPORT extern const char kUseGL[];
HEADLESS_EXPORT extern const char kUserAgent[];
HEADLESS_EXPORT extern const char kUserDataDir[];
HEADLESS_EXPORT extern const char kVirtualTimeBudget[];
HEADLESS_EXPORT extern const char kWindowSize[];
HEADLESS_EXPORT extern const char kAuthServerWhitelist[];
HEADLESS_EXPORT extern const char kFontRenderHinting[];
HEADLESS_EXPORT extern const char kBlockNewWebContents[];

// Switches which are replicated from content.
using ::switches::kRemoteDebuggingPort;
using ::switches::kRemoteDebuggingPipe;

}  // namespace switches
}  // namespace headless

#endif  // HEADLESS_APP_HEADLESS_SHELL_SWITCHES_H_
