// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RejectedPromises_h
#define RejectedPromises_h

#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"

namespace v8 {
class PromiseRejectMessage;
};

namespace blink {

class ScriptState;

class RejectedPromises final : public RefCounted<RejectedPromises> {
  USING_FAST_MALLOC(RejectedPromises);

 public:
  static scoped_refptr<RejectedPromises> Create() {
    return base::AdoptRef(new RejectedPromises());
  }

  ~RejectedPromises();
  void Dispose();

  void RejectedWithNoHandler(ScriptState*,
                             v8::PromiseRejectMessage,
                             const String& error_message,
                             std::unique_ptr<SourceLocation>,
                             AccessControlStatus);
  void HandlerAdded(v8::PromiseRejectMessage);

  void ProcessQueue();

 private:
  class Message;

  RejectedPromises();

  using MessageQueue = Deque<std::unique_ptr<Message>>;
  std::unique_ptr<MessageQueue> CreateMessageQueue();

  void ProcessQueueNow(std::unique_ptr<MessageQueue>);
  void RevokeNow(std::unique_ptr<Message>);

  MessageQueue queue_;
  Vector<std::unique_ptr<Message>> reported_as_errors_;
};

}  // namespace blink

#endif  // RejectedPromises_h
