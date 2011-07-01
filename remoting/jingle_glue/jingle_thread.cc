// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

namespace {

class JingleMessagePump : public base::MessagePump,
                          public talk_base::MessageHandler {
 public:
  JingleMessagePump(talk_base::Thread* thread)
      : thread_(thread), delegate_(NULL) {
  }

  virtual void Run(Delegate* delegate) {
    delegate_ = delegate;

    talk_base::Thread::Current()->Thread::Run();
    // Call Restart() so that we can run again.
    talk_base::Thread::Current()->Restart();

    delegate_ = NULL;
  }

  virtual void Quit() {
    talk_base::Thread::Current()->Quit();
  }

  virtual void ScheduleWork() {
    thread_->Post(this, kRunTasksMessageId);
  }

  virtual void ScheduleDelayedWork(const base::TimeTicks& time) {
    delayed_work_time_ = time;
    ScheduleNextDelayedTask();
  }

  void OnMessage(talk_base::Message* msg) {
    DCHECK(msg->message_id == kRunTasksMessageId);
    DCHECK(delegate_);

    // Clear currently pending messages in case there were delayed tasks.
    // Will schedule it again from ScheduleNextDelayedTask() if neccessary.
    thread_->Clear(this, kRunTasksMessageId);

    // Process all pending tasks.
    while (true) {
      if (delegate_->DoWork())
        continue;
      if (delegate_->DoDelayedWork(&delayed_work_time_))
        continue;
      if (delegate_->DoIdleWork())
        continue;
      break;
    }

    ScheduleNextDelayedTask();
  }

 private:
  void ScheduleNextDelayedTask() {
    if (!delayed_work_time_.is_null()) {
      base::TimeTicks now = base::TimeTicks::Now();
      int delay = static_cast<int>((delayed_work_time_ - now).InMilliseconds());
      if (delay > 0) {
        thread_->PostDelayed(delay, this, kRunTasksMessageId);
      } else {
        thread_->Post(this, kRunTasksMessageId);
      }
    }
  }

  talk_base::Thread* thread_;
  Delegate* delegate_;
  base::TimeTicks delayed_work_time_;
};

}  // namespace

JingleThreadMessageLoop::JingleThreadMessageLoop(talk_base::Thread* thread)
    : MessageLoop(MessageLoop::TYPE_IO) {
  pump_ = new JingleMessagePump(thread);
}

JingleThreadMessageLoop::~JingleThreadMessageLoop() {
}

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
  JingleThreadMessageLoop message_loop(this);
  message_loop_ = &message_loop;

  TaskPump task_pump;
  task_pump_ = &task_pump;

  // Signal after we've initialized |message_loop_| and |task_pump_|.
  started_event_.Signal();

  message_loop.Run();

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

TaskPump* JingleThread::task_pump() {
  return task_pump_;
}

void JingleThread::OnMessage(talk_base::Message* msg) {
  DCHECK(msg->message_id == kStopMessageId);

  // Stop the thread only if there are no more messages left in the queue,
  // otherwise post another task to try again later.
  if (!msgq_.empty() || fPeekKeep_) {
    Post(this, kStopMessageId);
  } else {
    MessageQueue::Quit();
  }
}

}  // namespace remoting
