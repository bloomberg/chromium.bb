// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_authenticator.h"

#include "base/compiler_specific.h"
#include "base/string_piece.h"
#include "crypto/hmac.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/ssl_socket.h"
#include "net/socket/stream_socket.h"

namespace remoting {
namespace protocol {

namespace {

// Labels for use when exporting the SSL master keys.
const char kClientSslExporterLabel[] = "EXPORTER-remoting-channel-auth-client";

// Size of the HMAC-SHA-256 authentication digest.
const size_t kAuthDigestLength = 32;

// static
bool GetAuthBytes(const std::string& shared_secret,
                  const std::string& key_material,
                  std::string* auth_bytes) {
  // Generate auth digest based on the keying material and shared secret.
  crypto::HMAC response(crypto::HMAC::SHA256);
  if (!response.Init(key_material)) {
    NOTREACHED() << "HMAC::Init failed";
    return false;
  }
  unsigned char out_bytes[kAuthDigestLength];
  if (!response.Sign(shared_secret, out_bytes, kAuthDigestLength)) {
    NOTREACHED() << "HMAC::Sign failed";
    return false;
  }

  auth_bytes->assign(out_bytes, out_bytes + kAuthDigestLength);
  return true;
}

}  // namespace

HostChannelAuthenticator::HostChannelAuthenticator(
    const std::string& shared_secret)
    : shared_secret_(shared_secret),
      socket_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(auth_read_callback_(
          this, &HostChannelAuthenticator::OnAuthBytesRead)) {
}

HostChannelAuthenticator::~HostChannelAuthenticator() {
}

void HostChannelAuthenticator::Authenticate(net::SSLSocket* socket,
                                            const DoneCallback& done_callback) {
  DCHECK(CalledOnValidThread());

  socket_ = socket;
  done_callback_ = done_callback;

  unsigned char key_material[kAuthDigestLength];
  int result = socket_->ExportKeyingMaterial(
      kClientSslExporterLabel, "", key_material, kAuthDigestLength);
  if (result != net::OK) {
    LOG(ERROR) << "Error fetching keying material: " << result;
    done_callback.Run(FAILURE);
    return;
  }

  if (!GetAuthBytes(shared_secret_,
                    std::string(key_material, key_material + kAuthDigestLength),
                    &auth_bytes_)) {
    done_callback.Run(FAILURE);
    return;
  }

  // Read an authentication digest.
  auth_read_buf_ = new net::GrowableIOBuffer();
  auth_read_buf_->SetCapacity(kAuthDigestLength);
  DoAuthRead();
}

void HostChannelAuthenticator::DoAuthRead() {
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

void HostChannelAuthenticator::OnAuthBytesRead(int result) {
  DCHECK(CalledOnValidThread());

  if (HandleAuthBytesRead(result))
    DoAuthRead();
}

bool HostChannelAuthenticator::HandleAuthBytesRead(int read_result) {
  if (read_result <= 0) {
    LOG(ERROR) << "Error reading authentication: " << read_result;
    done_callback_.Run(FAILURE);
    return false;
  }

  auth_read_buf_->set_offset(auth_read_buf_->offset() + read_result);
  if (auth_read_buf_->RemainingCapacity() > 0)
    return true;

  if (!VerifyAuthBytes(std::string(
          auth_read_buf_->StartOfBuffer(),
          auth_read_buf_->StartOfBuffer() + kAuthDigestLength))) {
    LOG(ERROR) << "Mismatched authentication";
    done_callback_.Run(FAILURE);
    return false;
  }

  done_callback_.Run(SUCCESS);
  return false;
}

bool HostChannelAuthenticator::VerifyAuthBytes(
    const std::string& received_auth_bytes) {
  DCHECK(received_auth_bytes.length() == kAuthDigestLength);

  // Compare the received and expected digests in fixed time, to limit the
  // scope for timing attacks.
  uint8 result = 0;
  for (unsigned i = 0; i < auth_bytes_.length(); i++) {
    result |= received_auth_bytes[i] ^ auth_bytes_[i];
  }
  return result == 0;
}

ClientChannelAuthenticator::ClientChannelAuthenticator(
    const std::string& shared_secret)
    : shared_secret_(shared_secret),
socket_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(auth_write_callback_(
        this, &ClientChannelAuthenticator::OnAuthBytesWritten)) {
}

ClientChannelAuthenticator::~ClientChannelAuthenticator() {
}

void ClientChannelAuthenticator::Authenticate(
    net::SSLSocket* socket,
    const DoneCallback& done_callback) {
  DCHECK(CalledOnValidThread());

  socket_ = socket;
  done_callback_ = done_callback;

  unsigned char key_material[kAuthDigestLength];
  int result = socket_->ExportKeyingMaterial(
      kClientSslExporterLabel, "", key_material, kAuthDigestLength);
  if (result != net::OK) {
    LOG(ERROR) << "Error fetching keying material: " << result;
    done_callback.Run(FAILURE);
    return;
  }

  std::string auth_bytes;
  if (!GetAuthBytes(shared_secret_,
                    std::string(key_material, key_material + kAuthDigestLength),
                    &auth_bytes)) {
    done_callback.Run(FAILURE);
    return;
  }

  // Allocate a buffer to write the authentication digest.
  auth_write_buf_ = new net::DrainableIOBuffer(
      new net::StringIOBuffer(auth_bytes), auth_bytes.size());
  DoAuthWrite();
}

void ClientChannelAuthenticator::DoAuthWrite() {
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

void ClientChannelAuthenticator::OnAuthBytesWritten(int result) {
  DCHECK(CalledOnValidThread());

  if (HandleAuthBytesWritten(result))
    DoAuthWrite();
}

bool ClientChannelAuthenticator::HandleAuthBytesWritten(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Error writing authentication: " << result;
    done_callback_.Run(FAILURE);
    return false;
  }

  auth_write_buf_->DidConsume(result);
  if (auth_write_buf_->BytesRemaining() > 0)
    return true;

  done_callback_.Run(SUCCESS);
  return false;
}

}  // namespace protocol
}  // namespace remoting
