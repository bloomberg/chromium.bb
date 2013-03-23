// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/third_party_client_authenticator.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/v2_authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

ThirdPartyClientAuthenticator::ThirdPartyClientAuthenticator(
    const std::string& host_public_key,
    scoped_ptr<TokenFetcher> token_fetcher)
    : ThirdPartyAuthenticatorBase(WAITING_MESSAGE),
      host_public_key_(host_public_key),
      token_fetcher_(token_fetcher.Pass()) {
}

ThirdPartyClientAuthenticator::~ThirdPartyClientAuthenticator() {
}

void ThirdPartyClientAuthenticator::ProcessTokenMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  std::string token_url = message->TextNamed(kTokenUrlTag);
  std::string token_scope = message->TextNamed(kTokenScopeTag);

  if (token_url.empty() || token_scope.empty()) {
    LOG(ERROR) << "Third-party authentication protocol error: "
        "missing token verification URL or scope.";
    token_state_ = REJECTED;
    rejection_reason_ = PROTOCOL_ERROR;
    resume_callback.Run();
    return;
  }

  token_state_ = PROCESSING_MESSAGE;

  // |token_fetcher_| is owned, so Unretained() is safe here.
  token_fetcher_->FetchThirdPartyToken(
      GURL(token_url), host_public_key_, token_scope, base::Bind(
          &ThirdPartyClientAuthenticator::OnThirdPartyTokenFetched,
          base::Unretained(this), resume_callback));
}

void ThirdPartyClientAuthenticator::AddTokenElements(
    buzz::XmlElement* message) {
  DCHECK_EQ(token_state_, MESSAGE_READY);
  DCHECK(!token_.empty());

  buzz::XmlElement* token_tag = new buzz::XmlElement(kTokenTag);
  token_tag->SetBodyText(token_);
  message->AddElement(token_tag);
  token_state_ = ACCEPTED;
}

void ThirdPartyClientAuthenticator::OnThirdPartyTokenFetched(
    const base::Closure& resume_callback,
    const std::string& third_party_token,
    const std::string& shared_secret) {
  token_ = third_party_token;
  if (token_.empty() || shared_secret.empty()) {
    token_state_ = REJECTED;
    rejection_reason_ = INVALID_CREDENTIALS;
  } else {
    token_state_ = MESSAGE_READY;
    underlying_ = V2Authenticator::CreateForClient(
        shared_secret, MESSAGE_READY);
  }
  resume_callback.Run();
}

}  // namespace protocol
}  // namespace remoting
