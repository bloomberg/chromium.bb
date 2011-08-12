// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/jingle_thread.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
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
      : thread_(thread), delegate_(NULL), stopping_(false) {
  }

  virtual void Run(Delegate* delegate) {
    delegate_ = delegate;

    thread_->Thread::Run();
    // Call Restart() so that we can run again.
    thread_->Restart();

    delegate_ = NULL;
  }

  virtual void Quit() {
    if (!stopping_) {
      stopping_ = true;

      // Shutdown gracefully: make sure that we excute all messages
      // left in the queue before exiting. Thread::Quit() would not do
      // that.
      thread_->Post(this, kStopMessageId);
    }
  }

  virtual void ScheduleWork() {
    thread_->Post(this, kRunTasksMessageId);
  }

  virtual void ScheduleDelayedWork(const base::TimeTicks& time) {
    delayed_work_time_ = time;
    ScheduleNextDelayedTask();
  }

  void OnMessage(talk_base::Message* msg) {
    if (msg->message_id == kRunTasksMessageId) {
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
    } else if (msg->message_id == kStopMessageId) {
      DCHECK(stopping_);
      // Stop the thread only if there are no more non-delayed
      // messages left in the queue, otherwise post another task to
      // try again later.
      int delay = thread_->GetDelay();
      if (delay > 0 || delay == talk_base::kForever) {
        stopping_ = false;
        thread_->Quit();
      } else {
        thread_->Post(this, kStopMessageId);
      }
    } else {
      NOTREACHED();
    }
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
  bool stopping_;
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
  message_loop_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();

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
  message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  stopped_event_.Wait();

  // This will wait until the thread is actually finished.
  Thread::Stop();
}

MessageLoop* JingleThread::message_loop() {
  return message_loop_;
}

base::MessageLoopProxy* JingleThread::message_loop_proxy() {
  return message_loop_proxy_;
}

TaskPump* JingleThread::task_pump() {
  return task_pump_;
}

}  // namespace remoting
