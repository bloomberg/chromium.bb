// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_DISPATCHER_BASE_H_
#define REMOTING_PROTOCOL_CHANNEL_DISPATCHER_BASE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/buffered_socket_writer.h"
#include "remoting/protocol/errors.h"

namespace remoting {

class CompoundBuffer;

namespace protocol {

class MessagePipe;
class P2PStreamSocket;
class StreamChannelFactory;

// Base class for channel message dispatchers. It's responsible for
// creating the named channel. Derived dispatchers then dispatch
// incoming messages on this channel as well as send outgoing
// messages.
class ChannelDispatcherBase {
 public:
  class EventHandler {
   public:
    EventHandler() {}
    virtual ~EventHandler() {}

    virtual void OnChannelInitialized(
        ChannelDispatcherBase* channel_dispatcher) = 0;
    virtual void OnChannelError(ChannelDispatcherBase* channel_dispatcher,
                                ErrorCode error) = 0;
  };

  // The callback is called when initialization is finished. The
  // parameter is set to true on success.
  typedef base::Callback<void(bool)> InitializedCallback;

  virtual ~ChannelDispatcherBase();

  // Creates and connects the channel in the specified
  // |session|. Caller retains ownership of the Session.
  void Init(StreamChannelFactory* channel_factory, EventHandler* event_handler);

  const std::string& channel_name() { return channel_name_; }

  // Returns true if the channel is currently connected.
  bool is_connected() { return message_pipe() != nullptr; }

 protected:
  explicit ChannelDispatcherBase(const char* channel_name);

  MessagePipe* message_pipe() { return message_pipe_.get(); }

  // Child classes must override this method to handle incoming messages.
  virtual void OnIncomingMessage(scoped_ptr<CompoundBuffer> message) = 0;

 private:
  void OnChannelReady(scoped_ptr<P2PStreamSocket> socket);
  void OnPipeError(int error);

  std::string channel_name_;
  StreamChannelFactory* channel_factory_;
  EventHandler* event_handler_;

  scoped_ptr<MessagePipe> message_pipe_;

  DISALLOW_COPY_AND_ASSIGN(ChannelDispatcherBase);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHANNEL_DISPATCHER_BASE_H_
