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

  // Revocation checking is not needed because we use self-signed
  // certs. Disable it so that SSL layer doesn't try to initialize
  // OCSP (OCSP works only on IO thread).
  ssl_config.rev_checking_enabled = false;

  // SSLClientSocket takes ownership of the adapter.
  net::HostPortPair host_and_port(
      ContentDescription::kChromotingContentName, 0);
  net::SSLClientSocketContext context;
  context.cert_verifier = cert_verifier;
  net::SSLClientSocket* ssl_socket =
      net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
          socket, host_and_port, ssl_config, NULL, context);
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
      ssl_client_socket_(NULL),
      ssl_server_socket_(NULL),
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
    ssl_client_socket_ = CreateSSLClientSocket(
        socket_.release(), remote_cert_, cert_verifier_.get());
    socket_.reset(ssl_client_socket_);

    result = ssl_client_socket_->Connect(&ssl_connect_callback_);
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
    ssl_server_socket_ = net::CreateSSLServerSocket(
        socket_.release(), cert, local_private_key_, ssl_config);
    socket_.reset(ssl_server_socket_);

    result = ssl_server_socket_->Handshake(&ssl_connect_callback_);
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
  AuthenticateChannel();
}

void JingleStreamConnector::AuthenticateChannel() {
  if (initiator_) {
    authenticator_.reset(new ClientChannelAuthenticator(ssl_client_socket_));
  } else {
    authenticator_.reset(new HostChannelAuthenticator(ssl_server_socket_));
  }

  authenticator_->Authenticate(
      session_->shared_secret(),
      base::Bind(&JingleStreamConnector::OnAuthenticationDone,
                 base::Unretained(this)));
}

void JingleStreamConnector::OnAuthenticationDone(
    ChannelAuthenticator::Result result) {
  switch (result) {
    case ChannelAuthenticator::SUCCESS:
      NotifyDone(socket_.release());
      break;

    case ChannelAuthenticator::FAILURE:
      NotifyError();
      break;
  }
}

void JingleStreamConnector::NotifyDone(net::StreamSocket* socket) {
  session_->OnChannelConnectorFinished(name_, this);
  callback_.Run(socket);
  delete this;
}

void JingleStreamConnector::NotifyError() {
  socket_.reset();
  NotifyDone(NULL);
}

}  // namespace protocol
}  // namespace remoting
