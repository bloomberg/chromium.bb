// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/third_party_host_authenticator.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/v2_authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

ThirdPartyHostAuthenticator::ThirdPartyHostAuthenticator(
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    scoped_ptr<TokenValidator> token_validator)
    : ThirdPartyAuthenticatorBase(MESSAGE_READY),
      local_cert_(local_cert),
      key_pair_(key_pair),
      token_validator_(token_validator.Pass()) {
}

ThirdPartyHostAuthenticator::~ThirdPartyHostAuthenticator() {
}

void ThirdPartyHostAuthenticator::ProcessTokenMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  // Host has already sent the URL and expects a token from the client.
  std::string token = message->TextNamed(kTokenTag);
  if (token.empty()) {
    LOG(ERROR) << "Third-party authentication protocol error: missing token.";
    token_state_ = REJECTED;
    rejection_reason_ = PROTOCOL_ERROR;
    resume_callback.Run();
    return;
  }

  token_state_ = PROCESSING_MESSAGE;

  // This message also contains the client's first SPAKE message. Copy the
  // message into the callback, so that OnThirdPartyTokenValidated can give it
  // to the underlying SPAKE authenticator that will be created.
  // |token_validator_| is owned, so Unretained() is safe here.
  token_validator_->ValidateThirdPartyToken(token, base::Bind(
          &ThirdPartyHostAuthenticator::OnThirdPartyTokenValidated,
          base::Unretained(this),
          base::Owned(new buzz::XmlElement(*message)),
          resume_callback));
}

void ThirdPartyHostAuthenticator::AddTokenElements(
    buzz::XmlElement* message) {
  DCHECK_EQ(token_state_, MESSAGE_READY);
  DCHECK(token_validator_->token_url().is_valid());
  DCHECK(!token_validator_->token_scope().empty());

  buzz::XmlElement* token_url_tag = new buzz::XmlElement(
      kTokenUrlTag);
  token_url_tag->SetBodyText(token_validator_->token_url().spec());
  message->AddElement(token_url_tag);
  buzz::XmlElement* token_scope_tag = new buzz::XmlElement(
      kTokenScopeTag);
  token_scope_tag->SetBodyText(token_validator_->token_scope());
  message->AddElement(token_scope_tag);
  token_state_ = WAITING_MESSAGE;
}

void ThirdPartyHostAuthenticator::OnThirdPartyTokenValidated(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback,
    const std::string& shared_secret) {
  if (shared_secret.empty()) {
    token_state_ = REJECTED;
    rejection_reason_ = INVALID_CREDENTIALS;
    resume_callback.Run();
    return;
  }

  // The other side already started the SPAKE authentication.
  token_state_ = ACCEPTED;
  underlying_ = V2Authenticator::CreateForHost(
      local_cert_, key_pair_, shared_secret, WAITING_MESSAGE);
  underlying_->ProcessMessage(message, resume_callback);
}

}  // namespace protocol
}  // namespace remoting
