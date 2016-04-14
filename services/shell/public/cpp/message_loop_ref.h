// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_MESSAGE_LOOP_REF_H_
#define SERVICES_SHELL_PUBLIC_CPP_MESSAGE_LOOP_REF_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/system/core.h"

namespace shell {

class MessageLoopRefImpl;

// An interface implementation can keep this object as a member variable to
// hold a reference to the ShellConnection, keeping it alive as long as the
// bound implementation exists.
// Since interface implementations can be bound on different threads than the
// ShellConnection, this class is safe to use on any thread. However, each
// instance should only be used on one thread at a time (otherwise there'll be
// races between the AddRef resulting from cloning and destruction).
class MessageLoopRef {
 public:
  virtual ~MessageLoopRef() {}

  virtual std::unique_ptr<MessageLoopRef> Clone() = 0;
};

class MessageLoopRefFactory {
 public:
  MessageLoopRefFactory();
  ~MessageLoopRefFactory();

  void set_quit_closure(const base::Closure& quit_closure) {
    quit_closure_ = quit_closure;
  }

  std::unique_ptr<MessageLoopRef> CreateRef();

 private:
  friend MessageLoopRefImpl;

  // Called from MessageLoopRefImpl.
  void AddRef();
  void Release();

  base::Closure quit_closure_;
  int ref_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopRefFactory);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_MESSAGE_LOOP_REF_H_
