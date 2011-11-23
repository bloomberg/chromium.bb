// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_datagram_connector.h"

#include "jingle/glue/channel_socket_adapter.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/jingle_session.h"

namespace remoting {
namespace protocol {

JingleDatagramConnector::JingleDatagramConnector(
    JingleSession* session,
    const std::string& name,
    const Session::DatagramChannelCallback& callback)
    : session_(session),
      name_(name),
      callback_(callback) {
}

JingleDatagramConnector::~JingleDatagramConnector() {
}

void JingleDatagramConnector::Connect(
    ChannelAuthenticator* authenticator,
    cricket::TransportChannel* raw_channel) {
  DCHECK(CalledOnValidThread());

  authenticator_.reset(authenticator);

  net::Socket* socket =
      new jingle_glue::TransportChannelSocketAdapter(raw_channel);

  // TODO(sergeyu): Implement encryption for datagram channels.

  session_->OnChannelConnectorFinished(name_, this);
  callback_.Run(socket);
  delete this;
}

}  // namespace protocol
}  // namespace remoting
