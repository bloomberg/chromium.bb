// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_stream_connector.h"

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
#include "remoting/protocol/jingle_session.h"

namespace remoting {
namespace protocol {

namespace {

// Value is choosen to balance the extra latency against the reduced
// load due to ACK traffic.
const int kTcpAckDelayMilliseconds = 10;

// Helper method to create a SSL client socket.
net::SSLClientSocket* CreateSSLClientSocket(
    net::StreamSocket* socket, const std::string& der_cert,
    net::CertVerifier* cert_verifier) {
  net::SSLConfig ssl_config;

  // Certificate provided by the host doesn't need authority.
  net::SSLConfig::CertAndStatus cert_and_status;
  cert_and_status.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  cert_and_status.der_cert = der_cert;
  ssl_config.allowed_bad_certs.push_back(cert_and_status);

  // SSLClientSocket takes ownership of the adapter.
  net::HostPortPair host_and_pair(JingleSession::kChromotingContentName, 0);
  net::SSLClientSocketContext context;
  context.cert_verifier = cert_verifier;
  net::SSLClientSocket* ssl_socket =
      net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
          socket, host_and_pair, ssl_config, NULL, context);
  return ssl_socket;
}

}  // namespace

JingleStreamConnector::JingleStreamConnector(
    JingleSession* session,
    const std::string& name,
    const Session::StreamChannelCallback& callback)
    : session_(session),
      name_(name),
      callback_(callback),
      initiator_(false),
      local_private_key_(NULL),
      raw_channel_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tcp_connect_callback_(
          this, &JingleStreamConnector::OnTCPConnect)),
      ALLOW_THIS_IN_INITIALIZER_LIST(ssl_connect_callback_(
          this, &JingleStreamConnector::OnSSLConnect)) {
}

JingleStreamConnector::~JingleStreamConnector() {
}

void JingleStreamConnector::Connect(bool initiator,
                                    const std::string& local_cert,
                                    const std::string& remote_cert,
                                    crypto::RSAPrivateKey* local_private_key,
                                    cricket::TransportChannel* raw_channel) {
  DCHECK(CalledOnValidThread());
  DCHECK(!raw_channel_);

  initiator_ = initiator;
  local_cert_ = local_cert;
  remote_cert_ = remote_cert;
  local_private_key_ = local_private_key;
  raw_channel_ = raw_channel;

  net::Socket* socket =
      new jingle_glue::TransportChannelSocketAdapter(raw_channel_);

  if (!EstablishTCPConnection(socket))
    NotifyError();
}

bool JingleStreamConnector::EstablishTCPConnection(net::Socket* socket) {
  jingle_glue::PseudoTcpAdapter* adapter =
      new jingle_glue::PseudoTcpAdapter(socket);
  adapter->SetAckDelay(kTcpAckDelayMilliseconds);
  adapter->SetNoDelay(true);

  socket_.reset(adapter);
  int result = socket_->Connect(&tcp_connect_callback_);
  if (result == net::ERR_IO_PENDING) {
    return true;
  } else if (result == net::OK) {
    tcp_connect_callback_.Run(result);
    return true;
  }

  return false;
}

bool JingleStreamConnector::EstablishSSLConnection() {
  DCHECK(socket_->IsConnected());

  int result;
  if (initiator_) {
    cert_verifier_.reset(new net::CertVerifier());

    // Create client SSL socket.
    net::SSLClientSocket* ssl_client_socket = CreateSSLClientSocket(
        socket_.release(), remote_cert_, cert_verifier_.get());
    socket_.reset(ssl_client_socket);

    result = ssl_client_socket->Connect(&ssl_connect_callback_);
  } else {
    scoped_refptr<net::X509Certificate> cert =
        net::X509Certificate::CreateFromBytes(
            local_cert_.data(), local_cert_.length());
    if (!cert) {
      LOG(ERROR) << "Failed to parse X509Certificate";
      return false;
    }

    // Create server SSL socket.
    net::SSLConfig ssl_config;

    net::SSLServerSocket* ssl_server_socket =
        net::CreateSSLServerSocket(socket_.release(), cert,
                                   local_private_key_, ssl_config);
    socket_.reset(ssl_server_socket);

    result = ssl_server_socket->Handshake(&ssl_connect_callback_);
  }

  if (result == net::ERR_IO_PENDING) {
    return true;
  } else if (result != net::OK) {
    LOG(ERROR) << "Failed to establish SSL connection";
    return false;
  }

  // Reach here if net::OK is received.
  ssl_connect_callback_.Run(net::OK);
  return true;
}

void JingleStreamConnector::OnTCPConnect(int result) {
  DCHECK(CalledOnValidThread());

  if (result != net::OK) {
    LOG(ERROR) << "PseudoTCP connection failed: " << result;
    NotifyError();
    return;
  }

  if (!EstablishSSLConnection())
    NotifyError();
}

void JingleStreamConnector::OnSSLConnect(int result) {
  DCHECK(CalledOnValidThread());

  if (result != net::OK) {
    LOG(ERROR) << "Error during SSL connection: " << result;
    NotifyError();
    return;
  }

  DCHECK(socket_->IsConnected());
  NotifyDone(socket_.release());
}

void JingleStreamConnector::NotifyDone(net::StreamSocket* socket) {
  callback_.Run(name_, socket);
  session_->OnChannelConnectorFinished(name_, this);
}

void JingleStreamConnector::NotifyError() {
  socket_.reset();
  callback_.Run(name_, NULL);
  session_->OnChannelConnectorFinished(name_, this);
}

}  // namespace protocol
}  // namespace remoting
