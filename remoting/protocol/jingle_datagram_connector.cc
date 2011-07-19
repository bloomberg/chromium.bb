// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_datagram_connector.h"

#include "jingle/glue/channel_socket_adapter.h"
#include "remoting/protocol/jingle_session.h"
#include "third_party/libjingle/source/talk/p2p/base/p2ptransportchannel.h"

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
    bool initiator,
    const std::string& local_cert,
    const std::string& remote_cert,
    crypto::RSAPrivateKey* local_private_key,
    cricket::TransportChannel* raw_channel) {
  DCHECK(CalledOnValidThread());

  if (!initiator) {
    // Don't make outgoing connections from the host to client.
    raw_channel->GetP2PChannel()->set_incoming_only(true);
  }

  net::Socket* socket =
      new jingle_glue::TransportChannelSocketAdapter(raw_channel);

  // TODO(sergeyu): Implement encryption for datagram channels.

  callback_.Run(name_, socket);
  session_->OnChannelConnectorFinished(name_, this);
}

}  // namespace protocol
}  // namespace remoting
