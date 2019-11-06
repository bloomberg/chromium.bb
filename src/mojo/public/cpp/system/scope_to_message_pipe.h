// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_SCOPE_TO_MESSAGE_PIPE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_SCOPE_TO_MESSAGE_PIPE_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

namespace internal {

// Owns the state and details to implement ScopeToMessagePipe (see below).
class MOJO_CPP_SYSTEM_EXPORT MessagePipeScoperBase {
 public:
  explicit MessagePipeScoperBase(ScopedMessagePipeHandle pipe);
  virtual ~MessagePipeScoperBase();

  static void StartWatchingPipe(std::unique_ptr<MessagePipeScoperBase> scoper);

 private:
  ScopedMessagePipeHandle pipe_;
  SimpleWatcher pipe_watcher_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipeScoperBase);
};

template <typename T>
class MessagePipeScoper : public MessagePipeScoperBase {
 public:
  explicit MessagePipeScoper(T scoped_object, ScopedMessagePipeHandle pipe)
      : MessagePipeScoperBase(std::move(pipe)),
        scoped_object_(std::move(scoped_object)) {}
  ~MessagePipeScoper() override = default;

 private:
  T scoped_object_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipeScoper);
};

}  // namespace internal

// Binds the lifetime of |object| to that of |pipe|'s connection. When |pipe|'s
// peer is closed, |pipe| will be closed and |object| will be destroyed. This
// can be useful as a simple mechanism to track object lifetime across process
// boundaries.
template <typename T>
void ScopeToMessagePipe(T object, ScopedMessagePipeHandle pipe) {
  internal::MessagePipeScoperBase::StartWatchingPipe(
      std::make_unique<internal::MessagePipeScoper<T>>(std::move(object),
                                                       std::move(pipe)));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_SCOPE_TO_MESSAGE_PIPE_H_
