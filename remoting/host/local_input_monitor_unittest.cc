// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/local_input_monitor.h"
#include "remoting/host/mouse_move_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPoint.h"

namespace remoting {

using testing::_;
using testing::AnyNumber;

namespace {

class FakeMouseMoveObserver : public MouseMoveObserver {
 public:
  FakeMouseMoveObserver() {}
  virtual ~FakeMouseMoveObserver() {}

  // Ignore mouse events to avoid breaking the test of someone moves the mouse.
  virtual void OnLocalMouseMoved(const SkIPoint&) OVERRIDE {}
  void OnDisconnectCallback() {}
};

}  // namespace

class LocalInputMonitorTest : public testing::Test {
 public:
  LocalInputMonitorTest() {}

  virtual void SetUp() OVERRIDE {
    task_runner_ = new AutoThreadTaskRunner(
        message_loop_.message_loop_proxy(),
        base::Bind(&LocalInputMonitorTest::OnStopTaskRunner,
                   base::Unretained(this)));
  }

  void OnStopTaskRunner() {
    message_loop_.PostTask(
        FROM_HERE, base::Bind(&LocalInputMonitorTest::DestroyLocalInputMonitor,
                              base::Unretained(this)));
  }

  void DestroyLocalInputMonitor() {
    if (local_input_monitor_) {
      local_input_monitor_->Stop();
      local_input_monitor_.reset();
    }

    message_loop_.PostTask(FROM_HERE, run_loop_.QuitClosure());
  }

  MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  FakeMouseMoveObserver mouse_move_observer_;
  scoped_ptr<LocalInputMonitor> local_input_monitor_;
};

// This test is really to exercise only the creation and destruction code in
// LocalInputMonitor.
TEST_F(LocalInputMonitorTest, Basic) {
  local_input_monitor_ = LocalInputMonitor::Create();
  local_input_monitor_->Start(
      &mouse_move_observer_,
      base::Bind(&FakeMouseMoveObserver::OnDisconnectCallback,
                 base::Unretained(&mouse_move_observer_)));

  task_runner_ = NULL;
  run_loop_.Run();
}

}  // namespace remoting
