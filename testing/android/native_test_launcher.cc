// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class sets up the environment for running the native tests inside an
// android application. It outputs (to a fifo) markers identifying the
// START/PASSED/CRASH of the test suite, FAILURE/SUCCESS of individual tests,
// etc.
// These markers are read by the test runner script to generate test results.
// It installs signal handlers to detect crashes.

#include <android/log.h>
#include <signal.h>

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "gtest/gtest.h"
#include "testing/android/native_test_util.h"
#include "testing/jni/ChromeNativeTestActivity_jni.h"

using testing::native_test_util::ArgsToArgv;
using testing::native_test_util::CreateFIFO;
using testing::native_test_util::ParseArgsFromCommandLineFile;
using testing::native_test_util::RedirectStream;
using testing::native_test_util::ScopedMainEntryLogger;

// The main function of the program to be wrapped as a test apk.
extern int main(int argc, char** argv);

namespace {

// These two command line flags are supported for DumpRenderTree, which needs
// three fifos rather than a combined one: one for stderr, stdin and stdout.
const char kSeparateStderrFifo[] = "separate-stderr-fifo";
const char kCreateStdinFifo[] = "create-stdin-fifo";

// The test runner script writes the command line file in
// "/data/local/tmp".
static const char kCommandLineFilePath[] =
    "/data/local/tmp/chrome-native-tests-command-line";

const char kLogTag[] = "chromium";
const char kCrashedMarker[] = "[ CRASHED      ]\n";

// The list of signals which are considered to be crashes.
const int kExceptionSignals[] = {
  SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, -1
};

struct sigaction g_old_sa[NSIG];

// This function runs in a compromised context. It should not allocate memory.
void SignalHandler(int sig, siginfo_t* info, void* reserved) {
  // Output the crash marker.
  write(STDOUT_FILENO, kCrashedMarker, sizeof(kCrashedMarker));
  g_old_sa[sig].sa_sigaction(sig, info, reserved);
}

// TODO(nileshagrawal): now that we're using FIFO, test scripts can detect EOF.
// Remove the signal handlers.
void InstallHandlers() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  sa.sa_sigaction = SignalHandler;
  sa.sa_flags = SA_SIGINFO;

  for (unsigned int i = 0; kExceptionSignals[i] != -1; ++i) {
    sigaction(kExceptionSignals[i], &sa, &g_old_sa[kExceptionSignals[i]]);
  }
}

}  // namespace

// This method is called on a separate java thread so that we won't trigger
// an ANR.
static void RunTests(JNIEnv* env,
                     jobject obj,
                     jstring jfiles_dir,
                     jobject app_context) {
  base::AtExitManager exit_manager;

  // Command line initialized basically, will be fully initialized later.
  static const char* const kInitialArgv[] = { "ChromeTestActivity" };
  CommandLine::Init(arraysize(kInitialArgv), kInitialArgv);

  // Set the application context in base.
  base::android::ScopedJavaLocalRef<jobject> scoped_context(
      env, env->NewLocalRef(app_context));
  base::android::InitApplicationContext(scoped_context);
  base::android::RegisterJni(env);

  std::vector<std::string> args;
  ParseArgsFromCommandLineFile(kCommandLineFilePath, &args);

  std::vector<char*> argv;
  int argc = ArgsToArgv(args, &argv);

  // Fully initialize command line with arguments.
  CommandLine::ForCurrentProcess()->AppendArguments(
      CommandLine(argc, &argv[0]), false);
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  base::FilePath files_dir(
      base::android::ConvertJavaStringToUTF8(env, jfiles_dir));

  // A few options, such "--gtest_list_tests", will just use printf directly
  // Always redirect stdout to a known file.
  base::FilePath fifo_path(files_dir.Append(base::FilePath("test.fifo")));
  CreateFIFO(fifo_path.value().c_str());

  base::FilePath stderr_fifo_path, stdin_fifo_path;

  // DumpRenderTree needs a separate fifo for the stderr output. For all
  // other tests, insert stderr content to the same fifo we use for stdout.
  if (command_line.HasSwitch(kSeparateStderrFifo)) {
    stderr_fifo_path = files_dir.Append(base::FilePath("stderr.fifo"));
    CreateFIFO(stderr_fifo_path.value().c_str());
  }

  // DumpRenderTree uses stdin to receive input about which test to run.
  if (command_line.HasSwitch(kCreateStdinFifo)) {
    stdin_fifo_path = files_dir.Append(base::FilePath("stdin.fifo"));
    CreateFIFO(stdin_fifo_path.value().c_str());
  }

  // Only redirect the streams after all fifos have been created.
  RedirectStream(stdout, fifo_path.value().c_str(), "w");
  if (!stdin_fifo_path.empty())
    RedirectStream(stdin, stdin_fifo_path.value().c_str(), "r");
  if (!stderr_fifo_path.empty())
    RedirectStream(stderr, stderr_fifo_path.value().c_str(), "w");
  else
    dup2(STDOUT_FILENO, STDERR_FILENO);

  if (command_line.HasSwitch(switches::kWaitForDebugger)) {
    std::string msg = base::StringPrintf("Native test waiting for GDB because "
                                         "flag %s was supplied",
                                         switches::kWaitForDebugger);
    __android_log_write(ANDROID_LOG_VERBOSE, kLogTag, msg.c_str());
    base::debug::WaitForDebugger(24 * 60 * 60, false);
  }

  ScopedMainEntryLogger scoped_main_entry_logger;
  main(argc, &argv[0]);
}

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  // Install signal handlers to detect crashes.
  InstallHandlers();

  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!RegisterNativesImpl(env)) {
    return -1;
  }

  return JNI_VERSION_1_4;
}
