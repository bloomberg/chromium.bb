// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/v1_authenticator.h"

#include "base/base64.h"
#include "base/logging.h"
#include "crypto/rsa_private_key.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/ssl_hmac_channel_authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {
namespace protocol {

namespace {
const char kAuthTokenTag[] = "auth-token";
const char kCertificateTag[] = "certificate";
}  // namespace

V1ClientAuthenticator::V1ClientAuthenticator(
    const std::string& local_jid,
    const std::string& shared_secret)
    : local_jid_(local_jid),
      shared_secret_(shared_secret),
      state_(MESSAGE_READY) {
}

V1ClientAuthenticator::~V1ClientAuthenticator() {
}

Authenticator::State V1ClientAuthenticator::state() const {
  return state_;
}

void V1ClientAuthenticator::ProcessMessage(const XmlElement* message) {
  DCHECK_EQ(state_, WAITING_MESSAGE);

  // Parse the certificate.
  const XmlElement* cert_tag =
      message->FirstNamed(QName(kChromotingXmlNamespace, kCertificateTag));
  if (cert_tag) {
    std::string base64_cert = cert_tag->BodyText();
    if (!base::Base64Decode(base64_cert, &remote_cert_)) {
      LOG(ERROR) << "Failed to decode certificate received from the peer.";
      remote_cert_.clear();
    }
  }

  if (remote_cert_.empty()) {
    state_ = REJECTED;
  } else {
    state_ = ACCEPTED;
  }
}

XmlElement* V1ClientAuthenticator::GetNextMessage() {
  DCHECK_EQ(state_, MESSAGE_READY);

  XmlElement* message = CreateEmptyAuthenticatorMessage();
  std::string token =
      protocol::GenerateSupportAuthToken(local_jid_, shared_secret_);
  XmlElement* auth_token_tag = new XmlElement(
      QName(kChromotingXmlNamespace, kAuthTokenTag));
  auth_token_tag->SetBodyText(token);
  message->AddElement(auth_token_tag);

  state_ = WAITING_MESSAGE;
  return message;
}

ChannelAuthenticator*
V1ClientAuthenticator::CreateChannelAuthenticator() const {
  DCHECK_EQ(state_, ACCEPTED);
  SslHmacChannelAuthenticator* result =
      SslHmacChannelAuthenticator::CreateForClient(
          remote_cert_, shared_secret_);
  result->SetLegacyOneWayMode(SslHmacChannelAuthenticator::SEND_ONLY);
  return result;
};

V1HostAuthenticator::V1HostAuthenticator(
    const std::string& local_cert,
    const crypto::RSAPrivateKey& local_private_key,
    const std::string& shared_secret,
    const std::string& remote_jid)
    : local_cert_(local_cert),
      local_private_key_(local_private_key.Copy()),
      shared_secret_(shared_secret),
      remote_jid_(remote_jid),
      state_(WAITING_MESSAGE) {
}

V1HostAuthenticator::~V1HostAuthenticator() {
}

Authenticator::State V1HostAuthenticator::state() const {
  return state_;
}

void V1HostAuthenticator::ProcessMessage(const XmlElement* message) {
  DCHECK_EQ(state_, WAITING_MESSAGE);

  std::string auth_token =
      message->TextNamed(buzz::QName(kChromotingXmlNamespace, kAuthTokenTag));

  if (!protocol::VerifySupportAuthToken(
          remote_jid_, shared_secret_, auth_token)) {
    state_ = REJECTED;
  } else {
    state_ = MESSAGE_READY;
  }
}

XmlElement* V1HostAuthenticator::GetNextMessage() {
  DCHECK_EQ(state_, MESSAGE_READY);

  XmlElement* message = CreateEmptyAuthenticatorMessage();
  buzz::XmlElement* certificate_tag = new XmlElement(
      buzz::QName(kChromotingXmlNamespace, kCertificateTag));
  std::string base64_cert;
  if (!base::Base64Encode(local_cert_, &base64_cert)) {
    LOG(DFATAL) << "Cannot perform base64 encode on certificate";
  }
  certificate_tag->SetBodyText(base64_cert);
  message->AddElement(certificate_tag);

  state_ = ACCEPTED;
  return message;
}

ChannelAuthenticator*
V1HostAuthenticator::CreateChannelAuthenticator() const {
  DCHECK_EQ(state_, ACCEPTED);
  SslHmacChannelAuthenticator* result =
      SslHmacChannelAuthenticator::CreateForHost(
          local_cert_, local_private_key_.get(), shared_secret_);
  result->SetLegacyOneWayMode(SslHmacChannelAuthenticator::RECEIVE_ONLY);
  return result;
};

}  // namespace remoting
}  // namespace protocol
