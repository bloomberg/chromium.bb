// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "breakpad/src/client/windows/crash_generation/client_info.h"
#include "breakpad/src/client/windows/crash_generation/crash_generation_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The name of the environment variable used to pass the crash server pipe name
// to the crashing child process.
const char kPipeVariableName[] = "REMOTING_BREAKPAD_WIN_DEATH_TEST_PIPE_NAME";

// The prefix string used to generate a unique crash server pipe name.
// The name has to be unique as multiple test instances can be running
// simultaneously.
const wchar_t kPipeNamePrefix[] = L"\\\\.\\pipe\\";

class MockCrashServerCallbacks {
 public:
  MockCrashServerCallbacks();
  virtual ~MockCrashServerCallbacks();

  // |google_breakpad::CrashGenerationServer| invokes callbacks from artitrary
  // thread pool threads. |OnClientDumpRequested| is the only one that happened
  // to be called in synchronous manner. While it is still called on
  // a thread pool thread, the crashing process will wait until the server
  // signals an event after |OnClientDumpRequested| completes (or until 15
  // seconds timeout expires).
  MOCK_METHOD0(OnClientDumpRequested, void());

  static void OnClientDumpRequestCallback(
      void* context,
      const google_breakpad::ClientInfo* client_info,
      const std::wstring* file_path);
};

MockCrashServerCallbacks::MockCrashServerCallbacks() {
}

MockCrashServerCallbacks::~MockCrashServerCallbacks() {
}

// static
void MockCrashServerCallbacks::OnClientDumpRequestCallback(
    void* context,
    const google_breakpad::ClientInfo* /* client_info */,
    const std::wstring* /* file_path */) {
  reinterpret_cast<MockCrashServerCallbacks*>(context)->OnClientDumpRequested();
}

}  // namespace

namespace remoting {

void InitializeCrashReportingForTest(const wchar_t* pipe_name);

class BreakpadWinDeathTest : public testing::Test {
 public:
  BreakpadWinDeathTest();
  virtual ~BreakpadWinDeathTest();

  virtual void SetUp() OVERRIDE;

 protected:
  scoped_ptr<google_breakpad::CrashGenerationServer> crash_server_;
  scoped_ptr<MockCrashServerCallbacks> callbacks_;
  std::wstring pipe_name_;
};

BreakpadWinDeathTest::BreakpadWinDeathTest() {
}

BreakpadWinDeathTest::~BreakpadWinDeathTest() {
}

void BreakpadWinDeathTest::SetUp() {
  scoped_ptr<base::Environment> environment(base::Environment::Create());
  std::string pipe_name;
  if (environment->GetVar(kPipeVariableName, &pipe_name)) {
    // This is a child process. Initialize crash dump reporting to the crash
    // dump server.
    pipe_name_ = base::UTF8ToWide(pipe_name);
    InitializeCrashReportingForTest(pipe_name_.c_str());
  } else {
    // This is the parent process. Generate a unique pipe name and setup
    // a dummy crash dump server.
    UUID guid = {0};
    RPC_STATUS status = UuidCreate(&guid);
    EXPECT_TRUE(status == RPC_S_OK || status == RPC_S_UUID_LOCAL_ONLY);

    pipe_name_ =
        base::StringPrintf(
            L"%ls%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            kPipeNamePrefix,
            guid.Data1,
            guid.Data2,
            guid.Data3,
            guid.Data4[0],
            guid.Data4[1],
            guid.Data4[2],
            guid.Data4[3],
            guid.Data4[4],
            guid.Data4[5],
            guid.Data4[6],
            guid.Data4[7]);
    EXPECT_TRUE(environment->SetVar(kPipeVariableName,
                                    base::WideToUTF8(pipe_name_)));

    // Setup a dummy crash dump server.
    callbacks_.reset(new MockCrashServerCallbacks());
    crash_server_.reset(
        new google_breakpad::CrashGenerationServer(
            pipe_name_,
            NULL,
            NULL,
            NULL,
            MockCrashServerCallbacks::OnClientDumpRequestCallback,
            callbacks_.get(),
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            NULL));
    ASSERT_TRUE(crash_server_->Start());
  }
}

TEST_F(BreakpadWinDeathTest, TestAccessViolation) {
  if (callbacks_.get()) {
    EXPECT_CALL(*callbacks_, OnClientDumpRequested());
  }

  // Generate access violation exception.
  ASSERT_DEATH(*reinterpret_cast<int*>(NULL) = 1, "");
}

TEST_F(BreakpadWinDeathTest, TestInvalidParameter) {
  if (callbacks_.get()) {
    EXPECT_CALL(*callbacks_, OnClientDumpRequested());
  }

  // Cause the invalid parameter callback to be called.
  ASSERT_EXIT(printf(NULL), testing::ExitedWithCode(0), "");
}

TEST_F(BreakpadWinDeathTest, TestDebugbreak) {
  if (callbacks_.get()) {
    EXPECT_CALL(*callbacks_, OnClientDumpRequested());
  }

  // See if __debugbreak() is intercepted.
  ASSERT_DEATH(__debugbreak(), "");
}

}  // namespace remoting
