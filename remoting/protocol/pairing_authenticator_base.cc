// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_authenticator_base.h"

#include "base/bind.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/channel_authenticator.h"

namespace remoting {
namespace protocol {

const buzz::StaticQName PairingAuthenticatorBase::kPairingInfoTag =
    { kChromotingXmlNamespace, "pairing-info" };
const buzz::StaticQName PairingAuthenticatorBase::kClientIdAttribute =
    { "", "client-id" };

namespace {
const buzz::StaticQName kPairingFailedTag =
    { kChromotingXmlNamespace, "pairing-failed" };
const buzz::StaticQName kPairingErrorAttribute = { "", "error" };
}  // namespace

PairingAuthenticatorBase::PairingAuthenticatorBase()
    : using_paired_secret_(false),
      waiting_for_authenticator_(false),
      weak_factory_(this) {
}

PairingAuthenticatorBase::~PairingAuthenticatorBase() {
}

Authenticator::State PairingAuthenticatorBase::state() const {
  if (waiting_for_authenticator_) {
    return PROCESSING_MESSAGE;
  }
  return v2_authenticator_->state();
}

bool PairingAuthenticatorBase::started() const {
  if (!v2_authenticator_) {
    return false;
  }
  return v2_authenticator_->started();
}

Authenticator::RejectionReason
PairingAuthenticatorBase::rejection_reason() const {
  if (!v2_authenticator_) {
    return PROTOCOL_ERROR;
  }
  return v2_authenticator_->rejection_reason();
}

void PairingAuthenticatorBase::ProcessMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  DCHECK_EQ(state(), WAITING_MESSAGE);

  // The client authenticator creates the underlying authenticator in the ctor
  // and the host creates it in response to the first message before deferring
  // to this class to process it. Either way, it should exist here.
  DCHECK(v2_authenticator_);

  // If pairing failed, and we haven't already done so, try again with the PIN.
  if (using_paired_secret_ && HasErrorMessage(message)) {
    using_paired_secret_ = false;
    waiting_for_authenticator_ = true;
    v2_authenticator_.reset();
    SetAuthenticatorCallback set_authenticator = base::Bind(
        &PairingAuthenticatorBase::SetAuthenticatorAndProcessMessage,
        weak_factory_.GetWeakPtr(), base::Owned(new buzz::XmlElement(*message)),
        resume_callback);
    CreateV2AuthenticatorWithPIN(WAITING_MESSAGE, set_authenticator);
    return;
  }

  // Pass the message to the underlying authenticator for processing, but
  // check for a failed SPAKE exchange if we're using the paired secret. In
  // this case the pairing protocol can continue by communicating the error
  // to the peer and retrying with the PIN.
  v2_authenticator_->ProcessMessage(
      message,
      base::Bind(&PairingAuthenticatorBase::CheckForFailedSpakeExchange,
                 weak_factory_.GetWeakPtr(), resume_callback));
}

scoped_ptr<buzz::XmlElement> PairingAuthenticatorBase::GetNextMessage() {
  DCHECK_EQ(state(), MESSAGE_READY);
  scoped_ptr<buzz::XmlElement> result = v2_authenticator_->GetNextMessage();
  AddPairingElements(result.get());
  MaybeAddErrorMessage(result.get());
  return result.Pass();
}

scoped_ptr<ChannelAuthenticator>
PairingAuthenticatorBase::CreateChannelAuthenticator() const {
  return v2_authenticator_->CreateChannelAuthenticator();
}

void PairingAuthenticatorBase::MaybeAddErrorMessage(buzz::XmlElement* message) {
  if (!error_message_.empty()) {
    buzz::XmlElement* pairing_failed_tag =
        new buzz::XmlElement(kPairingFailedTag);
    pairing_failed_tag->AddAttr(kPairingErrorAttribute, error_message_);
    message->AddElement(pairing_failed_tag);
    error_message_.clear();
  }
}

bool PairingAuthenticatorBase::HasErrorMessage(
    const buzz::XmlElement* message) const {
  const buzz::XmlElement* pairing_failed_tag =
      message->FirstNamed(kPairingFailedTag);
  if (pairing_failed_tag) {
    std::string error = pairing_failed_tag->Attr(kPairingErrorAttribute);
    LOG(ERROR) << "Pairing failed: " << error;
  }
  return pairing_failed_tag != NULL;
}

void PairingAuthenticatorBase::CheckForFailedSpakeExchange(
    const base::Closure& resume_callback) {
  // If the SPAKE exchange failed due to invalid credentials, and those
  // credentials were the paired secret, then notify the peer that the
  // PIN-less connection failed and retry using the PIN.
  if (v2_authenticator_->state() == REJECTED &&
      v2_authenticator_->rejection_reason() == INVALID_CREDENTIALS &&
      using_paired_secret_) {
    using_paired_secret_ = false;
    error_message_ = "invalid-shared-secret";
    v2_authenticator_.reset();
    buzz::XmlElement* no_message = NULL;
    SetAuthenticatorCallback set_authenticator = base::Bind(
        &PairingAuthenticatorBase::SetAuthenticatorAndProcessMessage,
        weak_factory_.GetWeakPtr(), no_message, resume_callback);
    CreateV2AuthenticatorWithPIN(MESSAGE_READY, set_authenticator);
    return;
  }

  resume_callback.Run();
}

void PairingAuthenticatorBase::SetAuthenticatorAndProcessMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback,
    scoped_ptr<Authenticator> authenticator) {
  DCHECK(!v2_authenticator_);
  DCHECK(authenticator);
  waiting_for_authenticator_ = false;
  v2_authenticator_ = authenticator.Pass();
  if (message) {
    ProcessMessage(message, resume_callback);
  } else {
    resume_callback.Run();
  }
}

}  // namespace protocol
}  // namespace remoting
