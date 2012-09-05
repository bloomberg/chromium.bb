// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_STOPPABLE_H_
#define REMOTING_BASE_STOPPABLE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// A helper base class that implements asynchronous shutdown on a specific
// thread.
class Stoppable {
 public:
  // Constructs an object and stores the callback to be posted to |task_runner|
  // once the object has been shutdown completely.
  Stoppable(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
            const base::Closure& stopped_callback);
  virtual ~Stoppable();

  // Initiates shutdown. It can be called by both the owner of the object and
  // the object itself resulting in the same shutdown sequence.
  void Stop();

 protected:
  // Called by derived classes to notify about shutdown completion. Posts
  // the completion task on |task_runner_| message loop.
  void CompleteStopping();

  // Derived classes have to override this method to implement the shutdown
  // logic.
  virtual void DoStop() = 0;

  enum State {
    kRunning,
    kStopping,
    kStopped
  };

  State stoppable_state() const { return state_; }

 private:
  State state_;

  // A callback to be called when shutdown is completed.
  base::Closure stopped_callback_;

  // The target task runner where the shutdown notification will be posted.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Stoppable);
};

}  // namespace remoting

#endif  // REMOTING_BASE_STOPPABLE_H_
