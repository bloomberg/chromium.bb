// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/negotiating_authenticator.h"

#include <algorithm>
#include <sstream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/v2_authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

namespace {

const buzz::StaticQName kMethodAttributeQName = { "", "method" };
const buzz::StaticQName kSupportedMethodsAttributeQName =
    { "", "supported-methods" };

const char kSupportedMethodsSeparator = ',';

}  // namespace

// static
scoped_ptr<Authenticator> NegotiatingAuthenticator::CreateForClient(
    const std::string& authentication_tag,
    const FetchSecretCallback& fetch_secret_callback,
    const std::vector<AuthenticationMethod>& methods) {
  scoped_ptr<NegotiatingAuthenticator> result(
      new NegotiatingAuthenticator(MESSAGE_READY));
  result->authentication_tag_ = authentication_tag;
  result->fetch_secret_callback_ = fetch_secret_callback;

  DCHECK(!methods.empty());
  for (std::vector<AuthenticationMethod>::const_iterator it = methods.begin();
       it != methods.end(); ++it) {
    result->AddMethod(*it);
  }

  return scoped_ptr<Authenticator>(result.Pass());
}

// static
scoped_ptr<Authenticator> NegotiatingAuthenticator::CreateForHost(
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& shared_secret_hash,
    AuthenticationMethod::HashFunction hash_function) {
  scoped_ptr<NegotiatingAuthenticator> result(
      new NegotiatingAuthenticator(WAITING_MESSAGE));
  result->local_cert_ = local_cert;
  result->local_key_pair_ = key_pair;
  result->shared_secret_hash_ = shared_secret_hash;

  result->AddMethod(AuthenticationMethod::Spake2(hash_function));

  return scoped_ptr<Authenticator>(result.Pass());
}

NegotiatingAuthenticator::NegotiatingAuthenticator(
    Authenticator::State initial_state)
    : current_method_(AuthenticationMethod::Invalid()),
      state_(initial_state),
      rejection_reason_(INVALID_CREDENTIALS),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

NegotiatingAuthenticator::~NegotiatingAuthenticator() {
}

Authenticator::State NegotiatingAuthenticator::state() const {
  return state_;
}

Authenticator::RejectionReason
NegotiatingAuthenticator::rejection_reason() const {
  return rejection_reason_;
}

void NegotiatingAuthenticator::ProcessMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  DCHECK_EQ(state(), WAITING_MESSAGE);

  std::string method_attr = message->Attr(kMethodAttributeQName);
  AuthenticationMethod method = AuthenticationMethod::FromString(method_attr);

  // Check if the remote end specified a method that is not supported locally.
  if (method.is_valid() &&
      std::find(methods_.begin(), methods_.end(), method) == methods_.end()) {
    method = AuthenticationMethod::Invalid();
  }

  // If the remote peer did not specify auth method or specified unknown
  // method then select the first known method from the supported-methods
  // attribute.
  if (!method.is_valid()) {
    std::string supported_methods_attr =
        message->Attr(kSupportedMethodsAttributeQName);
    if (supported_methods_attr.empty()) {
      // Message contains neither method nor supported-methods attributes.
      state_ = REJECTED;
      rejection_reason_ = PROTOCOL_ERROR;
      resume_callback.Run();
      return;
    }

    // Find the first mutually-supported method in the peer's list of
    // supported-methods.
    std::vector<std::string> supported_methods_strs;
    base::SplitString(supported_methods_attr, kSupportedMethodsSeparator,
                      &supported_methods_strs);
    for (std::vector<std::string>::iterator it = supported_methods_strs.begin();
         it != supported_methods_strs.end(); ++it) {
      AuthenticationMethod list_value = AuthenticationMethod::FromString(*it);
      if (list_value.is_valid() &&
          std::find(methods_.begin(),
                    methods_.end(), list_value) != methods_.end()) {
        // Found common method.
        method = list_value;
        break;
      }
    }

    if (!method.is_valid()) {
      // Failed to find a common auth method.
      state_ = REJECTED;
      rejection_reason_ = PROTOCOL_ERROR;
      resume_callback.Run();
      return;
    }

    // Drop the current message because we've chosen a different
    // method.
    current_method_ = method;
    state_ = PROCESSING_MESSAGE;
    CreateAuthenticator(MESSAGE_READY, base::Bind(
        &NegotiatingAuthenticator::UpdateState,
        base::Unretained(this), resume_callback));
    return;
  }
  if (method != current_method_) {
    current_method_ = method;
    state_ = PROCESSING_MESSAGE;
    // Copy the message since the authenticator may process it asynchronously.
    CreateAuthenticator(WAITING_MESSAGE, base::Bind(
        &NegotiatingAuthenticator::ProcessMessageInternal,
        base::Unretained(this), base::Owned(new buzz::XmlElement(*message)),
        resume_callback));
    return;
  }
  ProcessMessageInternal(message, resume_callback);
}

void NegotiatingAuthenticator::UpdateState(
    const base::Closure& resume_callback) {
  // After the underlying authenticator finishes processing the message, the
  // NegotiatingAuthenticator must update its own state before running the
  // |resume_callback| to resume the session negotiation.
  state_ = current_authenticator_->state();
  if (state_ == REJECTED)
    rejection_reason_ = current_authenticator_->rejection_reason();
  resume_callback.Run();
}

scoped_ptr<buzz::XmlElement> NegotiatingAuthenticator::GetNextMessage() {
  DCHECK_EQ(state(), MESSAGE_READY);

  scoped_ptr<buzz::XmlElement> result;

  // No method yet, just send a message with the list of supported methods.
  // Normally happens only on client.
  if (!current_method_.is_valid()) {
    result = CreateEmptyAuthenticatorMessage();
    std::stringstream supported_methods(std::stringstream::out);
    for (std::vector<AuthenticationMethod>::iterator it = methods_.begin();
         it != methods_.end(); ++it) {
      if (it != methods_.begin())
        supported_methods << kSupportedMethodsSeparator;
      supported_methods << it->ToString();
    }
    result->AddAttr(kSupportedMethodsAttributeQName, supported_methods.str());
    state_ = WAITING_MESSAGE;
  } else {
    if (current_authenticator_->state() == MESSAGE_READY) {
      result = current_authenticator_->GetNextMessage();
    } else {
      result = CreateEmptyAuthenticatorMessage();
    }
    state_ = current_authenticator_->state();
    DCHECK(state_ == ACCEPTED || state_ == WAITING_MESSAGE);
    result->AddAttr(kMethodAttributeQName, current_method_.ToString());
  }

  return result.Pass();
}

void NegotiatingAuthenticator::AddMethod(const AuthenticationMethod& method) {
  DCHECK(method.is_valid());
  methods_.push_back(method);
}

scoped_ptr<ChannelAuthenticator>
NegotiatingAuthenticator::CreateChannelAuthenticator() const {
  DCHECK_EQ(state(), ACCEPTED);
  return current_authenticator_->CreateChannelAuthenticator();
}

bool NegotiatingAuthenticator::is_host_side() const {
  return local_key_pair_.get() != NULL;
}

void NegotiatingAuthenticator::CreateAuthenticator(
    Authenticator::State preferred_initial_state,
    const base::Closure& resume_callback) {
  if (is_host_side()) {
    current_authenticator_ = V2Authenticator::CreateForHost(
        local_cert_, local_key_pair_, shared_secret_hash_,
        preferred_initial_state);
    resume_callback.Run();
  } else {
    fetch_secret_callback_.Run(base::Bind(
        &NegotiatingAuthenticator::CreateV2AuthenticatorWithSecret,
        weak_factory_.GetWeakPtr(), preferred_initial_state, resume_callback));
  }
}

void NegotiatingAuthenticator::ProcessMessageInternal(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  if (current_authenticator_->state() == WAITING_MESSAGE) {
    // If the message was not discarded and the authenticator is waiting for it,
    // give it to the underlying authenticator to process.
    // |current_authenticator_| is owned, so Unretained() is safe here.
    current_authenticator_->ProcessMessage(message, base::Bind(
        &NegotiatingAuthenticator::UpdateState,
        base::Unretained(this), resume_callback));
  }
}

void NegotiatingAuthenticator::CreateV2AuthenticatorWithSecret(
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
