// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_thread.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_pump.h"
#include "base/time.h"
#include "third_party/libjingle/source/talk/base/ssladapter.h"

namespace remoting {

const uint32 kRunTasksMessageId = 1;
const uint32 kStopMessageId = 2;

class JingleThread::JingleMessagePump : public base::MessagePump {
 public:
  JingleMessagePump(JingleThread* thread) : thread_(thread) { }

  virtual void Run(Delegate* delegate) { NOTIMPLEMENTED() ;}
  virtual void Quit() { NOTIMPLEMENTED(); }
  virtual void ScheduleWork() {
    thread_->Post(thread_, kRunTasksMessageId);
  }
  virtual void ScheduleDelayedWork(const base::Time& time) {
    NOTIMPLEMENTED();
  }

 private:
  JingleThread* thread_;
};

class JingleThread::JingleMessageLoop : public MessageLoop {
 public:
  JingleMessageLoop(JingleThread* thread)
      : MessageLoop(MessageLoop::TYPE_IO) {
    pump_ = new JingleMessagePump(thread);
  }
};

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
      stopped_event_(true, false),
      message_loop_(NULL) {
}

JingleThread::~JingleThread() { }

void JingleThread::Start() {
  Thread::Start();
  started_event_.Wait();
}

void JingleThread::Run() {
  JingleMessageLoop message_loop(this);
  message_loop_ = &message_loop;

  TaskPump task_pump;
  task_pump_ = &task_pump;

  // Signal after we've initialized |message_loop_| and |task_pump_|.
  started_event_.Signal();

  Thread::Run();

  stopped_event_.Signal();

  task_pump_ = NULL;
  message_loop_ = NULL;
}

void JingleThread::Stop() {
  // Shutdown gracefully: make sure that we excute all messages left in the
  // queue before exiting. Thread::Stop() would not do that.
  Post(this, kStopMessageId);
  stopped_event_.Wait();

  // This will wait until the thread is actually finished.
  Thread::Stop();
}

MessageLoop* JingleThread::message_loop() {
  return message_loop_;
}

  // Returns task pump if the thread is running, otherwise NULL is returned.
TaskPump* JingleThread::task_pump() {
  return task_pump_;
}

void JingleThread::OnMessage(talk_base::Message* msg) {
  if (msg->message_id == kRunTasksMessageId) {
    // This code is executed whenever we get new message in |message_loop_|.
    // JingleMessagePump posts new tasks in the jingle thread.
    // TODO(sergeyu): Remove it when JingleThread moved on Chromium's
    // base::Thread.
    base::MessagePump::Delegate* delegate = message_loop_;
    // Loop until we run out of work.
    while (true) {
      if (!delegate->DoWork())
        break;
    }
  } else if (msg->message_id == kStopMessageId) {
    // Stop the thread only if there are no more messages left in the queue,
    // otherwise post another task to try again later.
    if (msgq_.size() > 0 || fPeekKeep_) {
      Post(this, kStopMessageId);
    } else {
      MessageQueue::Quit();
    }
  }
}

}  // namespace remoting
