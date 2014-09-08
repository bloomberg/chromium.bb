// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/negotiating_authenticator_base.h"

#include <algorithm>
#include <sstream>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/v2_authenticator.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

const buzz::StaticQName NegotiatingAuthenticatorBase::kMethodAttributeQName =
    { "", "method" };
const buzz::StaticQName
NegotiatingAuthenticatorBase::kSupportedMethodsAttributeQName =
    { "", "supported-methods" };
const char NegotiatingAuthenticatorBase::kSupportedMethodsSeparator = ',';

NegotiatingAuthenticatorBase::NegotiatingAuthenticatorBase(
    Authenticator::State initial_state)
    : current_method_(AuthenticationMethod::Invalid()),
      state_(initial_state),
      rejection_reason_(INVALID_CREDENTIALS) {
}

NegotiatingAuthenticatorBase::~NegotiatingAuthenticatorBase() {
}

Authenticator::State NegotiatingAuthenticatorBase::state() const {
  return state_;
}

bool NegotiatingAuthenticatorBase::started() const {
  if (!current_authenticator_) {
    return false;
  }
  return current_authenticator_->started();
}

Authenticator::RejectionReason
NegotiatingAuthenticatorBase::rejection_reason() const {
  return rejection_reason_;
}

void NegotiatingAuthenticatorBase::ProcessMessageInternal(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  if (current_authenticator_->state() == WAITING_MESSAGE) {
    // If the message was not discarded and the authenticator is waiting for it,
    // give it to the underlying authenticator to process.
    // |current_authenticator_| is owned, so Unretained() is safe here.
    state_ = PROCESSING_MESSAGE;
    current_authenticator_->ProcessMessage(message, base::Bind(
        &NegotiatingAuthenticatorBase::UpdateState,
        base::Unretained(this), resume_callback));
  } else {
    // Otherwise, just discard the message and run the callback.
    resume_callback.Run();
  }
}

void NegotiatingAuthenticatorBase::UpdateState(
    const base::Closure& resume_callback) {
  // After the underlying authenticator finishes processing the message, the
  // NegotiatingAuthenticatorBase must update its own state before running the
  // |resume_callback| to resume the session negotiation.
  state_ = current_authenticator_->state();
  if (state_ == REJECTED)
    rejection_reason_ = current_authenticator_->rejection_reason();
  resume_callback.Run();
}

scoped_ptr<buzz::XmlElement>
NegotiatingAuthenticatorBase::GetNextMessageInternal() {
  DCHECK_EQ(state(), MESSAGE_READY);
  DCHECK(current_method_.is_valid());

  scoped_ptr<buzz::XmlElement> result;
  if (current_authenticator_->state() == MESSAGE_READY) {
    result = current_authenticator_->GetNextMessage();
  } else {
    result = CreateEmptyAuthenticatorMessage();
  }
  state_ = current_authenticator_->state();
  DCHECK(state_ == ACCEPTED || state_ == WAITING_MESSAGE);
  result->AddAttr(kMethodAttributeQName, current_method_.ToString());
  return result.Pass();
}

void NegotiatingAuthenticatorBase::AddMethod(
    const AuthenticationMethod& method) {
  DCHECK(method.is_valid());
  methods_.push_back(method);
}

scoped_ptr<ChannelAuthenticator>
NegotiatingAuthenticatorBase::CreateChannelAuthenticator() const {
  DCHECK_EQ(state(), ACCEPTED);
  return current_authenticator_->CreateChannelAuthenticator();
}

}  // namespace protocol
}  // namespace remoting
