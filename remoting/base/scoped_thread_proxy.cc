// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/scoped_thread_proxy.h"

namespace remoting {

class ScopedThreadProxy::Core : public base::RefCountedThreadSafe<Core> {
 public:
  Core(base::MessageLoopProxy* message_loop)
      : message_loop_(message_loop),
        canceled_(false) {
  }

  void PostTask(const tracked_objects::Location& from_here,
                const base::Closure& closure) {
    if (!canceled_)
      message_loop_->PostTask(from_here, Wrap(closure));
  }

  void PostDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& closure,
      int64 delay_ms) {
    if (!canceled_) {
      message_loop_->PostDelayedTask(from_here, Wrap(closure), delay_ms);
    }
  }

  void Detach() {
    DCHECK(message_loop_->BelongsToCurrentThread());
    canceled_ = true;
  }

 private:
  friend class base::RefCountedThreadSafe<Core>;

  ~Core() {
    DCHECK(canceled_);
  }

  base::Closure Wrap(const base::Closure& closure) {
    return base::Bind(&Core::CallClosure, this, closure);
  }

  void CallClosure(const base::Closure& closure) {
    DCHECK(message_loop_->BelongsToCurrentThread());

    if (!canceled_)
      closure.Run();
  }

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

ScopedThreadProxy::ScopedThreadProxy(base::MessageLoopProxy* message_loop)
    : core_(new Core(message_loop)) {
}

ScopedThreadProxy::~ScopedThreadProxy() {
  Detach();
}

void ScopedThreadProxy::PostTask(const tracked_objects::Location& from_here,
                                 const base::Closure& closure) {
  core_->PostTask(from_here, closure);
}

void ScopedThreadProxy::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& closure,
    int64 delay_ms) {
  core_->PostDelayedTask(from_here, closure, delay_ms);
}

void ScopedThreadProxy::Detach() {
  core_->Detach();
}

}  // namespace remoting
