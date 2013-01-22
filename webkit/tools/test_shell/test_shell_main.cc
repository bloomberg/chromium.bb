// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/debug/trace_event.h"
#include "base/environment.h"
#include "base/event_recorder.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_module.h"
#include "net/base/net_util.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/http/http_util.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_platform_delegate.h"
#include "webkit/tools/test_shell/test_shell_request_context.h"
#include "webkit/tools/test_shell/test_shell_switches.h"
#include "webkit/tools/test_shell/test_shell_webkit_init.h"

#if defined(OS_WIN)
#pragma warning(disable: 4996)
#endif

static const size_t kPathBufSize = 2048;

using WebKit::WebScriptController;

namespace {

// StatsTable initialization parameters.
const char* const kStatsFilePrefix = "testshell_";
int kStatsFileThreads = 20;
int kStatsFileCounters = 200;

void RemoveSharedMemoryFile(std::string& filename) {
  // Stats uses SharedMemory under the hood. On posix, this results in a file
  // on disk.
#if defined(OS_POSIX)
  base::SharedMemory memory;
  memory.Delete(filename);
#endif
}

}  // namespace

int main(int argc, char* argv[]) {
  base::debug::EnableInProcessStackDumping();
  base::EnableTerminationOnHeapCorruption();

  // Some tests may use base::Singleton<>, thus we need to instanciate
  // the AtExitManager or else we will leak objects.
  base::AtExitManager at_exit_manager;

  TestShellPlatformDelegate::PreflightArgs(&argc, &argv);
  CommandLine::Init(argc, argv);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

  TestShellPlatformDelegate platform(parsed_command_line);

  if (parsed_command_line.HasSwitch(test_shell::kStartupDialog))
    TestShell::ShowStartupDebuggingDialog();

  if (parsed_command_line.HasSwitch(test_shell::kCheckLayoutTestSystemDeps)) {
    exit(platform.CheckLayoutTestSystemDependencies() ? 0 : 1);
  }

  // Allocate a message loop for this thread.  Although it is not used
  // directly, its constructor sets up some necessary state.
  MessageLoopForUI main_message_loop;

  scoped_ptr<base::Environment> env(base::Environment::Create());
  bool suppress_error_dialogs = (
       env->HasVar("CHROME_HEADLESS") ||
       parsed_command_line.HasSwitch(test_shell::kNoErrorDialogs));
  bool ux_theme = parsed_command_line.HasSwitch(test_shell::kUxTheme);
#if defined(OS_MACOSX)
  // The "classic theme" flag is meaningless on OS X.  But there is a bunch
  // of code that sets up the environment for running pixel tests that only
  // runs if it's set to true.
  bool classic_theme = true;
#else
  bool classic_theme =
      parsed_command_line.HasSwitch(test_shell::kClassicTheme);
#endif  // !OS_MACOSX
#if defined(OS_WIN)
  bool generic_theme = parsed_command_line.HasSwitch(test_shell::kGenericTheme);
#else
  // Stop compiler warnings about unused variables.
  static_cast<void>(ux_theme);
#endif

  bool enable_gp_fault_error_box = false;
  enable_gp_fault_error_box =
      parsed_command_line.HasSwitch(test_shell::kGPFaultErrorBox);

  bool allow_external_pages =
      parsed_command_line.HasSwitch(test_shell::kAllowExternalPages);

  if (parsed_command_line.HasSwitch(test_shell::kEnableAccel2DCanvas))
    TestShell::SetAccelerated2dCanvasEnabled(true);
  if (parsed_command_line.HasSwitch(test_shell::kEnableAccelCompositing))
    TestShell::SetAcceleratedCompositingEnabled(true);

  if (parsed_command_line.HasSwitch(test_shell::kMultipleLoads)) {
    const std::string multiple_loads_str =
        parsed_command_line.GetSwitchValueASCII(test_shell::kMultipleLoads);
    int load_count;
    base::StringToInt(multiple_loads_str, &load_count);
    if (load_count <= 0) {
  #ifndef NDEBUG
      load_count = 2;
  #else
      load_count = 5;
  #endif
    }
    TestShell::SetMultipleLoad(load_count);
  }

  bool layout_test_mode = false;
  TestShell::InitLogging(suppress_error_dialogs,
                         layout_test_mode,
                         enable_gp_fault_error_box);

  // Initialize WebKit for this scope.
  TestShellWebKitInit test_shell_webkit_init(layout_test_mode);

  // Suppress abort message in v8 library in debugging mode (but not
  // actually under a debugger).  V8 calls abort() when it hits
  // assertion errors.
  if (suppress_error_dialogs) {
    platform.SuppressErrorReporting();
  }

  net::HttpCache::Mode cache_mode = net::HttpCache::NORMAL;

  if (parsed_command_line.HasSwitch(test_shell::kEnableFileCookies))
    net::CookieMonster::EnableFileScheme();

  FilePath cache_path =
      parsed_command_line.GetSwitchValuePath(test_shell::kCacheDir);
  if (cache_path.empty()) {
    PathService::Get(base::DIR_EXE, &cache_path);
    cache_path = cache_path.AppendASCII("cache");
  }

  // Initializing with a default context, which means no on-disk cookie DB,
  // and no support for directory listings.
  SimpleResourceLoaderBridge::Init(cache_path, cache_mode, layout_test_mode);

  // Load ICU data tables
  icu_util::Initialize();

  // Config the modules that need access to a limited set of resources.
  net::NetModule::SetResourceProvider(TestShell::ResourceProvider);

  platform.InitializeGUI();

  TestShell::InitializeTestShell(layout_test_mode, allow_external_pages);

  if (parsed_command_line.HasSwitch(test_shell::kAllowScriptsToCloseWindows))
    TestShell::SetAllowScriptsToCloseWindows();

  if (parsed_command_line.HasSwitch(test_shell::kEnableSmoothScrolling))
    TestShell::GetWebPreferences()->enable_scroll_animator = true;

  // Disable user themes for layout tests so pixel tests are consistent.
#if defined(OS_WIN)
  TestShellWebTheme::Engine engine;
#endif
  if (classic_theme)
    platform.SelectUnifiedTheme();
#if defined(OS_WIN)
  if (generic_theme)
    test_shell_webkit_init.SetThemeEngine(&engine);
#endif

  if (parsed_command_line.HasSwitch(test_shell::kTestShellTimeOut)) {
    const std::string timeout_str = parsed_command_line.GetSwitchValueASCII(
        test_shell::kTestShellTimeOut);
    int timeout_ms;
    base::StringToInt(timeout_str, &timeout_ms);
    if (timeout_ms > 0)
      TestShell::SetFileTestTimeout(timeout_ms);
  }

  // Unless specifically requested otherwise, default to OSMesa for GL.
  if (!parsed_command_line.HasSwitch(switches::kUseGL))
    gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL);

  // Treat the first argument as the initial URL to open.
  GURL starting_url;

  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("webkit").AppendASCII("data")
             .AppendASCII("test_shell").AppendASCII("index.html");
  starting_url = net::FilePathToFileURL(path);

  const CommandLine::StringVector& args = parsed_command_line.GetArgs();
  if (!args.empty()) {
    GURL url(args[0]);
    if (url.is_valid()) {
      starting_url = url;
    } else {
      // Treat as a relative file path.
      FilePath path = FilePath(args[0]);
      file_util::AbsolutePath(&path);
      starting_url = net::FilePathToFileURL(path);
    }
  }

  // Get the JavaScript flags. The test runner might send a quoted string which
  // needs to be unquoted before further processing.
  std::string js_flags =
      parsed_command_line.GetSwitchValueASCII(test_shell::kJavaScriptFlags);
  js_flags = net::HttpUtil::Unquote(js_flags);
  // Split the JavaScript flags into a list.
  std::vector<std::string> js_flags_list;
  size_t start = 0;
  while (true) {
    size_t comma_pos = js_flags.find_first_of(',', start);
    std::string flags;
    if (comma_pos == std::string::npos) {
      flags = js_flags.substr(start, js_flags.length() - start);
    } else {
      flags = js_flags.substr(start, comma_pos - start);
      start = comma_pos + 1;
    }
    js_flags_list.push_back(flags);
    if (comma_pos == std::string::npos)
      break;
  }
  TestShell::SetJavaScriptFlags(js_flags_list);

  // Test shell always exposes the GC.
  webkit_glue::SetJavaScriptFlags("--expose-gc");

  // Load and initialize the stats table.  Attempt to construct a somewhat
  // unique name to isolate separate instances from each other.

  // truncate the random # to 32 bits for the benefit of Mac OS X, to
  // avoid tripping over its maximum shared memory segment name length
  std::string stats_filename = kStatsFilePrefix +
      base::Uint64ToString(base::RandUint64() & 0xFFFFFFFFL);
  RemoveSharedMemoryFile(stats_filename);
  base::StatsTable *table = new base::StatsTable(stats_filename,
      kStatsFileThreads,
      kStatsFileCounters);
  base::StatsTable::set_current(table);

  TestShell* shell;
  if (TestShell::CreateNewWindow(starting_url, &shell)) {
    shell->Show(WebKit::WebNavigationPolicyNewWindow);

    if (parsed_command_line.HasSwitch(test_shell::kDumpStatsTable))
      shell->DumpStatsTableOnExit();

    webkit_glue::SetJavaScriptFlags(TestShell::GetJSFlagsForLoad(0));
    MessageLoop::current()->Run();
  }

  TestShell::ShutdownTestShell();
  TestShell::CleanupLogging();

  // Tear down shared StatsTable; prevents unit_tests from leaking it.
  base::StatsTable::set_current(NULL);
  delete table;
  RemoveSharedMemoryFile(stats_filename);

  return 0;
}
