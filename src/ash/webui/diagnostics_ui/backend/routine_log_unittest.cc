// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/routine_log.h"

#include <string>
#include <vector>

#include "ash/webui/diagnostics_ui/backend/log_test_helpers.h"
#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {

const char kLogFileName[] = "diagnostic_routine_log";

}  // namespace

class RoutineLogTest : public testing::Test {
 public:
  RoutineLogTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().AppendASCII(kLogFileName);
  }

  ~RoutineLogTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;
};

TEST_F(RoutineLogTest, Empty) {
  RoutineLog log(log_path_);

  EXPECT_FALSE(base::PathExists(log_path_));
  EXPECT_TRUE(log.GetContents().empty());
}

TEST_F(RoutineLogTest, Basic) {
  RoutineLog log(log_path_);

  log.LogRoutineStarted(mojom::RoutineType::kCpuStress);

  EXPECT_TRUE(base::PathExists(log_path_));

  const std::string contents = log.GetContents();
  const std::string first_line = GetLogLines(contents)[0];
  const std::vector<std::string> first_line_contents =
      GetLogLineContents(first_line);

  ASSERT_EQ(3u, first_line_contents.size());
  EXPECT_EQ("CpuStress", first_line_contents[1]);
  EXPECT_EQ("Started", first_line_contents[2]);
}

TEST_F(RoutineLogTest, TwoLine) {
  RoutineLog log(log_path_);

  log.LogRoutineStarted(mojom::RoutineType::kMemory);
  log.LogRoutineCompleted(mojom::RoutineType::kMemory,
                          mojom::StandardRoutineResult::kTestPassed);
  EXPECT_TRUE(base::PathExists(log_path_));

  const std::string contents = log.GetContents();
  const std::vector<std::string> log_lines = GetLogLines(contents);
  const std::string first_line = log_lines[0];
  const std::vector<std::string> first_line_contents =
      GetLogLineContents(first_line);

  ASSERT_EQ(3u, first_line_contents.size());
  EXPECT_EQ("Memory", first_line_contents[1]);
  EXPECT_EQ("Started", first_line_contents[2]);

  const std::string second_line = log_lines[1];
  const std::vector<std::string> second_line_contents =
      GetLogLineContents(second_line);

  ASSERT_EQ(3u, second_line_contents.size());
  EXPECT_EQ("Memory", second_line_contents[1]);
  EXPECT_EQ("Passed", second_line_contents[2]);
}

TEST_F(RoutineLogTest, Cancelled) {
  RoutineLog log(log_path_);

  log.LogRoutineStarted(mojom::RoutineType::kMemory);
  log.LogRoutineCancelled();
  EXPECT_TRUE(base::PathExists(log_path_));

  const std::string contents = log.GetContents();
  LOG(ERROR) << contents;
  const std::vector<std::string> log_lines = GetLogLines(contents);

  ASSERT_EQ(2u, log_lines.size());
  const std::string second_line = log_lines[1];
  const std::vector<std::string> second_line_contents =
      GetLogLineContents(second_line);

  ASSERT_EQ(2u, second_line_contents.size());
  EXPECT_EQ("Inflight Routine Cancelled", second_line_contents[1]);
}

}  // namespace diagnostics
}  // namespace ash
