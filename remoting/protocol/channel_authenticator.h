// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_CHANNEL_AUTHENTICATOR_H_

#include <string>

#include "base/callback.h"
#include "net/base/net_errors.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

// Interface for channel authentications that perform channel-level
// authentication. Depending on implementation channel authenticators
// may also establish SSL connection. Each instance of this interface
// should be used only once for one channel.
class ChannelAuthenticator {
 public:
  typedef base::Callback<void(net::Error error, net::StreamSocket*)>
      DoneCallback;

  virtual ~ChannelAuthenticator() {}

  // Start authentication of the given |socket|. Takes ownership of
  // |socket|, and caller must not use |socket| after calling this
  // method. |done_callback| is called when authentication is
  // finished. Callback may be invoked before this method
  // returns. Callback handler must take ownership of the result.
  virtual void SecureAndAuthenticate(
      net::StreamSocket* socket, const DoneCallback& done_callback) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHANNEL_AUTHENTICATOR_H_
