// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_thread.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "third_party/libjingle/source/talk/base/ssladapter.h"

namespace remoting {

const int kRunTasksMessageId = 1;

TaskPump::TaskPump() {
}

void TaskPump::WakeTasks() {
  talk_base::Thread::Current()->Post(this);
}

int64 TaskPump::CurrentTime() {
  return static_cast<int64>(talk_base::Time());
}

void TaskPump::OnMessage(talk_base::Message* pmsg) {
  RunTasks();
}

JingleThread::JingleThread()
    : task_pump_(NULL),
      started_event_(true, false),
      message_loop_(NULL) {
}

JingleThread::~JingleThread() { }

void JingleThread::Start() {
  Thread::Start();
  started_event_.Wait();
}

void JingleThread::Run() {
  MessageLoopForIO message_loop;
  message_loop_ = &message_loop;

  TaskPump task_pump;
  task_pump_ = &task_pump;

  // Signal after we've initialized |message_loop_| and |task_pump_|.
  started_event_.Signal();

  Post(this, kRunTasksMessageId);

  Thread::Run();

  message_loop.RunAllPending();

  task_pump_ = NULL;
  message_loop_ = NULL;
}

// This method is called every 20ms to process tasks from |message_loop_|
// on this thread.
// TODO(sergeyu): Remove it when JingleThread moved to Chromium's base::Thread.
void JingleThread::PumpAuxiliaryLoops() {
  MessageLoop::current()->RunAllPending();

  // Schedule next execution 20ms from now.
  PostDelayed(20, this, kRunTasksMessageId);
}

void JingleThread::OnMessage(talk_base::Message* msg) {
  PumpAuxiliaryLoops();
}

}  // namespace remoting
