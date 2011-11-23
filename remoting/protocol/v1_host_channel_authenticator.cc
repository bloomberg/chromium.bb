// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/v1_host_channel_authenticator.h"

#include "crypto/rsa_private_key.h"
#include "crypto/secure_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/base/x509_certificate.h"
#include "net/socket/ssl_server_socket.h"
#include "remoting/protocol/auth_util.h"

namespace remoting {
namespace protocol {

V1HostChannelAuthenticator::V1HostChannelAuthenticator(
    const std::string& local_cert,
    crypto::RSAPrivateKey* local_private_key,
    const std::string& shared_secret)
    : local_cert_(local_cert),
      local_private_key_(local_private_key),
      shared_secret_(shared_secret),
      socket_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(connect_callback_(
          this, &V1HostChannelAuthenticator::OnConnected)),
      ALLOW_THIS_IN_INITIALIZER_LIST(auth_read_callback_(
          this, &V1HostChannelAuthenticator::OnAuthBytesRead)) {
}

V1HostChannelAuthenticator::~V1HostChannelAuthenticator() {
}

void V1HostChannelAuthenticator::SecureAndAuthenticate(
    net::StreamSocket* socket, const DoneCallback& done_callback) {
  DCHECK(CalledOnValidThread());

  scoped_ptr<net::StreamSocket> channel_socket(socket);
  done_callback_ = done_callback;

  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBytes(
          local_cert_.data(), local_cert_.length());
  if (!cert) {
    LOG(ERROR) << "Failed to parse X509Certificate";
    done_callback.Run(net::ERR_FAILED, NULL);
    return;
  }

  net::SSLConfig ssl_config;
  socket_.reset(net::CreateSSLServerSocket(
      channel_socket.release(), cert, local_private_key_, ssl_config));

  int result = socket_->Handshake(&connect_callback_);
  if (result == net::ERR_IO_PENDING) {
    return;
  }
  OnConnected(result);
}

void V1HostChannelAuthenticator::OnConnected(int result) {
  if (result != net::OK) {
    LOG(ERROR) << "Failed to establish SSL connection";
    done_callback_.Run(static_cast<net::Error>(result), NULL);
  }

  // Read an authentication digest.
  auth_read_buf_ = new net::GrowableIOBuffer();
  auth_read_buf_->SetCapacity(kAuthDigestLength);
  DoAuthRead();
}

void V1HostChannelAuthenticator::DoAuthRead(){
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

void V1HostChannelAuthenticator::OnAuthBytesRead(int result) {
  DCHECK(CalledOnValidThread());

  if (HandleAuthBytesRead(result))
    DoAuthRead();
}

bool V1HostChannelAuthenticator::HandleAuthBytesRead(int read_result) {
  if (read_result <= 0) {
    done_callback_.Run(static_cast<net::Error>(read_result), NULL);
    return false;
  }

  auth_read_buf_->set_offset(auth_read_buf_->offset() + read_result);
  if (auth_read_buf_->RemainingCapacity() > 0)
    return true;

  if (!VerifyAuthBytes(std::string(
          auth_read_buf_->StartOfBuffer(),
          auth_read_buf_->StartOfBuffer() + kAuthDigestLength))) {
    LOG(WARNING) << "Mismatched authentication";
    done_callback_.Run(net::ERR_FAILED, NULL);
    return false;
  }

  done_callback_.Run(net::OK, socket_.release());
  return false;
}

bool V1HostChannelAuthenticator::VerifyAuthBytes(
    const std::string& received_auth_bytes) {
  DCHECK(received_auth_bytes.length() == kAuthDigestLength);

  unsigned char key_material[kAuthDigestLength];
  int export_result = socket_->ExportKeyingMaterial(
      kClientAuthSslExporterLabel, "", key_material, kAuthDigestLength);
  if (export_result != net::OK) {
    LOG(ERROR) << "Error fetching keying material: " << export_result;
    done_callback_.Run(static_cast<net::Error>(export_result), NULL);
    return false;
  }

  std::string auth_bytes;
  if (!GetAuthBytes(shared_secret_,
                    std::string(key_material, key_material + kAuthDigestLength),
                    &auth_bytes)) {
    done_callback_.Run(net::ERR_FAILED, NULL);
    return false;
  }

  return crypto::SecureMemEqual(received_auth_bytes.data(),
                                &(auth_bytes[0]), kAuthDigestLength);
}

}  // namespace protocol
}  // namespace remoting
