// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/v1_client_channel_authenticator.h"

#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "remoting/protocol/auth_util.h"

namespace remoting {
namespace protocol {

V1ClientChannelAuthenticator::V1ClientChannelAuthenticator(
    const std::string& host_cert,
    const std::string& shared_secret)
    : host_cert_(host_cert),
      shared_secret_(shared_secret),
      socket_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(connect_callback_(
          this, &V1ClientChannelAuthenticator::OnConnected)),
      ALLOW_THIS_IN_INITIALIZER_LIST(auth_write_callback_(
          this, &V1ClientChannelAuthenticator::OnAuthBytesWritten)) {
}

V1ClientChannelAuthenticator::~V1ClientChannelAuthenticator() {
}

void V1ClientChannelAuthenticator::SecureAndAuthenticate(
    net::StreamSocket* socket, const DoneCallback& done_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket->IsConnected());

  done_callback_ = done_callback;

  cert_verifier_.reset(new net::CertVerifier());

  net::SSLConfig::CertAndStatus cert_and_status;
  cert_and_status.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  cert_and_status.der_cert = host_cert_;

  net::SSLConfig ssl_config;
  // Certificate verification and revocation checking are not needed
  // because we use self-signed certs. Disable it so that the SSL
  // layer doesn't try to initialize OCSP (OCSP works only on the IO
  // thread).
  ssl_config.allowed_bad_certs.push_back(cert_and_status);
  ssl_config.rev_checking_enabled = false;

  net::HostPortPair host_and_port(kSslFakeHostName, 0);
  net::SSLClientSocketContext context;
  context.cert_verifier = cert_verifier_.get();
  socket_.reset(
      net::ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
          socket, host_and_port, ssl_config, NULL, context));

  int result = socket_->Connect(&connect_callback_);
  if (result == net::ERR_IO_PENDING)
    return;
  OnConnected(result);
}

void V1ClientChannelAuthenticator::OnConnected(int result) {
  if (result != net::OK) {
    LOG(ERROR) << "Failed to establish SSL connection";
    done_callback_.Run(static_cast<net::Error>(result), NULL);
  }

  // Get keying material from SSL.
  unsigned char key_material[kAuthDigestLength];
  int export_result = socket_->ExportKeyingMaterial(
      kClientAuthSslExporterLabel, "", key_material, kAuthDigestLength);
  if (export_result != net::OK) {
    LOG(ERROR) << "Error fetching keying material: " << export_result;
    done_callback_.Run(static_cast<net::Error>(export_result), NULL);
    return;
  }

  // Generate authentication digest to write to the socket.
  std::string auth_bytes;
  if (!GetAuthBytes(shared_secret_,
                    std::string(reinterpret_cast<char*>(key_material),
                                kAuthDigestLength),
                    &auth_bytes)) {
    done_callback_.Run(net::ERR_FAILED, NULL);
    return;
  }

  // Allocate a buffer to write the digest.
  auth_write_buf_ = new net::DrainableIOBuffer(
      new net::StringIOBuffer(auth_bytes), auth_bytes.size());
  WriteAuthenticationBytes();
}

void V1ClientChannelAuthenticator::WriteAuthenticationBytes() {
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

void V1ClientChannelAuthenticator::OnAuthBytesWritten(int result) {
  DCHECK(CalledOnValidThread());

  if (HandleAuthBytesWritten(result))
    WriteAuthenticationBytes();
}

bool V1ClientChannelAuthenticator::HandleAuthBytesWritten(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Error writing authentication: " << result;
    done_callback_.Run(static_cast<net::Error>(result), NULL);
    return false;
  }

  auth_write_buf_->DidConsume(result);
  if (auth_write_buf_->BytesRemaining() > 0)
    return true;

  done_callback_.Run(net::OK, socket_.release());
  return false;
}

}  // namespace protocol
}  // namespace remoting
