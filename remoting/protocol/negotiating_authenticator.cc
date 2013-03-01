// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/negotiating_authenticator.h"

#include <algorithm>
#include <sstream>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "crypto/rsa_private_key.h"
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
bool NegotiatingAuthenticator::IsNegotiableMessage(
    const buzz::XmlElement* message) {
  return message->HasAttr(kSupportedMethodsAttributeQName);
}

// static
scoped_ptr<Authenticator> NegotiatingAuthenticator::CreateForClient(
    const std::string& authentication_tag,
    const std::string& shared_secret,
    const std::vector<AuthenticationMethod>& methods) {
  scoped_ptr<NegotiatingAuthenticator> result(
      new NegotiatingAuthenticator(MESSAGE_READY));
  result->authentication_tag_ = authentication_tag;
  result->shared_secret_ = shared_secret;

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
    const crypto::RSAPrivateKey& local_private_key,
    const std::string& shared_secret_hash,
    AuthenticationMethod::HashFunction hash_function) {
  scoped_ptr<NegotiatingAuthenticator> result(
      new NegotiatingAuthenticator(WAITING_MESSAGE));
  result->local_cert_ = local_cert;
  result->local_private_key_.reset(local_private_key.Copy());
  result->shared_secret_hash_ = shared_secret_hash;

  result->AddMethod(AuthenticationMethod::Spake2(hash_function));

  return scoped_ptr<Authenticator>(result.Pass());
}


NegotiatingAuthenticator::NegotiatingAuthenticator(
    Authenticator::State initial_state)
    : certificate_sent_(false),
      current_method_(AuthenticationMethod::Invalid()),
      state_(initial_state),
      rejection_reason_(INVALID_CREDENTIALS) {
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
    state_ = MESSAGE_READY;
  }

  DCHECK(method.is_valid());

  // Replace current authenticator if the method has changed.
  if (method != current_method_) {
    current_method_ = method;
    CreateAuthenticator(state_);
  }
  if (state_ == WAITING_MESSAGE) {
    // |current_authenticator_| is owned, so Unretained() is safe here.
    current_authenticator_->ProcessMessage(message, base::Bind(
        &NegotiatingAuthenticator::UpdateState,
        base::Unretained(this), resume_callback));
  } else {
    UpdateState(resume_callback);
  }
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

  bool add_supported_methods_attr = false;

  // Initialize current method in case it is not initialized
  // yet. Normally happens only on client.
  if (!current_method_.is_valid()) {
    CHECK(!methods_.empty());

    // Initially try the first method.
    current_method_ = methods_[0];
    CreateAuthenticator(MESSAGE_READY);
    add_supported_methods_attr = true;
  }

  scoped_ptr<buzz::XmlElement> result =
      current_authenticator_->GetNextMessage();
  state_ = current_authenticator_->state();
  DCHECK_NE(state_, REJECTED);

  result->AddAttr(kMethodAttributeQName, current_method_.ToString());

  if (add_supported_methods_attr) {
    std::stringstream supported_methods(std::stringstream::out);
    for (std::vector<AuthenticationMethod>::iterator it = methods_.begin();
         it != methods_.end(); ++it) {
      if (it != methods_.begin())
        supported_methods << kSupportedMethodsSeparator;
      supported_methods << it->ToString();
    }
    result->AddAttr(kSupportedMethodsAttributeQName, supported_methods.str());
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
  return local_private_key_.get() != NULL;
}

void NegotiatingAuthenticator::CreateAuthenticator(State initial_state) {
  if (is_host_side()) {
    current_authenticator_ = V2Authenticator::CreateForHost(
        local_cert_, *local_private_key_.get(),
        shared_secret_hash_, initial_state);
  } else {
    current_authenticator_ = V2Authenticator::CreateForClient(
        AuthenticationMethod::ApplyHashFunction(
            current_method_.hash_function(),
            authentication_tag_, shared_secret_),
        initial_state);
  }
}

}  // namespace protocol
}  // namespace remoting
