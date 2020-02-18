// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/process_proxy/process_output_watcher.h"

#include <gtest/gtest.h>
#include <stddef.h>

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_pump_type.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"

namespace chromeos {

namespace {

struct TestCase {
  TestCase(const std::string& input, bool send_terminating_null)
      : input(input),
        should_send_terminating_null(send_terminating_null),
        expected_output(input) {}

  // Conctructor for cases where the output is not expected to be the same as
  // input.
  TestCase(const std::string& input,
           bool send_terminating_null,
           const std::string& expected_output)
      : input(input),
        should_send_terminating_null(send_terminating_null),
        expected_output(expected_output) {}

  std::string input;
  bool should_send_terminating_null;
  std::string expected_output;
};

class ProcessWatcherExpectations {
 public:
  ProcessWatcherExpectations() = default;

  void SetTestCase(const TestCase& test_case) {
    received_from_out_ = 0;

    out_expectations_ = test_case.expected_output;
    if (test_case.should_send_terminating_null)
      out_expectations_.append(std::string("", 1));
  }

  bool CheckExpectations(const std::string& data, ProcessOutputType type) {
    EXPECT_EQ(PROCESS_OUTPUT_TYPE_OUT, type);
    if (type != PROCESS_OUTPUT_TYPE_OUT)
      return false;

    if (out_expectations_.length() == 0 && data.length() == 0)
      return true;

    EXPECT_LT(received_from_out_, out_expectations_.length());
    if (received_from_out_ >= out_expectations_.length())
      return false;

    EXPECT_EQ(received_from_out_,
              out_expectations_.find(data, received_from_out_));
    if (received_from_out_ != out_expectations_.find(data, received_from_out_))
      return false;

    received_from_out_ += data.length();
    return true;
  }

  bool IsDone() {
    return received_from_out_ >= out_expectations_.length();
  }

 private:
  std::string out_expectations_;
  size_t received_from_out_;
};

void StopProcessOutputWatcher(std::unique_ptr<ProcessOutputWatcher> watcher) {
  // Just deleting |watcher| if sufficient.
}

}  // namespace

class ProcessOutputWatcherTest : public testing::Test {
 public:
  ProcessOutputWatcherTest() : output_watch_thread_started_(false),
                               failed_(false) {
  }

  ~ProcessOutputWatcherTest() override = default;

  void TearDown() override {
    if (output_watch_thread_started_)
      output_watch_thread_->Stop();
  }

  void OnRead(ProcessOutputType type,
              const std::string& output,
              base::OnceClosure ack_callback) {
    ASSERT_FALSE(failed_);
    // There may be an EXIT signal sent during test tear down (which is sent
    // by process output watcher when master end of test pseudo-terminal is
    // closed). If this happens, ignore it. If EXIT is seen before test
    // expectations are met, fall through in order to fail the test.
    if (type == PROCESS_OUTPUT_TYPE_EXIT && expectations_.IsDone()) {
      ASSERT_TRUE(test_case_done_callback_.is_null());
      return;
    }

    failed_ = !expectations_.CheckExpectations(output, type);
    if (failed_ || expectations_.IsDone()) {
      ASSERT_FALSE(test_case_done_callback_.is_null());
      task_environment_.GetMainThreadTaskRunner()->PostTask(
          FROM_HERE, std::move(test_case_done_callback_));
      test_case_done_callback_.Reset();
    }

    ASSERT_FALSE(ack_callback.is_null());
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, std::move(ack_callback));
  }

 protected:
  std::string VeryLongString() {
    std::string result = "0123456789";
    for (int i = 0; i < 8; i++)
      result = result.append(result);
    return result;
  }

  void RunTest(const std::vector<TestCase>& test_cases) {
    ASSERT_FALSE(output_watch_thread_started_);
    output_watch_thread_.reset(new base::Thread("ProcessOutpuWatchThread"));
    output_watch_thread_started_ = output_watch_thread_->StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    ASSERT_TRUE(output_watch_thread_started_);

    int pt_pipe[2];
    ASSERT_FALSE(HANDLE_EINTR(pipe(pt_pipe)));

    auto crosh_watcher = std::make_unique<ProcessOutputWatcher>(
        pt_pipe[0], base::BindRepeating(&ProcessOutputWatcherTest::OnRead,
                                        base::Unretained(this)));

    output_watch_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ProcessOutputWatcher::Start,
                                  base::Unretained(crosh_watcher.get())));

    for (size_t i = 0; i < test_cases.size(); i++) {
      expectations_.SetTestCase(test_cases[i]);

      base::RunLoop run_loop;
      ASSERT_TRUE(test_case_done_callback_.is_null());
      test_case_done_callback_ = run_loop.QuitClosure();

      const std::string& test_str = test_cases[i].input;
      // Let's make inputs not NULL terminated, unless other is specified in
      // the test case.
      ssize_t test_size = test_str.length() * sizeof(*test_str.c_str());
      if (test_cases[i].should_send_terminating_null)
        test_size += sizeof(*test_str.c_str());
      EXPECT_TRUE(base::WriteFileDescriptor(pt_pipe[1], test_str.c_str(),
                                            test_size));

      run_loop.Run();
      EXPECT_TRUE(expectations_.IsDone());
      if (failed_)
        break;
    }

    output_watch_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&StopProcessOutputWatcher, std::move(crosh_watcher)));

    EXPECT_NE(-1, IGNORE_EINTR(close(pt_pipe[1])));
  }

 private:
  base::OnceClosure test_case_done_callback_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::Thread> output_watch_thread_;
  bool output_watch_thread_started_;
  bool failed_;
  ProcessWatcherExpectations expectations_;
  std::vector<TestCase> exp;
};

TEST_F(ProcessOutputWatcherTest, OutputWatcher) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("t", false));
  test_cases.push_back(TestCase("testing output\n", false));
  test_cases.push_back(TestCase("testing error\n", false));
  test_cases.push_back(TestCase("testing error1\n", false));
  test_cases.push_back(TestCase("testing output1\n", false));
  test_cases.push_back(TestCase("testing output2\n", false));
  test_cases.push_back(TestCase("testing output3\n", false));
  test_cases.push_back(TestCase(VeryLongString(), false));
  test_cases.push_back(TestCase("testing error2\n", false));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, SplitUTF8Character) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("test1\xc2", false, "test1"));
  test_cases.push_back(TestCase("\xb5test1", false, "\xc2\xb5test1"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, SplitSoleUTF8Character) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("\xc2", false, ""));
  test_cases.push_back(TestCase("\xb5", false, "\xc2\xb5"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, SplitUTF8CharacterLength3) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("test3\xe2\x82", false, "test3"));
  test_cases.push_back(TestCase("\xac", false, "\xe2\x82\xac"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, SplitSoleUTF8CharacterThreeWays) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("\xe2", false, ""));
  test_cases.push_back(TestCase("\x82", false, ""));
  test_cases.push_back(TestCase("\xac", false, "\xe2\x82\xac"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, EndsWithThreeByteUTF8Character) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("test\xe2\x82\xac", false, "test\xe2\x82\xac"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, SoleThreeByteUTF8Character) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("\xe2\x82\xac", false, "\xe2\x82\xac"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, HasThreeByteUTF8Character) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(
      TestCase("test\xe2\x82\xac_", false, "test\xe2\x82\xac_"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, MulitByteUTF8CharNullTerminated) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("test\xe2\x82\xac", true, "test\xe2\x82\xac"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, MultipleMultiByteUTF8Characters) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(
      TestCase("test\xe2\x82\xac\xc2", false, "test\xe2\x82\xac"));
  test_cases.push_back(TestCase("\xb5", false, "\xc2\xb5"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, ContainsInvalidUTF8) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("\xc2_", false, "\xc2_"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, InvalidUTF8SeriesOfTrailingBytes) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("\x82\x82\x82", false, "\x82\x82\x82"));
  test_cases.push_back(TestCase("\x82\x82\x82", false, "\x82\x82\x82"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, EndsWithInvalidUTF8) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("\xff", false, "\xff"));

  RunTest(test_cases);
}

TEST_F(ProcessOutputWatcherTest, FourByteUTF8) {
  std::vector<TestCase> test_cases;
  test_cases.push_back(TestCase("\xf0\xa4\xad", false, ""));
  test_cases.push_back(TestCase("\xa2", false, "\xf0\xa4\xad\xa2"));

  RunTest(test_cases);
}

// Verifies that sending '\0' generates PROCESS_OUTPUT_TYPE_OUT event and does
// not terminate output watcher.
TEST_F(ProcessOutputWatcherTest, SendNull) {
  std::vector<TestCase> test_cases;
  // This will send '\0' to output watcher.
  test_cases.push_back(TestCase("", true));
  // Let's verify that next input also gets detected (i.e. output watcher does
  // not exit after seeing '\0' from previous test case).
  test_cases.push_back(TestCase("a", true));

  RunTest(test_cases);
}

}  // namespace chromeos
