// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_DATAGRAM_CONNECTOR_H_
#define REMOTING_PROTOCOL_JINGLE_DATAGRAM_CONNECTOR_H_

#include <string>

#include "net/base/completion_callback.h"
#include "remoting/protocol/jingle_channel_connector.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

class JingleSession;

class JingleDatagramConnector : public JingleChannelConnector {
 public:
  JingleDatagramConnector(JingleSession* session,
                         const std::string& name,
                         const Session::DatagramChannelCallback& callback);
  virtual ~JingleDatagramConnector();

  // JingleChannelConnector implementation.
  // TODO(sergeyu): In the current implementation ChannelAuthenticator
  // cannot be used for datagram channels, so needs to be either
  // extended or replaced with something else here.
  virtual void Connect(ChannelAuthenticator* authenticator,
                       cricket::TransportChannel* raw_channel) OVERRIDE;

 private:
  JingleSession* session_;
  std::string name_;
  Session::DatagramChannelCallback callback_;

  scoped_ptr<ChannelAuthenticator> authenticator_;

  DISALLOW_COPY_AND_ASSIGN(JingleDatagramConnector);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_DATAGRAM_CONNECTOR_H_
