// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sanitizer_buildflags.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <signal.h>
#include <unistd.h>
#include "base/posix/eintr_wrapper.h"
#endif  // OS_POSIX

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include <ucontext.h>
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <excpt.h>
#endif  // OS_WIN

#if defined(OS_FUCHSIA)
#include <fuchsia/logger/cpp/fidl.h>
#include <fuchsia/logger/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/zx/channel.h>
#include <lib/zx/event.h>
#include <lib/zx/exception.h>
#include <lib/zx/process.h>
#include <lib/zx/thread.h>
#include <lib/zx/time.h>
#include <zircon/process.h>
#include <zircon/syscalls/debug.h>
#include <zircon/syscalls/exception.h>
#include <zircon/types.h>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#endif  // OS_FUCHSIA

namespace logging {

namespace {

using ::testing::Return;
using ::testing::_;

// Class to make sure any manipulations we do to the min log level are
// contained (i.e., do not affect other unit tests).
class LogStateSaver {
 public:
  LogStateSaver() : old_min_log_level_(GetMinLogLevel()) {}

  ~LogStateSaver() {
    SetMinLogLevel(old_min_log_level_);
  }

 private:
  int old_min_log_level_;

  DISALLOW_COPY_AND_ASSIGN(LogStateSaver);
};

class LoggingTest : public testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  LogStateSaver log_state_saver_;
};

class MockLogSource {
 public:
  MOCK_METHOD0(Log, const char*());
};

class MockLogAssertHandler {
 public:
  MOCK_METHOD4(
      HandleLogAssert,
      void(const char*, int, const base::StringPiece, const base::StringPiece));
};

TEST_F(LoggingTest, BasicLogging) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log())
      .Times(DCHECK_IS_ON() ? 16 : 8)
      .WillRepeatedly(Return("log message"));

  SetMinLogLevel(LOG_INFO);

  EXPECT_TRUE(LOG_IS_ON(INFO));
  EXPECT_EQ(DCHECK_IS_ON(), DLOG_IS_ON(INFO));
  EXPECT_TRUE(VLOG_IS_ON(0));

  LOG(INFO) << mock_log_source.Log();
  LOG_IF(INFO, true) << mock_log_source.Log();
  PLOG(INFO) << mock_log_source.Log();
  PLOG_IF(INFO, true) << mock_log_source.Log();
  VLOG(0) << mock_log_source.Log();
  VLOG_IF(0, true) << mock_log_source.Log();
  VPLOG(0) << mock_log_source.Log();
  VPLOG_IF(0, true) << mock_log_source.Log();

  DLOG(INFO) << mock_log_source.Log();
  DLOG_IF(INFO, true) << mock_log_source.Log();
  DPLOG(INFO) << mock_log_source.Log();
  DPLOG_IF(INFO, true) << mock_log_source.Log();
  DVLOG(0) << mock_log_source.Log();
  DVLOG_IF(0, true) << mock_log_source.Log();
  DVPLOG(0) << mock_log_source.Log();
  DVPLOG_IF(0, true) << mock_log_source.Log();
}

TEST_F(LoggingTest, LogIsOn) {
#if defined(NDEBUG)
  const bool kDfatalIsFatal = false;
#else  // defined(NDEBUG)
  const bool kDfatalIsFatal = true;
#endif  // defined(NDEBUG)

  SetMinLogLevel(LOG_INFO);
  EXPECT_TRUE(LOG_IS_ON(INFO));
  EXPECT_TRUE(LOG_IS_ON(WARNING));
  EXPECT_TRUE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOG_WARNING);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_TRUE(LOG_IS_ON(WARNING));
  EXPECT_TRUE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOG_ERROR);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(LOG_IS_ON(WARNING));
  EXPECT_TRUE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(LOG_IS_ON(DFATAL));

  // LOG_IS_ON(FATAL) should always be true.
  SetMinLogLevel(LOG_FATAL + 1);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(LOG_IS_ON(WARNING));
  EXPECT_FALSE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_EQ(kDfatalIsFatal, LOG_IS_ON(DFATAL));
}

TEST_F(LoggingTest, LoggingIsLazyBySeverity) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);

  SetMinLogLevel(LOG_WARNING);

  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(DLOG_IS_ON(INFO));
  EXPECT_FALSE(VLOG_IS_ON(1));

  LOG(INFO) << mock_log_source.Log();
  LOG_IF(INFO, false) << mock_log_source.Log();
  PLOG(INFO) << mock_log_source.Log();
  PLOG_IF(INFO, false) << mock_log_source.Log();
  VLOG(1) << mock_log_source.Log();
  VLOG_IF(1, true) << mock_log_source.Log();
  VPLOG(1) << mock_log_source.Log();
  VPLOG_IF(1, true) << mock_log_source.Log();

  DLOG(INFO) << mock_log_source.Log();
  DLOG_IF(INFO, true) << mock_log_source.Log();
  DPLOG(INFO) << mock_log_source.Log();
  DPLOG_IF(INFO, true) << mock_log_source.Log();
  DVLOG(1) << mock_log_source.Log();
  DVLOG_IF(1, true) << mock_log_source.Log();
  DVPLOG(1) << mock_log_source.Log();
  DVPLOG_IF(1, true) << mock_log_source.Log();
}

TEST_F(LoggingTest, LoggingIsLazyByDestination) {
  MockLogSource mock_log_source;
  MockLogSource mock_log_source_error;
  EXPECT_CALL(mock_log_source, Log()).Times(0);

  // Severity >= ERROR is always printed to stderr.
  EXPECT_CALL(mock_log_source_error, Log()).Times(1).
      WillRepeatedly(Return("log message"));

  LoggingSettings settings;
  settings.logging_dest = LOG_NONE;
  InitLogging(settings);

  LOG(INFO) << mock_log_source.Log();
  LOG(WARNING) << mock_log_source.Log();
  LOG(ERROR) << mock_log_source_error.Log();
}

// Check that logging to stderr is gated on LOG_TO_STDERR.
TEST_F(LoggingTest, LogToStdErrFlag) {
  LoggingSettings settings;
  settings.logging_dest = LOG_NONE;
  InitLogging(settings);
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);
  LOG(INFO) << mock_log_source.Log();

  settings.logging_dest = LOG_TO_STDERR;
  MockLogSource mock_log_source_stderr;
  InitLogging(settings);
  EXPECT_CALL(mock_log_source_stderr, Log()).Times(1).WillOnce(Return("foo"));
  LOG(INFO) << mock_log_source_stderr.Log();
}

// Check that messages with severity ERROR or higher are always logged to
// stderr if no log-destinations are set, other than LOG_TO_FILE.
// This test is currently only POSIX-compatible.
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
namespace {
void TestForLogToStderr(int log_destinations,
                        bool* did_log_info,
                        bool* did_log_error) {
  const char kInfoLogMessage[] = "This is an INFO level message";
  const char kErrorLogMessage[] = "Here we have a message of level ERROR";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Set up logging.
  LoggingSettings settings;
  settings.logging_dest = log_destinations;
  base::FilePath file_logs_path;
  if (log_destinations & LOG_TO_FILE) {
    file_logs_path = temp_dir.GetPath().Append("file.log");
    settings.log_file_path = file_logs_path.value().c_str();
  }
  InitLogging(settings);

  // Create a file and change stderr to write to that file, to easily check
  // contents.
  base::FilePath stderr_logs_path = temp_dir.GetPath().Append("stderr.log");
  base::File stderr_logs = base::File(
      stderr_logs_path,
      base::File::FLAG_CREATE | base::File::FLAG_WRITE | base::File::FLAG_READ);
  base::ScopedFD stderr_backup = base::ScopedFD(dup(STDERR_FILENO));
  int dup_result = dup2(stderr_logs.GetPlatformFile(), STDERR_FILENO);
  ASSERT_EQ(dup_result, STDERR_FILENO);

  LOG(INFO) << kInfoLogMessage;
  LOG(ERROR) << kErrorLogMessage;

  // Restore the original stderr logging destination.
  dup_result = dup2(stderr_backup.get(), STDERR_FILENO);
  ASSERT_EQ(dup_result, STDERR_FILENO);

  // Check which of the messages were written to stderr.
  std::string written_logs;
  ASSERT_TRUE(base::ReadFileToString(stderr_logs_path, &written_logs));
  *did_log_info = written_logs.find(kInfoLogMessage) != std::string::npos;
  *did_log_error = written_logs.find(kErrorLogMessage) != std::string::npos;
}
}  // namespace

TEST_F(LoggingTest, AlwaysLogErrorsToStderr) {
  bool did_log_info = false;
  bool did_log_error = false;

  // When no destinations are specified, ERRORs should still log to stderr.
  TestForLogToStderr(LOG_NONE, &did_log_info, &did_log_error);
  EXPECT_FALSE(did_log_info);
  EXPECT_TRUE(did_log_error);

  // Logging only to a file should also log ERRORs to stderr as well.
  TestForLogToStderr(LOG_TO_FILE, &did_log_info, &did_log_error);
  EXPECT_FALSE(did_log_info);
  EXPECT_TRUE(did_log_error);

  // ERRORs should not be logged to stderr if any destination besides FILE is
  // set.
  TestForLogToStderr(LOG_TO_SYSTEM_DEBUG_LOG, &did_log_info, &did_log_error);
  EXPECT_FALSE(did_log_info);
  EXPECT_FALSE(did_log_error);

  // Both ERRORs and INFO should be logged if LOG_TO_STDERR is set.
  TestForLogToStderr(LOG_TO_STDERR, &did_log_info, &did_log_error);
  EXPECT_TRUE(did_log_info);
  EXPECT_TRUE(did_log_error);
}
#endif

#if defined(OS_CHROMEOS)
TEST_F(LoggingTest, InitWithFileDescriptor) {
  const char kErrorLogMessage[] = "something bad happened";

  // Open a file to pass to the InitLogging.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_log_path = temp_dir.GetPath().Append("file.log");
  FILE* log_file = fopen(file_log_path.value().c_str(), "w");
  CHECK(log_file);

  // Set up logging.
  LoggingSettings settings;
  settings.logging_dest = LOG_TO_FILE;
  settings.log_file = log_file;
  InitLogging(settings);

  LOG(ERROR) << kErrorLogMessage;

  // Check the message was written to the log file.
  std::string written_logs;
  ASSERT_TRUE(base::ReadFileToString(file_log_path, &written_logs));
  ASSERT_NE(written_logs.find(kErrorLogMessage), std::string::npos);
}

TEST_F(LoggingTest, DuplicateLogFile) {
  const char kErrorLogMessage1[] = "something really bad happened";
  const char kErrorLogMessage2[] = "some other bad thing happened";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_log_path = temp_dir.GetPath().Append("file.log");

  // Set up logging.
  LoggingSettings settings;
  settings.logging_dest = LOG_TO_FILE;
  settings.log_file_path = file_log_path.value().c_str();
  InitLogging(settings);

  LOG(ERROR) << kErrorLogMessage1;

  // Duplicate the log FILE, close the original (to make sure we actually
  // duplicated it), and write to the duplicate.
  FILE* log_file_dup = DuplicateLogFILE();
  CHECK(log_file_dup);
  CloseLogFile();
  fprintf(log_file_dup, "%s\n", kErrorLogMessage2);
  fflush(log_file_dup);

  // Check the messages were written to the log file.
  std::string written_logs;
  ASSERT_TRUE(base::ReadFileToString(file_log_path, &written_logs));
  ASSERT_NE(written_logs.find(kErrorLogMessage1), std::string::npos);
  ASSERT_NE(written_logs.find(kErrorLogMessage2), std::string::npos);
  fclose(log_file_dup);
}
#endif  // defined(OS_CHROMEOS)

#if defined(OFFICIAL_BUILD) && defined(OS_WIN)
NOINLINE void CheckContainingFunc(int death_location) {
  CHECK(death_location != 1);
  CHECK(death_location != 2);
  CHECK(death_location != 3);
}

int GetCheckExceptionData(EXCEPTION_POINTERS* p, DWORD* code, void** addr) {
  *code = p->ExceptionRecord->ExceptionCode;
  *addr = p->ExceptionRecord->ExceptionAddress;
  return EXCEPTION_EXECUTE_HANDLER;
}

TEST_F(LoggingTest, CheckCausesDistinctBreakpoints) {
  DWORD code1 = 0;
  DWORD code2 = 0;
  DWORD code3 = 0;
  void* addr1 = nullptr;
  void* addr2 = nullptr;
  void* addr3 = nullptr;

  // Record the exception code and addresses.
  __try {
    CheckContainingFunc(1);
  } __except (
      GetCheckExceptionData(GetExceptionInformation(), &code1, &addr1)) {
  }

  __try {
    CheckContainingFunc(2);
  } __except (
      GetCheckExceptionData(GetExceptionInformation(), &code2, &addr2)) {
  }

  __try {
    CheckContainingFunc(3);
  } __except (
      GetCheckExceptionData(GetExceptionInformation(), &code3, &addr3)) {
  }

  // Ensure that the exception codes are correct (in particular, breakpoints,
  // not access violations).
  EXPECT_EQ(STATUS_BREAKPOINT, code1);
  EXPECT_EQ(STATUS_BREAKPOINT, code2);
  EXPECT_EQ(STATUS_BREAKPOINT, code3);

  // Ensure that none of the CHECKs are colocated.
  EXPECT_NE(addr1, addr2);
  EXPECT_NE(addr1, addr3);
  EXPECT_NE(addr2, addr3);
}
#elif defined(OS_FUCHSIA)

// CHECK causes a direct crash (without jumping to another function) only in
// official builds. Unfortunately, continuous test coverage on official builds
// is lower. Furthermore, since the Fuchsia implementation uses threads, it is
// not possible to rely on an implementation of CHECK that calls abort(), which
// takes down the whole process, preventing the thread exception handler from
// handling the exception. DO_CHECK here falls back on IMMEDIATE_CRASH() in
// non-official builds, to catch regressions earlier in the CQ.
#if defined(OFFICIAL_BUILD)
#define DO_CHECK CHECK
#else
#define DO_CHECK(cond) \
  if (!(cond)) {       \
    IMMEDIATE_CRASH(); \
  }
#endif

struct thread_data_t {
  // For signaling the thread ended properly.
  zx::event event;
  // For catching thread exceptions. Created by the crashing thread.
  zx::channel channel;
  // Location where the thread is expected to crash.
  int death_location;
};

// Indicates the exception channel has been created successfully.
constexpr zx_signals_t kChannelReadySignal = ZX_USER_SIGNAL_0;

// Indicates an error setting up the crash thread.
constexpr zx_signals_t kCrashThreadErrorSignal = ZX_USER_SIGNAL_1;

void* CrashThread(void* arg) {
  thread_data_t* data = (thread_data_t*)arg;
  int death_location = data->death_location;

  // Register the exception handler.
  zx_status_t status =
      zx::thread::self()->create_exception_channel(0, &data->channel);
  if (status != ZX_OK) {
    data->event.signal(0, kCrashThreadErrorSignal);
    return nullptr;
  }
  data->event.signal(0, kChannelReadySignal);

  DO_CHECK(death_location != 1);
  DO_CHECK(death_location != 2);
  DO_CHECK(death_location != 3);

  // We should never reach this point, signal the thread incorrectly ended
  // properly.
  data->event.signal(0, kCrashThreadErrorSignal);
  return nullptr;
}

// Runs the CrashThread function in a separate thread.
void SpawnCrashThread(int death_location, uintptr_t* child_crash_addr) {
  zx::event event;
  zx_status_t status = zx::event::create(0, &event);
  ASSERT_EQ(status, ZX_OK);

  // Run the thread.
  thread_data_t thread_data = {std::move(event), zx::channel(), death_location};
  pthread_t thread;
  int ret = pthread_create(&thread, nullptr, CrashThread, &thread_data);
  ASSERT_EQ(ret, 0);

  // Wait for the thread to set up its exception channel.
  zx_signals_t signals = 0;
  status =
      thread_data.event.wait_one(kChannelReadySignal | kCrashThreadErrorSignal,
                                 zx::time::infinite(), &signals);
  ASSERT_EQ(status, ZX_OK);
  ASSERT_EQ(signals, kChannelReadySignal);

  // Wait for the exception and read it out of the channel.
  status =
      thread_data.channel.wait_one(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED,
                                   zx::time::infinite(), &signals);
  ASSERT_EQ(status, ZX_OK);
  // Check the thread did crash and not terminate.
  ASSERT_FALSE(signals & ZX_CHANNEL_PEER_CLOSED);

  zx_exception_info_t exception_info;
  zx::exception exception;
  status = thread_data.channel.read(
      0, &exception_info, exception.reset_and_get_address(),
      sizeof(exception_info), 1, nullptr, nullptr);
  ASSERT_EQ(status, ZX_OK);

  // Get the crash address.
  zx::thread zircon_thread;
  status = exception.get_thread(&zircon_thread);
  ASSERT_EQ(status, ZX_OK);
  zx_thread_state_general_regs_t buffer;
  status = zircon_thread.read_state(ZX_THREAD_STATE_GENERAL_REGS, &buffer,
                                    sizeof(buffer));
  ASSERT_EQ(status, ZX_OK);
#if defined(ARCH_CPU_X86_64)
  *child_crash_addr = static_cast<uintptr_t>(buffer.rip);
#elif defined(ARCH_CPU_ARM64)
  *child_crash_addr = static_cast<uintptr_t>(buffer.pc);
#else
#error Unsupported architecture
#endif

  status = zircon_thread.kill();
  ASSERT_EQ(status, ZX_OK);
}

TEST_F(LoggingTest, CheckCausesDistinctBreakpoints) {
  uintptr_t child_crash_addr_1 = 0;
  uintptr_t child_crash_addr_2 = 0;
  uintptr_t child_crash_addr_3 = 0;

  SpawnCrashThread(1, &child_crash_addr_1);
  SpawnCrashThread(2, &child_crash_addr_2);
  SpawnCrashThread(3, &child_crash_addr_3);

  ASSERT_NE(0u, child_crash_addr_1);
  ASSERT_NE(0u, child_crash_addr_2);
  ASSERT_NE(0u, child_crash_addr_3);
  ASSERT_NE(child_crash_addr_1, child_crash_addr_2);
  ASSERT_NE(child_crash_addr_1, child_crash_addr_3);
  ASSERT_NE(child_crash_addr_2, child_crash_addr_3);
}
#elif defined(OS_POSIX) && !defined(OS_NACL) && !defined(OS_IOS) && \
    (defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY))

int g_child_crash_pipe;

void CheckCrashTestSighandler(int, siginfo_t* info, void* context_ptr) {
  // Conversely to what clearly stated in "man 2 sigaction", some Linux kernels
  // do NOT populate the |info->si_addr| in the case of a SIGTRAP. Hence we
  // need the arch-specific boilerplate below, which is inspired by breakpad.
  // At the same time, on OSX, ucontext.h is deprecated but si_addr works fine.
  uintptr_t crash_addr = 0;
#if defined(OS_MACOSX)
  crash_addr = reinterpret_cast<uintptr_t>(info->si_addr);
#else  // OS_POSIX && !OS_MACOSX
  ucontext_t* context = reinterpret_cast<ucontext_t*>(context_ptr);
#if defined(ARCH_CPU_X86)
  crash_addr = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_EIP]);
#elif defined(ARCH_CPU_X86_64)
  crash_addr = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RIP]);
#elif defined(ARCH_CPU_ARMEL)
  crash_addr = static_cast<uintptr_t>(context->uc_mcontext.arm_pc);
#elif defined(ARCH_CPU_ARM64)
  crash_addr = static_cast<uintptr_t>(context->uc_mcontext.pc);
#endif  // ARCH_*
#endif  // OS_POSIX && !OS_MACOSX
  HANDLE_EINTR(write(g_child_crash_pipe, &crash_addr, sizeof(uintptr_t)));
  _exit(0);
}

// CHECK causes a direct crash (without jumping to another function) only in
// official builds. Unfortunately, continuous test coverage on official builds
// is lower. DO_CHECK here falls back on a home-brewed implementation in
// non-official builds, to catch regressions earlier in the CQ.
#if defined(OFFICIAL_BUILD)
#define DO_CHECK CHECK
#else
#define DO_CHECK(cond) \
  if (!(cond))         \
  IMMEDIATE_CRASH()
#endif

void CrashChildMain(int death_location) {
  struct sigaction act = {};
  act.sa_sigaction = CheckCrashTestSighandler;
  act.sa_flags = SA_SIGINFO;
  ASSERT_EQ(0, sigaction(SIGTRAP, &act, nullptr));
  ASSERT_EQ(0, sigaction(SIGBUS, &act, nullptr));
  ASSERT_EQ(0, sigaction(SIGILL, &act, nullptr));
  DO_CHECK(death_location != 1);
  DO_CHECK(death_location != 2);
  printf("\n");
  DO_CHECK(death_location != 3);

  // Should never reach this point.
  const uintptr_t failed = 0;
  HANDLE_EINTR(write(g_child_crash_pipe, &failed, sizeof(uintptr_t)));
}

void SpawnChildAndCrash(int death_location, uintptr_t* child_crash_addr) {
  int pipefd[2];
  ASSERT_EQ(0, pipe(pipefd));

  int pid = fork();
  ASSERT_GE(pid, 0);

  if (pid == 0) {      // child process.
    close(pipefd[0]);  // Close reader (parent) end.
    g_child_crash_pipe = pipefd[1];
    CrashChildMain(death_location);
    FAIL() << "The child process was supposed to crash. It didn't.";
  }

  close(pipefd[1]);  // Close writer (child) end.
  DCHECK(child_crash_addr);
  int res = HANDLE_EINTR(read(pipefd[0], child_crash_addr, sizeof(uintptr_t)));
  ASSERT_EQ(static_cast<int>(sizeof(uintptr_t)), res);
}

TEST_F(LoggingTest, CheckCausesDistinctBreakpoints) {
  uintptr_t child_crash_addr_1 = 0;
  uintptr_t child_crash_addr_2 = 0;
  uintptr_t child_crash_addr_3 = 0;

  SpawnChildAndCrash(1, &child_crash_addr_1);
  SpawnChildAndCrash(2, &child_crash_addr_2);
  SpawnChildAndCrash(3, &child_crash_addr_3);

  ASSERT_NE(0u, child_crash_addr_1);
  ASSERT_NE(0u, child_crash_addr_2);
  ASSERT_NE(0u, child_crash_addr_3);
  ASSERT_NE(child_crash_addr_1, child_crash_addr_2);
  ASSERT_NE(child_crash_addr_1, child_crash_addr_3);
  ASSERT_NE(child_crash_addr_2, child_crash_addr_3);
}
#endif  // OS_POSIX

TEST_F(LoggingTest, DebugLoggingReleaseBehavior) {
#if DCHECK_IS_ON()
  int debug_only_variable = 1;
#endif
  // These should avoid emitting references to |debug_only_variable|
  // in release mode.
  DLOG_IF(INFO, debug_only_variable) << "test";
  DLOG_ASSERT(debug_only_variable) << "test";
  DPLOG_IF(INFO, debug_only_variable) << "test";
  DVLOG_IF(1, debug_only_variable) << "test";
}

TEST_F(LoggingTest, NestedLogAssertHandlers) {
  ::testing::InSequence dummy;
  ::testing::StrictMock<MockLogAssertHandler> handler_a, handler_b;

  EXPECT_CALL(
      handler_a,
      HandleLogAssert(
          _, _, base::StringPiece("First assert must be caught by handler_a"),
          _));
  EXPECT_CALL(
      handler_b,
      HandleLogAssert(
          _, _, base::StringPiece("Second assert must be caught by handler_b"),
          _));
  EXPECT_CALL(
      handler_a,
      HandleLogAssert(
          _, _,
          base::StringPiece("Last assert must be caught by handler_a again"),
          _));

  logging::ScopedLogAssertHandler scoped_handler_a(base::BindRepeating(
      &MockLogAssertHandler::HandleLogAssert, base::Unretained(&handler_a)));

  // Using LOG(FATAL) rather than CHECK(false) here since log messages aren't
  // preserved for CHECKs in official builds.
  LOG(FATAL) << "First assert must be caught by handler_a";

  {
    logging::ScopedLogAssertHandler scoped_handler_b(base::BindRepeating(
        &MockLogAssertHandler::HandleLogAssert, base::Unretained(&handler_b)));
    LOG(FATAL) << "Second assert must be caught by handler_b";
  }

  LOG(FATAL) << "Last assert must be caught by handler_a again";
}

// Test that defining an operator<< for a type in a namespace doesn't prevent
// other code in that namespace from calling the operator<<(ostream, wstring)
// defined by logging.h. This can fail if operator<<(ostream, wstring) can't be
// found by ADL, since defining another operator<< prevents name lookup from
// looking in the global namespace.
namespace nested_test {
  class Streamable {};
  ALLOW_UNUSED_TYPE std::ostream& operator<<(std::ostream& out,
                                             const Streamable&) {
    return out << "Streamable";
  }
  TEST_F(LoggingTest, StreamingWstringFindsCorrectOperator) {
    std::wstring wstr = L"Hello World";
    std::ostringstream ostr;
    ostr << wstr;
    EXPECT_EQ("Hello World", ostr.str());
  }
}  // namespace nested_test

#if defined(OS_FUCHSIA)

class TestLogListenerSafe
    : public fuchsia::logger::testing::LogListenerSafe_TestBase {
 public:
  TestLogListenerSafe() = default;
  ~TestLogListenerSafe() override = default;

  void set_on_dump_logs_done(base::OnceClosure on_dump_logs_done) {
    on_dump_logs_done_ = std::move(on_dump_logs_done);
  }

  bool DidReceiveString(base::StringPiece message,
                        fuchsia::logger::LogMessage* logged_message) {
    for (const auto& log_message : log_messages_) {
      if (log_message.msg.find(message.as_string()) != std::string::npos) {
        *logged_message = log_message;
        return true;
      }
    }
    return false;
  }

  // LogListener implementation.
  void LogMany(std::vector<fuchsia::logger::LogMessage> messages,
               LogManyCallback callback) override {
    log_messages_.insert(log_messages_.end(),
                         std::make_move_iterator(messages.begin()),
                         std::make_move_iterator(messages.end()));
    callback();
  }

  void Done() override { std::move(on_dump_logs_done_).Run(); }

  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "NotImplemented_: " << name;
  }

 private:
  fuchsia::logger::LogListenerSafePtr log_listener_;
  std::vector<fuchsia::logger::LogMessage> log_messages_;
  base::OnceClosure on_dump_logs_done_;

  DISALLOW_COPY_AND_ASSIGN(TestLogListenerSafe);
};

// Verifies that calling the log macro goes to the Fuchsia system logs.
TEST_F(LoggingTest, FuchsiaSystemLogging) {
  const char kLogMessage[] = "system log!";
  LOG(ERROR) << kLogMessage;

  TestLogListenerSafe listener;
  fidl::Binding<fuchsia::logger::LogListenerSafe> binding(&listener);

  fuchsia::logger::LogMessage logged_message;

  base::RunLoop wait_for_message_loop;

  // |dump_logs| checks whether the expected log line has been received yet,
  // and invokes DumpLogs() if not. It passes itself as the completion callback,
  // so that when the call completes it can check again for the expected message
  // and re-invoke DumpLogs(), or quit the loop, as appropriate.
  base::RepeatingClosure dump_logs = base::BindLambdaForTesting([&]() {
    if (listener.DidReceiveString(kLogMessage, &logged_message)) {
      wait_for_message_loop.Quit();
      return;
    }

    std::unique_ptr<fuchsia::logger::LogFilterOptions> options =
        std::make_unique<fuchsia::logger::LogFilterOptions>();
    options->tags = {"base_unittests__exec"};
    fuchsia::logger::LogPtr logger =
        base::fuchsia::ComponentContextForCurrentProcess()
            ->svc()
            ->Connect<fuchsia::logger::Log>();
    listener.set_on_dump_logs_done(dump_logs);
    logger->DumpLogsSafe(binding.NewBinding(), std::move(options));
  });

  // Start the first DumpLogs() call.
  dump_logs.Run();

  // Run until kLogMessage is received.
  wait_for_message_loop.Run();

  EXPECT_EQ(logged_message.severity,
            static_cast<int32_t>(fuchsia::logger::LogLevelFilter::ERROR));
  ASSERT_EQ(logged_message.tags.size(), 1u);
  EXPECT_EQ(logged_message.tags[0], base::CommandLine::ForCurrentProcess()
                                        ->GetProgram()
                                        .BaseName()
                                        .AsUTF8Unsafe());
}

TEST_F(LoggingTest, FuchsiaLogging) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log())
      .Times(DCHECK_IS_ON() ? 2 : 1)
      .WillRepeatedly(Return("log message"));

  SetMinLogLevel(LOG_INFO);

  EXPECT_TRUE(LOG_IS_ON(INFO));
  EXPECT_EQ(DCHECK_IS_ON(), DLOG_IS_ON(INFO));

  ZX_LOG(INFO, ZX_ERR_INTERNAL) << mock_log_source.Log();
  ZX_DLOG(INFO, ZX_ERR_INTERNAL) << mock_log_source.Log();

  ZX_CHECK(true, ZX_ERR_INTERNAL);
  ZX_DCHECK(true, ZX_ERR_INTERNAL);
}
#endif  // defined(OS_FUCHSIA)

TEST_F(LoggingTest, LogPrefix) {
  // Set up a callback function to capture the log output string.
  auto old_log_message_handler = GetLogMessageHandler();
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static std::string* log_string_ptr = nullptr;
  std::string log_string;
  log_string_ptr = &log_string;
  SetLogMessageHandler([](int severity, const char* file, int line,
                          size_t start, const std::string& str) -> bool {
    *log_string_ptr = str;
    return true;
  });

  // Logging with a prefix includes the prefix string after the opening '['.
  const char kPrefix[] = "prefix";
  SetLogPrefix(kPrefix);
  LOG(ERROR) << "test";  // Writes into |log_string|.
  EXPECT_EQ(1u, log_string.find(kPrefix));

  // Logging without a prefix does not include the prefix string.
  SetLogPrefix(nullptr);
  LOG(ERROR) << "test";  // Writes into |log_string|.
  EXPECT_EQ(std::string::npos, log_string.find(kPrefix));

  // Clean up.
  SetLogMessageHandler(old_log_message_handler);
  log_string_ptr = nullptr;
}

#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER) && \
    !BUILDFLAG(IS_HWASAN)
// Since we scan potentially uninitialized portions of the stack, we can't run
// this test under any sanitizer that checks for uninitialized reads.
TEST_F(LoggingTest, LogMessageMarkersOnStack) {
  const uint32_t kLogStartMarker = 0xbedead01;
  const uint32_t kLogEndMarker = 0x5050dead;
  const char kTestMessage[] = "Oh noes! I have crashed! 💩";

  uint32_t stack_start = 0;

  // Install a LogAssertHandler which will scan between |stack_start| and its
  // local-scope stack for the start & end markers, and verify the message.
  ScopedLogAssertHandler assert_handler(base::BindRepeating(
      [](uint32_t* stack_start_ptr, const char* file, int line,
         const base::StringPiece message, const base::StringPiece stack_trace) {
        uint32_t stack_end;
        uint32_t* stack_end_ptr = &stack_end;

        // Scan the stack for the expected markers.
        uint32_t* start_marker = nullptr;
        uint32_t* end_marker = nullptr;
        for (uint32_t* ptr = stack_end_ptr; ptr <= stack_start_ptr; ++ptr) {
          if (*ptr == kLogStartMarker)
            start_marker = ptr;
          else if (*ptr == kLogEndMarker)
            end_marker = ptr;
        }

        // Verify that start & end markers were found, somewhere, in-between
        // this and the LogAssertHandler scope, in the LogMessage destructor's
        // stack frame.
        ASSERT_TRUE(start_marker);
        ASSERT_TRUE(end_marker);

        // Verify that the |message| is found in-between the markers.
        const char* start_char_marker =
            reinterpret_cast<char*>(start_marker + 1);
        const char* end_char_marker = reinterpret_cast<char*>(end_marker);

        const base::StringPiece stack_view(start_char_marker,
                                           end_char_marker - start_char_marker);
        ASSERT_FALSE(stack_view.find(message) == base::StringPiece::npos);
      },
      &stack_start));

  // Trigger a log assertion, with a test message we can check for.
  LOG(FATAL) << kTestMessage;
}
#endif  // !defined(ADDRESS_SANITIZER)

}  // namespace

}  // namespace logging
