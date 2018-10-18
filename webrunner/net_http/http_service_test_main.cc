// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  // Instantiate various global structures.
  base::TaskScheduler::CreateAndStartWithDefaultParams("HTTP Service Test");
  base::CommandLine::Init(argc, argv);
  base::MessageLoopForIO loop;
  base::AtExitManager exit_manager;

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
