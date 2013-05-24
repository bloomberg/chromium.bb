// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/negotiating_client_authenticator.h"

#include <algorithm>
#include <sstream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/pairing_client_authenticator.h"
#include "remoting/protocol/v2_authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

NegotiatingClientAuthenticator::NegotiatingClientAuthenticator(
    const std::string& client_pairing_id,
    const std::string& shared_secret,
    const std::string& authentication_tag,
    const FetchSecretCallback& fetch_secret_callback,
    scoped_ptr<ThirdPartyClientAuthenticator::TokenFetcher> token_fetcher,
    const std::vector<AuthenticationMethod>& methods)
    : NegotiatingAuthenticatorBase(MESSAGE_READY),
      client_pairing_id_(client_pairing_id),
      shared_secret_(shared_secret),
      authentication_tag_(authentication_tag),
      fetch_secret_callback_(fetch_secret_callback),
      token_fetcher_(token_fetcher.Pass()),
      method_set_by_host_(false),
      weak_factory_(this) {
  DCHECK(!methods.empty());
  for (std::vector<AuthenticationMethod>::const_iterator it = methods.begin();
       it != methods.end(); ++it) {
    AddMethod(*it);
  }
}

NegotiatingClientAuthenticator::~NegotiatingClientAuthenticator() {
}

void NegotiatingClientAuthenticator::ProcessMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  DCHECK_EQ(state(), WAITING_MESSAGE);

  std::string method_attr = message->Attr(kMethodAttributeQName);
  AuthenticationMethod method = AuthenticationMethod::FromString(method_attr);

  // The host picked a method different from the one the client had selected.
  if (method != current_method_) {
    // The host must pick a method that is valid and supported by the client,
    // and it must not change methods after it has picked one.
    if (method_set_by_host_ || !method.is_valid() ||
        std::find(methods_.begin(), methods_.end(), method) == methods_.end()) {
      state_ = REJECTED;
      rejection_reason_ = PROTOCOL_ERROR;
      resume_callback.Run();
      return;
    }

    current_method_ = method;
    method_set_by_host_ = true;
    state_ = PROCESSING_MESSAGE;

    // Copy the message since the authenticator may process it asynchronously.
    base::Closure callback = base::Bind(
        &NegotiatingAuthenticatorBase::ProcessMessageInternal,
        base::Unretained(this), base::Owned(new buzz::XmlElement(*message)),
        resume_callback);
    CreateAuthenticatorForCurrentMethod(WAITING_MESSAGE, callback);
    return;
  }
  ProcessMessageInternal(message, resume_callback);
}

scoped_ptr<buzz::XmlElement> NegotiatingClientAuthenticator::GetNextMessage() {
  DCHECK_EQ(state(), MESSAGE_READY);

  // This is the first message to the host, send a list of supported methods.
  if (!current_method_.is_valid()) {
    // If no authentication method has been chosen, see if we can optimistically
    // choose one.
    scoped_ptr<buzz::XmlElement> result;
    CreatePreferredAuthenticator();
    if (current_authenticator_) {
      DCHECK(current_authenticator_->state() == MESSAGE_READY);
      result = GetNextMessageInternal();
    } else {
      result = CreateEmptyAuthenticatorMessage();
    }

    // Include a list of supported methods.
    std::stringstream supported_methods(std::stringstream::out);
    for (std::vector<AuthenticationMethod>::iterator it = methods_.begin();
         it != methods_.end(); ++it) {
      if (it != methods_.begin())
        supported_methods << kSupportedMethodsSeparator;
      supported_methods << it->ToString();
    }
    result->AddAttr(kSupportedMethodsAttributeQName, supported_methods.str());
    state_ = WAITING_MESSAGE;
    return result.Pass();
  }
  return GetNextMessageInternal();
}

void NegotiatingClientAuthenticator::CreateAuthenticatorForCurrentMethod(
    Authenticator::State preferred_initial_state,
    const base::Closure& resume_callback) {
  DCHECK(current_method_.is_valid());
  if (current_method_.type() == AuthenticationMethod::THIRD_PARTY) {
    // |ThirdPartyClientAuthenticator| takes ownership of |token_fetcher_|.
    // The authentication method negotiation logic should guarantee that only
    // one |ThirdPartyClientAuthenticator| will need to be created per session.
    DCHECK(token_fetcher_);
    current_authenticator_.reset(new ThirdPartyClientAuthenticator(
        token_fetcher_.Pass()));
    resume_callback.Run();
  } else {
    DCHECK(current_method_.type() == AuthenticationMethod::SPAKE2 ||
           current_method_.type() == AuthenticationMethod::SPAKE2_PAIR);
    bool pairing_supported =
        (current_method_.type() == AuthenticationMethod::SPAKE2_PAIR);
    SecretFetchedCallback callback = base::Bind(
        &NegotiatingClientAuthenticator::CreateV2AuthenticatorWithSecret,
        weak_factory_.GetWeakPtr(), preferred_initial_state, resume_callback);
    fetch_secret_callback_.Run(pairing_supported, callback);
  }
}

void NegotiatingClientAuthenticator::CreatePreferredAuthenticator() {
  if (!client_pairing_id_.empty() && !shared_secret_.empty() &&
      std::find(methods_.begin(), methods_.end(),
                AuthenticationMethod::Spake2Pair()) != methods_.end()) {
    // If the client specified a pairing id and shared secret, then create a
    // PairingAuthenticator.
    current_authenticator_.reset(new PairingClientAuthenticator(
        client_pairing_id_, shared_secret_, fetch_secret_callback_,
        authentication_tag_));
    current_method_ = AuthenticationMethod::Spake2Pair();
  }
}

void NegotiatingClientAuthenticator::CreateV2AuthenticatorWithSecret(
    Authenticator::State initial_state,
    const base::Closure& resume_callback,
    const std::string& shared_secret) {
  current_authenticator_ = V2Authenticator::CreateForClient(
      AuthenticationMethod::ApplyHashFunction(
          current_method_.hash_function(), authentication_tag_, shared_secret),
      initial_state);
  resume_callback.Run();
}

}  // namespace protocol
}  // namespace remoting
