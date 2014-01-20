// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/forwarders_manager.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "tools/android/forwarder2/forwarder.h"
#include "tools/android/forwarder2/socket.h"

namespace forwarder2 {

ForwardersManager::ForwardersManager() : delegate_(new Delegate()) {}

ForwardersManager::~ForwardersManager() {
  delegate_->Clear();
}

void ForwardersManager::CreateAndStartNewForwarder(scoped_ptr<Socket> socket1,
                                                   scoped_ptr<Socket> socket2) {
  delegate_->CreateAndStartNewForwarder(socket1.Pass(), socket2.Pass());
}

ForwardersManager::Delegate::Delegate() {}

ForwardersManager::Delegate::~Delegate() {
  // The forwarder instances should already have been deleted on their
  // construction thread. Deleting them here would be unsafe since we don't know
  // which thread this destructor is called on.
  DCHECK(forwarders_.empty());
}

void ForwardersManager::Delegate::Clear() {
  if (!forwarders_constructor_runner_) {
    DCHECK(forwarders_.empty());
    return;
  }
  if (forwarders_constructor_runner_->RunsTasksOnCurrentThread()) {
    ClearOnForwarderConstructorThread();
    return;
  }
  forwarders_constructor_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &ForwardersManager::Delegate::ClearOnForwarderConstructorThread,
          this));
}

void ForwardersManager::Delegate::CreateAndStartNewForwarder(
    scoped_ptr<Socket> socket1,
    scoped_ptr<Socket> socket2) {
  const scoped_refptr<base::SingleThreadTaskRunner> current_task_runner(
      base::MessageLoopProxy::current());
  DCHECK(current_task_runner);
  if (forwarders_constructor_runner_) {
    DCHECK_EQ(current_task_runner, forwarders_constructor_runner_);
  } else {
    forwarders_constructor_runner_ = current_task_runner;
  }
  forwarders_.push_back(
      new Forwarder(socket1.Pass(), socket2.Pass(),
                    &deletion_notifier_,
                    base::Bind(&ForwardersManager::Delegate::OnForwarderError,
                               this)));
  forwarders_.back()->Start();
}

void ForwardersManager::Delegate::OnForwarderError(
    scoped_ptr<Forwarder> forwarder) {
  DCHECK(forwarders_constructor_runner_->RunsTasksOnCurrentThread());
  const ScopedVector<Forwarder>::iterator it = std::find(
      forwarders_.begin(), forwarders_.end(), forwarder.get());
  DCHECK(it != forwarders_.end());
  std::swap(*it, forwarders_.back());
  forwarders_.pop_back();
  ignore_result(forwarder.release());  // Deleted by the pop_back() above.
}

void ForwardersManager::Delegate::ClearOnForwarderConstructorThread() {
  DCHECK(forwarders_constructor_runner_->RunsTasksOnCurrentThread());
  deletion_notifier_.Notify();
  forwarders_.clear();
}

}  // namespace forwarder2
