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

#if defined(OS_WIN)
const MessageLoop::Type kDesiredMessageLoopType = MessageLoop::TYPE_UI;
#else  // !defined(OS_WIN)
const MessageLoop::Type kDesiredMessageLoopType = MessageLoop::TYPE_IO;
#endif  // !defined(OS_WIN)

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
  LocalInputMonitorTest() : message_loop_(kDesiredMessageLoopType) {
  }

  virtual void SetUp() OVERRIDE {
    // Arrange to run |message_loop_| until no components depend on it.
    task_runner_ = new AutoThreadTaskRunner(
        message_loop_.message_loop_proxy(), run_loop_.QuitClosure());
  }

  MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  FakeMouseMoveObserver mouse_move_observer_;
};

// This test is really to exercise only the creation and destruction code in
// LocalInputMonitor.
TEST_F(LocalInputMonitorTest, Basic) {
  {
    scoped_ptr<LocalInputMonitor> local_input_monitor =
        LocalInputMonitor::Create(task_runner_, task_runner_, task_runner_);
    task_runner_ = NULL;

    local_input_monitor->Start(
        &mouse_move_observer_,
        base::Bind(&FakeMouseMoveObserver::OnDisconnectCallback,
                   base::Unretained(&mouse_move_observer_)));
    local_input_monitor->Stop();
  }

  run_loop_.Run();
}

}  // namespace remoting
