// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_FACTORY_H_
#define REMOTING_PROTOCOL_CHANNEL_FACTORY_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"

namespace net {
class Socket;
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class ChannelFactory : public base::NonThreadSafe {
 public:
  // TODO(sergeyu): Specify connection error code when channel
  // connection fails.
  typedef base::Callback<void(scoped_ptr<net::StreamSocket>)>
      StreamChannelCallback;
  typedef base::Callback<void(scoped_ptr<net::Socket>)>
      DatagramChannelCallback;

  ChannelFactory() {}

  // Creates new channels for this connection. The specified callback is called
  // when then new channel is created and connected. The callback is called with
  // NULL if connection failed for any reason. Callback may be called
  // synchronously, before the call returns. All channels must be destroyed
  // before the factory is destroyed and CancelChannelCreation() must be called
  // to cancel creation of channels for which the |callback| hasn't been called
  // yet.
  virtual void CreateStreamChannel(
      const std::string& name, const StreamChannelCallback& callback) = 0;
  virtual void CreateDatagramChannel(
      const std::string& name, const DatagramChannelCallback& callback) = 0;

  // Cancels a pending CreateStreamChannel() or CreateDatagramChannel()
  // operation for the named channel. If the channel creation already
  // completed then canceling it has no effect. When shutting down
  // this method must be called for each channel pending creation.
  virtual void CancelChannelCreation(const std::string& name) = 0;

 protected:
  virtual ~ChannelFactory() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ChannelFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHANNEL_FACTORY_H_
