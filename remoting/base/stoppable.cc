// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/stoppable.h"

#include "base/message_loop.h"
#include "base/single_thread_task_runner.h"

namespace remoting {

Stoppable::Stoppable(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& stopped_callback)
    : state_(kRunning),
      stopped_callback_(stopped_callback),
      task_runner_(task_runner) {
}

Stoppable::~Stoppable() {
  CHECK_EQ(state_, kStopped);
}

void Stoppable::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kRunning) {
    state_ = kStopping;
  }

  // DoStop() can be called multiple times.
  DoStop();
}

void Stoppable::CompleteStopping() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopping);

  state_ = kStopped;
  task_runner_->PostTask(FROM_HERE, stopped_callback_);
}

}  // namespace remoting
