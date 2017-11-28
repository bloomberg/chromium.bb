// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_authenticator_base.h"

#include <utility>

#include "base/bind.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/channel_authenticator.h"

namespace remoting {
namespace protocol {

namespace {
const buzz::StaticQName kPairingFailedTag =
    { kChromotingXmlNamespace, "pairing-failed" };
const buzz::StaticQName kPairingErrorAttribute = { "", "error" };
}  // namespace

PairingAuthenticatorBase::PairingAuthenticatorBase() : weak_factory_(this) {}
PairingAuthenticatorBase::~PairingAuthenticatorBase() = default;

Authenticator::State PairingAuthenticatorBase::state() const {
  DCHECK(spake2_authenticator_);
  return spake2_authenticator_->state();
}

bool PairingAuthenticatorBase::started() const {
  if (!spake2_authenticator_) {
    return false;
  }
  return spake2_authenticator_->started();
}

Authenticator::RejectionReason
PairingAuthenticatorBase::rejection_reason() const {
  if (!spake2_authenticator_) {
    return PROTOCOL_ERROR;
  }
  return spake2_authenticator_->rejection_reason();
}

void PairingAuthenticatorBase::ProcessMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  DCHECK_EQ(state(), WAITING_MESSAGE);

  // The client authenticator creates the underlying authenticator in the ctor
  // and the host creates it in response to the first message before deferring
  // to this class to process it. Either way, it should exist here.
  DCHECK(spake2_authenticator_);

  // If pairing failed, and we haven't already done so, try again with the PIN.
  if (using_paired_secret_ && HasErrorMessage(message)) {
    using_paired_secret_ = false;
    spake2_authenticator_.reset();
    CreateSpakeAuthenticatorWithPin(
        WAITING_MESSAGE, base::Bind(&PairingAuthenticatorBase::ProcessMessage,
                                    weak_factory_.GetWeakPtr(),
                                    base::Owned(new buzz::XmlElement(*message)),
                                    resume_callback));
    return;
  }

  // Pass the message to the underlying authenticator for processing, but
  // check for a failed SPAKE exchange if we're using the paired secret. In
  // this case the pairing protocol can continue by communicating the error
  // to the peer and retrying with the PIN.
  spake2_authenticator_->ProcessMessage(
      message,
      base::Bind(&PairingAuthenticatorBase::CheckForFailedSpakeExchange,
                 weak_factory_.GetWeakPtr(), resume_callback));
}

std::unique_ptr<buzz::XmlElement> PairingAuthenticatorBase::GetNextMessage() {
  DCHECK_EQ(state(), MESSAGE_READY);
  std::unique_ptr<buzz::XmlElement> result =
      spake2_authenticator_->GetNextMessage();
  MaybeAddErrorMessage(result.get());
  return result;
}

const std::string& PairingAuthenticatorBase::GetAuthKey() const {
  return spake2_authenticator_->GetAuthKey();
}

std::unique_ptr<ChannelAuthenticator>
PairingAuthenticatorBase::CreateChannelAuthenticator() const {
  return spake2_authenticator_->CreateChannelAuthenticator();
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
  return pairing_failed_tag != nullptr;
}

void PairingAuthenticatorBase::CheckForFailedSpakeExchange(
    const base::Closure& resume_callback) {
  // If the SPAKE exchange failed due to invalid credentials, and those
  // credentials were the paired secret, then notify the peer that the
  // PIN-less connection failed and retry using the PIN.
  if (spake2_authenticator_->state() == REJECTED &&
      spake2_authenticator_->rejection_reason() == INVALID_CREDENTIALS &&
      using_paired_secret_) {
    using_paired_secret_ = false;
    error_message_ = "invalid-shared-secret";
    spake2_authenticator_.reset();
    CreateSpakeAuthenticatorWithPin(MESSAGE_READY, resume_callback);
    return;
  }

  resume_callback.Run();
}

}  // namespace protocol
}  // namespace remoting
