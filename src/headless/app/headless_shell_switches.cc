// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/app/headless_shell_switches.h"

namespace headless {
namespace switches {

// The background color to be used if the page doesn't specify one. Provided as
// RGBA integer value in hex, e.g. 'ff0000ff' for red or '00000000' for
// transparent.
const char kDefaultBackgroundColor[] = "default-background-color";

// Whether cookies stored as part of user profile are encrypted.
const char kDisableCookieEncryption[] = "disable-cookie-encryption";

// Whether or not begin frames should be issued over DevToolsProtocol
// (experimental).
const char kEnableBeginFrameControl[] = "enable-begin-frame-control";

// Enable crash reporter for headless.
const char kEnableCrashReporter[] = "enable-crash-reporter";

// If enabled, generate a tagged (accessible) file when printing to PDF.
// The plan is for this to go away once tagged PDFs become the default.
// See https://crbug.com/607777
const char kExportTaggedPDF[] = "export-tagged-pdf";

// Disable crash reporter for headless. It is enabled by default in official
// builds.
const char kDisableCrashReporter[] = "disable-crash-reporter";

// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// A meta flag. This sets a number of flags which put the browser into
// deterministic mode where begin frames should be issued over DevToolsProtocol
// (experimental).
const char kDeterministicMode[] = "deterministic-mode";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
const char kDiskCacheDir[] = "disk-cache-dir";

// Instructs headless_shell to print document.body.innerHTML to stdout.
const char kDumpDom[] = "dump-dom";

// Hide scrollbars from screenshots.
const char kHideScrollbars[] = "hide-scrollbars";

// Specifies which encryption storage backend to use. Possible values are
// kwallet, kwallet5, gnome, gnome-keyring, gnome-libsecret, basic. Any other
// value will lead to Chrome detecting the best backend automatically.
// TODO(crbug.com/571003): Once PasswordStore no longer uses the Keyring or
// KWallet for storing passwords, rename this flag to stop referencing
// passwords. Do not rename it sooner, though; developers and testers might
// rely on it keeping large amounts of testing passwords out of their Keyrings
// or KWallets.
const char kPasswordStore[] = "password-store";

// Save a pdf file of the loaded page.
const char kPrintToPDF[] = "print-to-pdf";

// Do not display header and footer in the pdf file.
const char kPrintToPDFNoHeader[] = "print-to-pdf-no-header";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored unless --proxy-server is also specified. This is a
// comma-separated list of bypass rules. See:
// "net/proxy_resolution/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[] = "proxy-bypass-list";

// Uses a specified proxy server, overrides system settings. This switch only
// affects HTTP and HTTPS requests.
const char kProxyServer[] = "proxy-server";

// Use the given address instead of the default loopback for accepting remote
// debugging connections. Should be used together with --remote-debugging-port.
// Note that the remote debugging protocol does not perform any authentication,
// so exposing it too widely can be a security risk.
const char kRemoteDebuggingAddress[] = "remote-debugging-address";

// Runs a read-eval-print loop that allows the user to evaluate Javascript
// expressions.
const char kRepl[] = "repl";

// Save a screenshot of the loaded page.
const char kScreenshot[] = "screenshot";

// Causes SSL key material to be logged to the specified file for debugging
// purposes. See
// https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS/Key_Log_Format
// for the format.
const char kSSLKeyLogFile[] = "ssl-key-log-file";

// Issues a stop after the specified number of milliseconds.  This cancels all
// navigation and causes the DOMContentLoaded event to fire.
const char kTimeout[] = "timeout";

// Sets the GL implementation to use. Use a blank string to disable GL
// rendering.
const char kUseGL[] = "use-gl";

// A string used to override the default user agent with a custom one.
const char kUserAgent[] = "user-agent";

// Directory where the browser stores the user profile.
const char kUserDataDir[] = "user-data-dir";

// If set the system waits the specified number of virtual milliseconds before
// deeming the page to be ready.  For determinism virtual time does not advance
// while there are pending network fetches (i.e no timers will fire). Once all
// network fetches have completed, timers fire and if the system runs out of
// virtual time is fastforwarded so the next timer fires immediatley, until the
// specified virtual time budget is exhausted.
const char kVirtualTimeBudget[] = "virtual-time-budget";

// Sets the initial window size. Provided as string in the format "800,600".
const char kWindowSize[] = "window-size";

// Whitelist for Negotitate Auth servers.
const char kAuthServerWhitelist[] = "auth-server-whitelist";

// Sets font render hinting when running headless, affects Skia rendering and
// whether glyph subpixel positioning is enabled.
// Possible values: none|slight|medium|full|max. Default: full.
const char kFontRenderHinting[] = "font-render-hinting";

// If true, then all pop-ups and calls to window.open will fail.
const char kBlockNewWebContents[] = "block-new-web-contents";
}  // namespace switches
}  // namespace headless
