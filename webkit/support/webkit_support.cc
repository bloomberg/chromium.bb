// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/support/webkit_support.h"

#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "url/url_util.h"
#include "webkit/child/webkitplatformsupport_impl.h"
#include "webkit/common/user_agent/user_agent.h"
#include "webkit/common/user_agent/user_agent_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/support/platform_support.h"
#include "webkit/support/test_webkit_platform_support.h"

#if defined(OS_ANDROID)
#include "base/test/test_support_android.h"
#endif

namespace {

// All fatal log messages (e.g. DCHECK failures) imply unit test failures
void UnitTestAssertHandler(const std::string& str) {
  FAIL() << str;
}

void InitLogging() {
#if defined(OS_WIN)
  if (!::IsDebuggerPresent()) {
    UINT new_flags = SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX
        | SEM_NOGPFAULTERRORBOX;

    // Preserve existing error mode, as discussed at
    // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
    UINT existing_flags = SetErrorMode(new_flags);
    SetErrorMode(existing_flags | new_flags);

    // Don't pop up dialog on assertion failure, log to stdout instead.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
  }
#endif

#if defined(OS_ANDROID)
  // On Android we expect the log to appear in logcat.
  base::InitAndroidTestLogging();
#else
  base::FilePath log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  log_filename = log_filename.AppendASCII("DumpRenderTree.log");
  logging::LoggingSettings settings;
  // Only log to a file. This prevents debugging output from disrupting
  // whether or not we pass.
  settings.logging_dest = logging::LOG_TO_FILE;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);

  // We want process and thread IDs because we may have multiple processes.
  const bool kProcessId = true;
  const bool kThreadId = true;
  const bool kTimestamp = true;
  const bool kTickcount = true;
  logging::SetLogItems(kProcessId, kThreadId, !kTimestamp, kTickcount);
#endif  // else defined(OS_ANDROID)
}

class TestEnvironment {
 public:
#if defined(OS_ANDROID)
  // Android UI message loop goes through Java, so don't use it in tests.
  typedef base::MessageLoop MessageLoopType;
#else
  typedef base::MessageLoopForUI MessageLoopType;
#endif

  TestEnvironment() {
    logging::SetLogAssertHandler(UnitTestAssertHandler);
    main_message_loop_.reset(new MessageLoopType);

    // TestWebKitPlatformSupport must be instantiated after MessageLoopType.
    webkit_platform_support_.reset(new TestWebKitPlatformSupport);
  }

  TestWebKitPlatformSupport* webkit_platform_support() const {
    return webkit_platform_support_.get();
  }

 private:
  scoped_ptr<MessageLoopType> main_message_loop_;
  scoped_ptr<TestWebKitPlatformSupport> webkit_platform_support_;
};

TestEnvironment* test_environment;

}  // namespace

namespace webkit_support {

void SetUpTestEnvironmentForUnitTests() {
  base::debug::EnableInProcessStackDumping();
  base::EnableTerminationOnHeapCorruption();

  // Initialize the singleton CommandLine with fixed values.  Some code refer to
  // CommandLine::ForCurrentProcess().  We don't use the actual command-line
  // arguments of DRT to avoid unexpected behavior change.
  //
  // webkit/glue/plugin/plugin_list_posix.cc checks --debug-plugin-loading.
  // webkit/glue/plugin/plugin_list_win.cc checks --old-wmp.
  // If DRT needs these flags, specify them in the following kFixedArguments.
  const char* kFixedArguments[] = {"DumpRenderTree"};
  CommandLine::Init(arraysize(kFixedArguments), kFixedArguments);

  WebKit::WebRuntimeFeatures::enableStableFeatures(true);
  WebKit::WebRuntimeFeatures::enableExperimentalFeatures(true);
  WebKit::WebRuntimeFeatures::enableTestOnlyFeatures(true);

  // Explicitly initialize the GURL library before spawning any threads.
  // Otherwise crash may happend when different threads try to create a GURL
  // at same time.
  url_util::Initialize();
  webkit_support::BeforeInitialize();
  test_environment = new TestEnvironment;
  webkit_support::AfterInitialize();
  webkit_glue::SetUserAgent(webkit_glue::BuildUserAgentFromProduct(
      "DumpRenderTree/0.0.0.0"), false);
}

void TearDownTestEnvironment() {
  // Flush any remaining messages before we kill ourselves.
  // http://code.google.com/p/chromium/issues/detail?id=9500
  base::RunLoop().RunUntilIdle();

  BeforeShutdown();
  if (RunningOnValgrind())
    WebKit::WebCache::clear();
  WebKit::shutdown();
  delete test_environment;
  test_environment = NULL;
  AfterShutdown();
  logging::CloseLogFile();
}

}  // namespace webkit_support
