// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_DISPATCHER_BASE_H_
#define REMOTING_PROTOCOL_CHANNEL_DISPATCHER_BASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

struct ChannelConfig;
class ChannelFactory;
class Session;

// Base class for channel message dispatchers. It's responsible for
// creating the named channel. Derived dispatchers then dispatch
// incoming messages on this channel as well as send outgoing
// messages.
class ChannelDispatcherBase {
 public:
  // The callback is called when initialization is finished. The
  // parameter is set to true on success.
  typedef base::Callback<void(bool)> InitializedCallback;

  virtual ~ChannelDispatcherBase();

  // Creates and connects the channel in the specified
  // |session|. Caller retains ownership of the Session.
  void Init(Session* session,
            const ChannelConfig& config,
            const InitializedCallback& callback);

  // Returns true if the channel is currently connected.
  bool is_connected() { return channel() != NULL; }

 protected:
  explicit ChannelDispatcherBase(const char* channel_name);

  net::StreamSocket* channel() { return channel_.get(); }

  // Called when channel is initialized. Must be overriden in the
  // child classes. Should not delete the dispatcher.
  virtual void OnInitialized() = 0;

 private:
  void OnChannelReady(scoped_ptr<net::StreamSocket> socket);

  std::string channel_name_;
  ChannelFactory* channel_factory_;
  InitializedCallback initialized_callback_;
  scoped_ptr<net::StreamSocket> channel_;

  DISALLOW_COPY_AND_ASSIGN(ChannelDispatcherBase);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHANNEL_DISPATCHER_BASE_H_
