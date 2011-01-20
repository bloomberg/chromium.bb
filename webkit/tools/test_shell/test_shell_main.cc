// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/app_switches.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/environment.h"
#include "base/event_recorder.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/memory_debug.h"
#include "base/message_loop.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "gfx/gfx_module.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_module.h"
#include "net/base/net_util.h"
#include "net/http/http_cache.h"
#include "net/http/http_util.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/extensions/v8/gc_extension.h"
#include "webkit/extensions/v8/heap_profiler_extension.h"
#include "webkit/extensions/v8/playback_extension.h"
#include "webkit/extensions/v8/profiler_extension.h"
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
  base::EnableInProcessStackDumping();
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
       parsed_command_line.HasSwitch(test_shell::kNoErrorDialogs) ||
       parsed_command_line.HasSwitch(test_shell::kLayoutTests));
  bool layout_test_mode =
      parsed_command_line.HasSwitch(test_shell::kLayoutTests);
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
  bool generic_theme = (layout_test_mode && !ux_theme && !classic_theme) ||
      parsed_command_line.HasSwitch(test_shell::kGenericTheme);
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

  if (parsed_command_line.HasSwitch(test_shell::kEnableTracing))
    base::debug::TraceLog::StartTracing();

  net::HttpCache::Mode cache_mode = net::HttpCache::NORMAL;

  // This is a special mode where JS helps the browser implement
  // playback/record mode.  Generally, in this mode, some functions
  // of client-side randomness are removed.  For example, in
  // this mode Math.random() and Date.getTime() may not return
  // values which vary.
  bool playback_mode =
    parsed_command_line.HasSwitch(test_shell::kPlaybackMode);
  bool record_mode =
    parsed_command_line.HasSwitch(test_shell::kRecordMode);

  if (playback_mode)
    cache_mode = net::HttpCache::PLAYBACK;
  else if (record_mode)
    cache_mode = net::HttpCache::RECORD;

  if (layout_test_mode ||
      parsed_command_line.HasSwitch(test_shell::kEnableFileCookies))
    net::CookieMonster::EnableFileScheme();

  FilePath cache_path =
      parsed_command_line.GetSwitchValuePath(test_shell::kCacheDir);
  // If the cache_path is empty and it's layout_test_mode, leave it empty
  // so we use an in-memory cache. This makes running multiple test_shells
  // in parallel less flaky.
  if (cache_path.empty() && !layout_test_mode) {
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
  gfx::GfxModule::SetResourceProvider(TestShell::ResourceProvider);

  platform.InitializeGUI();

  TestShell::InitializeTestShell(layout_test_mode, allow_external_pages);

  if (parsed_command_line.HasSwitch(test_shell::kAllowScriptsToCloseWindows))
    TestShell::SetAllowScriptsToCloseWindows();

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

  // Default to a homepage if we're interactive.
  if (!layout_test_mode) {
    FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("webkit");
    path = path.AppendASCII("data");
    path = path.AppendASCII("test_shell");
    path = path.AppendASCII("index.html");
    starting_url = net::FilePathToFileURL(path);
  }

  const std::vector<CommandLine::StringType>& args = parsed_command_line.args();
  if (args.size() > 0) {
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

  // Expose GCController to JavaScript.
  WebScriptController::registerExtension(extensions_v8::GCExtension::Get());

  if (parsed_command_line.HasSwitch(test_shell::kProfiler)) {
    WebScriptController::registerExtension(
        extensions_v8::ProfilerExtension::Get());
  }

  if (parsed_command_line.HasSwitch(test_shell::kHeapProfiler)) {
    WebScriptController::registerExtension(
        extensions_v8::HeapProfilerExtension::Get());
  }

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
    if (record_mode || playback_mode) {
      platform.SetWindowPositionForRecording(shell);
      WebScriptController::registerExtension(
          extensions_v8::PlaybackExtension::Get());
    }

    shell->Show(WebKit::WebNavigationPolicyNewWindow);

    if (parsed_command_line.HasSwitch(test_shell::kDumpStatsTable))
      shell->DumpStatsTableOnExit();

    bool no_events = parsed_command_line.HasSwitch(test_shell::kNoEvents);
    if ((record_mode || playback_mode) && !no_events) {
      FilePath script_path = cache_path;
      // Create the cache directory in case it doesn't exist.
      file_util::CreateDirectory(cache_path);
      script_path = script_path.AppendASCII("script.log");
      if (record_mode)
        base::EventRecorder::current()->StartRecording(script_path);
      if (playback_mode)
        base::EventRecorder::current()->StartPlayback(script_path);
    }

    if (parsed_command_line.HasSwitch(test_shell::kDebugMemoryInUse)) {
      base::MemoryDebug::SetMemoryInUseEnabled(true);
      // Dump all in use memory at startup
      base::MemoryDebug::DumpAllMemoryInUse();
    }

    // See if we need to run the tests.
    if (layout_test_mode) {
      // Set up for the kind of test requested.
      TestShell::TestParams params;
      if (parsed_command_line.HasSwitch(test_shell::kDumpPixels)) {
        // The pixel test flag also gives the image file name to use.
        params.dump_pixels = true;
        params.pixel_file_name = parsed_command_line.GetSwitchValuePath(
            test_shell::kDumpPixels);
        if (params.pixel_file_name.empty()) {
          fprintf(stderr, "No file specified for pixel tests");
          exit(1);
        }
      }
      if (parsed_command_line.HasSwitch(test_shell::kNoTree)) {
          params.dump_tree = false;
      }

      if (!starting_url.is_valid()) {
        // Watch stdin for URLs.
        char filenameBuffer[kPathBufSize];
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
          // When running layout tests we pass new line separated
          // tests to TestShell. Each line is a space separated list
          // of filename, timeout and expected pixel hash. The timeout
          // and the pixel hash are optional.
          char* newLine = strchr(filenameBuffer, '\n');
          if (newLine)
            *newLine = '\0';
          if (!*filenameBuffer)
            continue;

          params.test_url = strtok(filenameBuffer, " ");

          // Set the current path to the directory that contains the test
          // files. This is because certain test file may use the relative
          // path.
          GURL test_url(params.test_url);
          FilePath test_file_path;
          net::FileURLToFilePath(test_url, &test_file_path);
          file_util::SetCurrentDirectory(test_file_path.DirName());

          int old_timeout_ms = TestShell::GetLayoutTestTimeout();

          char* timeout = strtok(NULL, " ");
          if (timeout) {
            TestShell::SetFileTestTimeout(atoi(timeout));
            char* pixel_hash = strtok(NULL, " ");
            if (pixel_hash)
              params.pixel_hash = pixel_hash;
          }

          // Load the page the number of times specified.
          bool fatal_error = false;
          for (int i = 0; i < TestShell::GetLoadCount(); i++) {
            // Set the JavaScript flags specified for this load.
            webkit_glue::SetJavaScriptFlags(TestShell::GetJSFlagsForLoad(i));

            // Only dump for the last load.
            bool is_last_load = (i == (TestShell::GetLoadCount() - 1));
            TestShell::SetDumpWhenFinished(is_last_load);

            if (!TestShell::RunFileTest(params)) {
              fatal_error = true;
              break;
            }
          }

          if (fatal_error)
            break;

          TestShell::SetFileTestTimeout(old_timeout_ms);
        }
      } else {
        // TODO(ojan): Provide a way for run-singly tests to pass
        // in a hash and then set params.pixel_hash here.
        params.test_url = starting_url.spec();
        TestShell::RunFileTest(params);
      }

      shell->CallJSGC();
      shell->CallJSGC();

      // When we finish the last test, cleanup the LayoutTestController.
      // It may have references to not-yet-cleaned up windows.  By
      // cleaning up here we help purify reports.
      shell->ResetTestController();

      // Flush any remaining messages before we kill ourselves.
      // http://code.google.com/p/chromium/issues/detail?id=9500
      MessageLoop::current()->RunAllPending();
    } else {
      webkit_glue::SetJavaScriptFlags(TestShell::GetJSFlagsForLoad(0));
      MessageLoop::current()->Run();
    }

    if (record_mode)
      base::EventRecorder::current()->StopRecording();
    if (playback_mode)
      base::EventRecorder::current()->StopPlayback();
  }

  TestShell::ShutdownTestShell();
  TestShell::CleanupLogging();

  // Tear down shared StatsTable; prevents unit_tests from leaking it.
  base::StatsTable::set_current(NULL);
  delete table;
  RemoveSharedMemoryFile(stats_filename);

  return 0;
}
