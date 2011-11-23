// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_stream_connector.h"

#include "base/bind.h"
#include "jingle/glue/channel_socket_adapter.h"
#include "jingle/glue/pseudotcp_adapter.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/ssl_config_service.h"
#include "net/base/x509_certificate.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_server_socket.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_session.h"

namespace remoting {
namespace protocol {

namespace {

// Value is choosen to balance the extra latency against the reduced
// load due to ACK traffic.
const int kTcpAckDelayMilliseconds = 10;

// Values for the TCP send and receive buffer size. This should be tuned to
// accomodate high latency network but not backlog the decoding pipeline.
const int kTcpReceiveBufferSize = 256 * 1024;
const int kTcpSendBufferSize = kTcpReceiveBufferSize + 30 * 1024;

}  // namespace

JingleStreamConnector::JingleStreamConnector(
    JingleSession* session,
    const std::string& name,
    const Session::StreamChannelCallback& callback)
    : session_(session),
      name_(name),
      callback_(callback),
      raw_channel_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tcp_connect_callback_(
          this, &JingleStreamConnector::OnTCPConnect)) {
}

JingleStreamConnector::~JingleStreamConnector() {
}

void JingleStreamConnector::Connect(ChannelAuthenticator* authenticator,
                                    cricket::TransportChannel* raw_channel) {
  DCHECK(CalledOnValidThread());
  DCHECK(!raw_channel_);

  authenticator_.reset(authenticator);
  raw_channel_ = raw_channel;

  net::Socket* socket =
      new jingle_glue::TransportChannelSocketAdapter(raw_channel_);

  if (!EstablishTCPConnection(socket))
    NotifyError();
}

bool JingleStreamConnector::EstablishTCPConnection(net::Socket* socket) {
  // Set options for the raw socket layer.
  // Send buffer size is set to match the PseudoTcp layer so that it can fit
  // all the data submitted by the PseudoTcp layer.
  socket->SetSendBufferSize(kTcpSendBufferSize);
  // TODO(hclam): We should also set the receive buffer size once we can detect
  // the underlying socket is a TCP socket. We should also investigate what
  // value would gurantee that Windows's UDP socket doesn't return a EWOULDBLOCK
  // error.

  // Set options for the TCP layer.
  jingle_glue::PseudoTcpAdapter* adapter =
      new jingle_glue::PseudoTcpAdapter(socket);
  adapter->SetAckDelay(kTcpAckDelayMilliseconds);
  adapter->SetNoDelay(true);
  adapter->SetReceiveBufferSize(kTcpReceiveBufferSize);
  adapter->SetSendBufferSize(kTcpSendBufferSize);

  tcp_socket_.reset(adapter);
  int result = tcp_socket_->Connect(&tcp_connect_callback_);
  if (result == net::ERR_IO_PENDING) {
    return true;
  } else if (result == net::OK) {
    tcp_connect_callback_.Run(result);
    return true;
  }

  return false;
}

void JingleStreamConnector::OnTCPConnect(int result) {
  DCHECK(CalledOnValidThread());

  if (result != net::OK) {
    LOG(ERROR) << "PseudoTCP connection failed: " << result;
    NotifyError();
    return;
  }

  authenticator_->SecureAndAuthenticate(tcp_socket_.release(), base::Bind(
      &JingleStreamConnector::OnAuthenticationDone, base::Unretained(this)));
}

void JingleStreamConnector::OnAuthenticationDone(
    net::Error error, net::StreamSocket* socket) {
  if (error != net::OK) {
    NotifyError();
  } else {
    NotifyDone(socket);
  }
}

void JingleStreamConnector::NotifyDone(net::StreamSocket* socket) {
  session_->OnChannelConnectorFinished(name_, this);
  callback_.Run(socket);
  delete this;
}

void JingleStreamConnector::NotifyError() {
  NotifyDone(NULL);
}

}  // namespace protocol
}  // namespace remoting
