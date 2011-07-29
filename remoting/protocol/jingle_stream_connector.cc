// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_stream_connector.h"

#include "crypto/hmac.h"
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

// Size of the HMAC-SHA-1 authentication digest.
const int kAuthDigestLength = 20;

// Labels for use when exporting the SSL master keys.
const char kClientSslExporterLabel[] = "EXPORTER-remoting-channel-auth-client";

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
      ssl_client_socket_(NULL),
      ssl_server_socket_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(tcp_connect_callback_(
          this, &JingleStreamConnector::OnTCPConnect)),
      ALLOW_THIS_IN_INITIALIZER_LIST(ssl_connect_callback_(
          this, &JingleStreamConnector::OnSSLConnect)),
      ALLOW_THIS_IN_INITIALIZER_LIST(auth_write_callback_(
          this, &JingleStreamConnector::OnAuthBytesWritten)),
      ALLOW_THIS_IN_INITIALIZER_LIST(auth_read_callback_(
          this, &JingleStreamConnector::OnAuthBytesRead)) {
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
  DCHECK(CalledOnValidThread());
  if (initiator_) {
    // Allocate a buffer to write the authentication digest.
    scoped_refptr<net::IOBuffer> write_buf =
        new net::IOBuffer(kAuthDigestLength);
    auth_write_buf_ = new net::DrainableIOBuffer(write_buf,
                                                 kAuthDigestLength);

    // Generate the auth digest to send.
    if (!GetAuthBytes(kClientSslExporterLabel, auth_write_buf_->data())) {
      NotifyError();
      return;
    }

    DoAuthWrite();
  } else {
    // Read an authentication digest.
    auth_read_buf_ = new net::GrowableIOBuffer();
    auth_read_buf_->SetCapacity(kAuthDigestLength);
    DoAuthRead();
  }
}

void JingleStreamConnector::DoAuthWrite() {
  while (true) {
    int result = socket_->Write(auth_write_buf_,
                                auth_write_buf_->BytesRemaining(),
                                &auth_write_callback_);
    if (result == net::ERR_IO_PENDING)
      break;
    if (!HandleAuthBytesWritten(result))
      break;
  }
}

void JingleStreamConnector::DoAuthRead() {
  while (true) {
    int result = socket_->Read(auth_read_buf_,
                               auth_read_buf_->RemainingCapacity(),
                               &auth_read_callback_);
    if (result == net::ERR_IO_PENDING)
      break;
    if (!HandleAuthBytesRead(result))
      break;
  }
}

void JingleStreamConnector::OnAuthBytesWritten(int result) {
  if (HandleAuthBytesWritten(result))
    DoAuthWrite();
}

void JingleStreamConnector::OnAuthBytesRead(int result) {
  if (HandleAuthBytesRead(result))
    DoAuthRead();
}

bool JingleStreamConnector::HandleAuthBytesWritten(int result) {
  DCHECK(CalledOnValidThread());

  if (result <= 0) {
    LOG(ERROR) << "Error writing authentication: " << result;
    NotifyError();
    return false;
  }

  auth_write_buf_->DidConsume(result);
  if (auth_write_buf_->BytesRemaining() > 0)
    return true;

  NotifyDone(socket_.release());
  return false;
}

bool JingleStreamConnector::HandleAuthBytesRead(int read_result) {
  DCHECK(CalledOnValidThread());

  if (read_result <= 0) {
    LOG(ERROR) << "Error reading authentication: " << read_result;
    NotifyError();
    return false;
  }

  auth_read_buf_->set_offset(auth_read_buf_->offset() + read_result);
  if (auth_read_buf_->RemainingCapacity() > 0)
    return true;

  if (!VerifyAuthBytes(
          kClientSslExporterLabel,
          auth_read_buf_->StartOfBuffer())) {
    NotifyError();
    return false;
  }

  NotifyDone(socket_.release());
  return false;
}

bool JingleStreamConnector::VerifyAuthBytes(const char* label,
                                            const char* auth_bytes) {
  char expected[kAuthDigestLength];
  if (!GetAuthBytes(label, expected))
    return false;
  // Compare the received and expected digests in fixed time, to limit the
  // scope for timing attacks.
  uint8 result = 0;
  for (unsigned i = 0; i < sizeof(expected); i++) {
    result |= auth_bytes[i] ^ expected[i];
  }
  if (result != 0) {
    LOG(ERROR) << "Mismatched authentication";
    return false;
  }
  return true;
}

bool JingleStreamConnector::GetAuthBytes(const char* label,
                                         char* out_bytes) {
  // Fetch keying material from the socket.
  unsigned char key_material[kAuthDigestLength];
  int result;
  if (initiator_) {
    result = ssl_client_socket_->ExportKeyingMaterial(
        kClientSslExporterLabel, "", key_material, sizeof(key_material));
  } else {
    result = ssl_server_socket_->ExportKeyingMaterial(
        kClientSslExporterLabel, "", key_material, sizeof(key_material));
  }
  if (result != net::OK) {
    LOG(ERROR) << "Error fetching keying material: " << result;
    return false;
  }

  // Generate auth digest based on the keying material and shared secret.
  crypto::HMAC response(crypto::HMAC::SHA1);
  if (!response.Init(session_->shared_secret())) {
    NOTREACHED() << "HMAC::Init failed";
    return false;
  }
  base::StringPiece message(reinterpret_cast<const char*>(key_material),
                            sizeof(key_material));
  if (!response.Sign(
          message,
          reinterpret_cast<unsigned char*>(out_bytes),
          kAuthDigestLength)) {
    NOTREACHED() << "HMAC::Sign failed";
    return false;
  }

  return true;
}

void JingleStreamConnector::NotifyDone(net::StreamSocket* socket) {
  session_->OnChannelConnectorFinished(name_, this);
  callback_.Run(name_, socket);
  delete this;
}

void JingleStreamConnector::NotifyError() {
  socket_.reset();
  NotifyDone(NULL);
}

}  // namespace protocol
}  // namespace remoting
