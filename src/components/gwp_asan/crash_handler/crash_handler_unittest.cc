// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_handler.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/tools/tool_support.h"

namespace gwp_asan {
namespace internal {

namespace {

class GwpAsanTest : public testing::Test {};

constexpr size_t kAllocationSize = 902;
constexpr int kSuccess = 0;

int HandlerMainAdaptor(int argc, char* argv[]) {
  crashpad::UserStreamDataSources user_stream_data_sources;
  user_stream_data_sources.push_back(
      std::make_unique<gwp_asan::UserStreamDataSource>());
  return crashpad::HandlerMain(argc, argv, &user_stream_data_sources);
}

// Child process that runs the crashpad handler.
MULTIPROCESS_TEST_MAIN(CrashpadHandler) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  std::vector<base::CommandLine::StringType> argv(cmd_line->argv());
  for (auto I = argv.begin(); I != argv.end(); ++I) {
    if (I->find(L"test-child-process") != std::string::npos) {
      argv.erase(I);
      break;
    }
  }

  wchar_t* wargv[argv.size()];
  for (size_t i = 0; i < argv.size(); i++)
    wargv[i] = const_cast<wchar_t*>(argv[i].c_str());

  crashpad::ToolSupport::Wmain(argv.size(), wargv, HandlerMainAdaptor);

  return 0;
}

// Child process that launches the crashpad handler and then crashes.
MULTIPROCESS_TEST_MAIN(CrashingProcess) {
  base::NoDestructor<GuardedPageAllocator> gpa;
  gpa->Init(AllocatorState::kGpaMaxPages);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  base::FilePath directory = cmd_line->GetSwitchValuePath("directory");
  CHECK(!directory.empty());
  std::string crash_type = cmd_line->GetSwitchValueASCII("crash-type");
  CHECK(!crash_type.empty());

  static char gpa_addr_buf[32];
  snprintf(gpa_addr_buf, sizeof(gpa_addr_buf), "%zx",
           gpa->GetCrashKeyAddress());
  static crashpad::Annotation gpa_annotation(
      crashpad::Annotation::Type::kString, kGpaCrashKey, gpa_addr_buf);
  gpa_annotation.SetSize(strlen(gpa_addr_buf));

  base::FilePath metrics_dir(FILE_PATH_LITERAL(""));
  std::map<std::string, std::string> annotations;
  std::vector<std::string> arguments;
  arguments.push_back("--test-child-process=CrashpadHandler");

  crashpad::CrashpadClient* client = new crashpad::CrashpadClient();
  bool handler = client->StartHandler(/* handler */ cmd_line->GetProgram(),
                                      /* database */ directory,
                                      /* metrics_dir */ metrics_dir,
                                      /* url */ "",
                                      /* annotations */ annotations,
                                      /* arguments */ arguments,
                                      /* restartable */ false,
                                      /* asynchronous_start */ false);
  if (!handler) {
    LOG(ERROR) << "Crash handler failed to launch";
    return kSuccess;
  }

  if (crash_type == "use-after-free") {
    void* ptr = gpa->Allocate(kAllocationSize);
    gpa->Deallocate(ptr);
    *(int*)ptr = 0;
  } else if (crash_type == "double-free") {
    void* ptr = gpa->Allocate(kAllocationSize);
    gpa->Deallocate(ptr);
    gpa->Deallocate(ptr);
  } else if (crash_type == "underflow") {
    void* ptr = gpa->Allocate(kAllocationSize);
    for (size_t i = 0; i < base::GetPageSize(); i++)
      ((unsigned char*)ptr)[-i] = 0;
  } else if (crash_type == "overflow") {
    void* ptr = gpa->Allocate(kAllocationSize);
    for (size_t i = 0; i <= base::GetPageSize(); i++)
      ((unsigned char*)ptr)[i] = 0;
  } else if (crash_type == "trap") {
    __builtin_trap();
  } else {
    LOG(ERROR) << "Unknown crash type " << crash_type;
  }

  LOG(ERROR) << "This return should never be reached.";
  return kSuccess;
}

class CrashHandlerTest : public base::MultiProcessTest {
 public:
  // Launch a second process that installs a crashpad handler and causes an
  // exception of type crash_type, then validate that it exited successfully.
  // crashpad is initialized to write to the given database directory.
  bool runTestProcess(const base::FilePath& database_dir,
                      const char* crash_type) {
    base::CommandLine cmd_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    cmd_line.AppendSwitchPath("directory", database_dir);
    cmd_line.AppendSwitchASCII("crash-type", crash_type);

    base::LaunchOptions options;
    options.start_hidden = true;
    base::Process process =
        base::SpawnMultiProcessTestChild("CrashingProcess", cmd_line, options);

    int exit_code = -1;
    EXPECT_TRUE(process.WaitForExitWithTimeout(
        TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_NE(exit_code, kSuccess);

    return (exit_code != kSuccess);
  }

  // Given a directory with a single crashpad exception, read and parse the
  // minidump and identify whether it has a GWP-ASan stream. If it successfully
  // found a minidump, it writes true to minidump_found. If it sucessfully found
  // a GWP-ASan stream in the minidump, it writes true to gwp_asan_found and
  // parses the protobuf into into proto_out.
  void readGwpAsanStreamFromCrash(const base::FilePath& database_dir,
                                  bool* minidump_found,
                                  bool* gwp_asan_found,
                                  gwp_asan::Crash* proto_out) {
    *minidump_found = *gwp_asan_found = false;
    auto database =
        crashpad::CrashReportDatabase::InitializeWithoutCreating(database_dir);

    std::vector<crashpad::CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetPendingReports(&reports),
              crashpad::CrashReportDatabase::kNoError);
    ASSERT_EQ(reports.size(), 1U);

    crashpad::FileReader minidump_file_reader;
    ASSERT_TRUE(minidump_file_reader.Open(reports[0].file_path));

    crashpad::ProcessSnapshotMinidump minidump_process_snapshot;
    ASSERT_TRUE(minidump_process_snapshot.Initialize(&minidump_file_reader));
    *minidump_found = true;

    auto custom_streams = minidump_process_snapshot.CustomMinidumpStreams();
    for (auto* stream : custom_streams) {
      if (stream->stream_type() == static_cast<crashpad::MinidumpStreamType>(
                                       kGwpAsanMinidumpStreamType)) {
        ASSERT_TRUE(proto_out->ParseFromArray(stream->data().data(),
                                              stream->data().size()));
        *gwp_asan_found = true;
        return;
      }
    }
  }
};

#if defined(ADDRESS_SANITIZER) && defined(OS_WIN)
// ASan intercepts crashes and crashpad doesn't have a chance to see them.
#define MAYBE_DISABLED(name) DISABLED_ ##name
#else
#define MAYBE_DISABLED(name) name
#endif  // defined(ADDRESS_SANITIZER) && defined(OS_WIN)

TEST_F(CrashHandlerTest, MAYBE_DISABLED(UseAfterFree)) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  ASSERT_TRUE(runTestProcess(database_dir.GetPath(), "use-after-free"));

  bool minidump_found, gwp_asan_found;
  gwp_asan::Crash proto;
  readGwpAsanStreamFromCrash(database_dir.GetPath(), &minidump_found,
                             &gwp_asan_found, &proto);
  ASSERT_TRUE(minidump_found);
  ASSERT_TRUE(gwp_asan_found);

  EXPECT_TRUE(proto.has_error_type());
  EXPECT_EQ(proto.error_type(), Crash_ErrorType_USE_AFTER_FREE);

  EXPECT_TRUE(proto.has_allocation_address());

  EXPECT_TRUE(proto.has_allocation_size());
  EXPECT_EQ(proto.allocation_size(), kAllocationSize);

  EXPECT_TRUE(proto.has_allocation());
  EXPECT_TRUE(proto.allocation().has_thread_id());
  EXPECT_NE(proto.allocation().thread_id(), base::kInvalidThreadId);

  EXPECT_TRUE(proto.has_deallocation());
  EXPECT_TRUE(proto.deallocation().has_thread_id());
  EXPECT_NE(proto.deallocation().thread_id(), base::kInvalidThreadId);
  EXPECT_EQ(proto.allocation().thread_id(), proto.deallocation().thread_id());

  EXPECT_GT(proto.allocation().stack_trace_size(), 0);
  EXPECT_GT(proto.deallocation().stack_trace_size(), 0);
}

TEST_F(CrashHandlerTest, MAYBE_DISABLED(DoubleFree)) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  ASSERT_TRUE(runTestProcess(database_dir.GetPath(), "double-free"));

  bool minidump_found, gwp_asan_found;
  gwp_asan::Crash proto;
  readGwpAsanStreamFromCrash(database_dir.GetPath(), &minidump_found,
                             &gwp_asan_found, &proto);
  ASSERT_TRUE(minidump_found);
  ASSERT_TRUE(gwp_asan_found);

  EXPECT_TRUE(proto.has_error_type());
  EXPECT_EQ(proto.error_type(), Crash_ErrorType_DOUBLE_FREE);

  EXPECT_TRUE(proto.has_allocation_address());

  EXPECT_TRUE(proto.has_allocation_size());
  EXPECT_EQ(proto.allocation_size(), kAllocationSize);

  EXPECT_TRUE(proto.has_allocation());
  EXPECT_TRUE(proto.allocation().has_thread_id());
  EXPECT_NE(proto.allocation().thread_id(), base::kInvalidThreadId);

  EXPECT_TRUE(proto.has_deallocation());
  EXPECT_TRUE(proto.deallocation().has_thread_id());
  EXPECT_NE(proto.deallocation().thread_id(), base::kInvalidThreadId);
  EXPECT_EQ(proto.allocation().thread_id(), proto.deallocation().thread_id());

  EXPECT_GT(proto.allocation().stack_trace_size(), 0);
  EXPECT_GT(proto.deallocation().stack_trace_size(), 0);
}

TEST_F(CrashHandlerTest, MAYBE_DISABLED(Underflow)) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  ASSERT_TRUE(runTestProcess(database_dir.GetPath(), "underflow"));

  bool minidump_found, gwp_asan_found;
  gwp_asan::Crash proto;
  readGwpAsanStreamFromCrash(database_dir.GetPath(), &minidump_found,
                             &gwp_asan_found, &proto);
  ASSERT_TRUE(minidump_found);
  ASSERT_TRUE(gwp_asan_found);

  EXPECT_TRUE(proto.has_error_type());
  EXPECT_EQ(proto.error_type(), Crash_ErrorType_BUFFER_UNDERFLOW);

  EXPECT_TRUE(proto.has_allocation_address());

  EXPECT_TRUE(proto.has_allocation_size());
  EXPECT_EQ(proto.allocation_size(), kAllocationSize);

  EXPECT_TRUE(proto.has_allocation());
  EXPECT_TRUE(proto.allocation().has_thread_id());
  EXPECT_NE(proto.allocation().thread_id(), base::kInvalidThreadId);

  EXPECT_FALSE(proto.has_deallocation());

  EXPECT_GT(proto.allocation().stack_trace_size(), 0);
}

TEST_F(CrashHandlerTest, MAYBE_DISABLED(Overflow)) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  ASSERT_TRUE(runTestProcess(database_dir.GetPath(), "overflow"));

  bool minidump_found, gwp_asan_found;
  gwp_asan::Crash proto;
  readGwpAsanStreamFromCrash(database_dir.GetPath(), &minidump_found,
                             &gwp_asan_found, &proto);
  ASSERT_TRUE(minidump_found);
  ASSERT_TRUE(gwp_asan_found);

  EXPECT_TRUE(proto.has_error_type());
  EXPECT_EQ(proto.error_type(), Crash_ErrorType_BUFFER_OVERFLOW);

  EXPECT_TRUE(proto.has_allocation_address());

  EXPECT_TRUE(proto.has_allocation_size());
  EXPECT_EQ(proto.allocation_size(), kAllocationSize);

  EXPECT_TRUE(proto.has_allocation());
  EXPECT_TRUE(proto.allocation().has_thread_id());
  EXPECT_NE(proto.allocation().thread_id(), base::kInvalidThreadId);

  EXPECT_FALSE(proto.has_deallocation());

  EXPECT_GT(proto.allocation().stack_trace_size(), 0);
}

TEST_F(CrashHandlerTest, MAYBE_DISABLED(UnrelatedException)) {
  base::ScopedTempDir database_dir;
  ASSERT_TRUE(database_dir.CreateUniqueTempDir());

  ASSERT_TRUE(runTestProcess(database_dir.GetPath(), "trap"));

  bool minidump_found, gwp_asan_found;
  gwp_asan::Crash proto;
  readGwpAsanStreamFromCrash(database_dir.GetPath(), &minidump_found,
                             &gwp_asan_found, &proto);
  ASSERT_TRUE(minidump_found);
  ASSERT_FALSE(gwp_asan_found);
}

}  // namespace

}  // namespace internal
}  // namespace gwp_asan
