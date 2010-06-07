// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_THREAD_H
#define REMOTING_JINGLE_GLUE_JINGLE_THREAD_H

#include "base/tracked_objects.h"
#include "base/waitable_event.h"
#include "talk/base/messagequeue.h"
#include "talk/base/taskrunner.h"
#include "talk/base/thread.h"

class MessageLoop;

namespace buzz {
class XmppClient;
}

namespace remoting {

class TaskPump : public talk_base::MessageHandler,
                 public talk_base::TaskRunner {
 public:
  TaskPump();

  // TaskRunner methods.
  void WakeTasks();
  int64 CurrentTime();

  // MessageHandler methods.
  void OnMessage(talk_base::Message* pmsg);
};

// TODO(sergeyu): This class should be changed to inherit from Chromiums
// base::Thread instead of libjingle's thread.
class JingleThread : public talk_base::Thread,
                     private talk_base::MessageHandler {
 public:
  JingleThread();
  virtual ~JingleThread();

  void Start();

  // Main function for the thread. Should not be called directly.
  void Run();

  // Returns Chromiums message loop for this thread.
  // TODO(sergeyu): remove this methid when we use base::Thread insted of
  // talk_base::Thread
  MessageLoop* message_loop() { return message_loop_; }

  // Returns task pump if the thread is running, otherwise NULL is returned.
  TaskPump* task_pump() { return task_pump_; }

 private:
  virtual void OnMessage(talk_base::Message* msg);

  void PumpAuxiliaryLoops();

  TaskPump* task_pump_;
  base::WaitableEvent started_event_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(JingleThread);
};

}  // namespace remoting

#endif // REMOTING_JINGLE_GLUE_JINGLE_THREAD_H
