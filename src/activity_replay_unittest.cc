// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/file_util.h>
#include <base/logging.h>
#include <gtest/gtest.h>

#include "gestures/include/activity_replay.h"
#include "gestures/include/logging_filter_interpreter.h"
#include "gestures/include/gestures.h"

using std::string;

namespace gestures {

class ActivityReplayTest : public ::testing::Test {};

// This test reads a log file and replays it. This test should be enabled for a
// hands-on debugging session.

TEST(ActivityReplayTest, DISABLED_SimpleTest) {
  GestureInterpreter* c_interpreter = NewGestureInterpreter();

  Interpreter* interpreter = c_interpreter->interpreter();
  PropRegistry* prop_reg = c_interpreter->prop_reg();

  const char filename[] = "touchpad_activity_log.txt";
  string log_contents;
  ASSERT_TRUE(file_util::ReadFileToString(FilePath(filename), &log_contents));

  ActivityReplay replay(prop_reg);
  replay.Parse(log_contents);
  replay.Replay(interpreter);

  // Dump the new log
  static_cast<LoggingFilterInterpreter*>(interpreter)->log_.Dump("testlog.txt");

  DeleteGestureInterpreter(c_interpreter);
}

}  // namespace gestures
