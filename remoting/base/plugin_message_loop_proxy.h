// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PLUGIN_MESSAGE_LOOP_H_
#define REMOTING_BASE_PLUGIN_MESSAGE_LOOP_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"

namespace remoting {

// MessageLoopProxy for plugin main threads.
class PluginMessageLoopProxy : public base::MessageLoopProxy {
 public:
  class Delegate {
   public:
    Delegate() { }
    virtual ~Delegate() { }

    virtual bool RunOnPluginThread(
        int delay_ms, void(function)(void*), void* data) = 0;
  };

  // Caller keeps ownership of delegate.
  PluginMessageLoopProxy(Delegate* delegate);
  virtual ~PluginMessageLoopProxy();

  void Detach();

  // base::MessageLoopProxy interface.
  virtual bool PostTask(
      const tracked_objects::Location& from_here,
      Task* task) OVERRIDE;
  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      Task* task,
      int64 delay_ms) OVERRIDE;
  virtual bool PostNonNestableTask(
      const tracked_objects::Location& from_here,
      Task* task) OVERRIDE;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      Task* task,
      int64 delay_ms) OVERRIDE;

  virtual bool PostTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;
  virtual bool PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int64 delay_ms) OVERRIDE;
  virtual bool PostNonNestableTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int64 delay_ms) OVERRIDE;

  virtual bool BelongsToCurrentThread() OVERRIDE;

 private:
  static void TaskSpringboard(void* data);

  void RunTaskIf(Task* task);
  void RunClosureIf(const base::Closure& task);

  base::PlatformThreadId plugin_thread_id_;

  // |lock_| must be acquired when accessing |delegate_|.
  base::Lock lock_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PluginMessageLoopProxy);
};

}  // namespace remoting

#endif  // REMOTING_BASE_PLUGIN_MESSAGE_LOOP_H_
