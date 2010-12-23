// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a wrapper class to help ref-counting a protobuf message.
// This file should only be inclued on host_message_dispatcher.cc and
// client_message_dispatche.cc.

// A single protobuf can contain multiple messages that will be handled by
// different message handlers.  We use this wrapper to ensure that the
// protobuf is only deleted after all the handlers have finished executing.

#ifndef REMOTING_PROTOCOL_REF_COUNTED_MESSAGE_H_
#define REMOTING_PROTOCOL_REF_COUNTED_MESSAGE_H_

#include "base/ref_counted.h"
#include "base/task.h"

namespace remoting {
namespace protocol {

template <typename T>
class RefCountedMessage : public base::RefCounted<RefCountedMessage<T> > {
 public:
  RefCountedMessage(T* message) : message_(message) { }

  T* message() { return message_.get(); }

 private:
  scoped_ptr<T> message_;
};

// Dummy methods to destroy messages.
template <class T>
static void DeleteMessage(scoped_refptr<T> message) { }

template <class T>
static Task* NewDeleteTask(scoped_refptr<T> message) {
  return NewRunnableFunction(&DeleteMessage<T>, message);
}

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_REF_COUNTED_MESSAGE_H_
