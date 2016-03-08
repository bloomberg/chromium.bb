// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_host_authenticator.h"

#include "base/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

PairingHostAuthenticator::PairingHostAuthenticator(
    scoped_refptr<PairingRegistry> pairing_registry,
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
    const std::string& pin)
    : pairing_registry_(pairing_registry),
      create_base_authenticator_callback_(create_base_authenticator_callback),
      pin_(pin),
      weak_factory_(this) {}

PairingHostAuthenticator::~PairingHostAuthenticator() {}

Authenticator::State PairingHostAuthenticator::state() const {
  if (protocol_error_) {
    return REJECTED;
  } else if (waiting_for_paired_secret_) {
    return PROCESSING_MESSAGE;
  } else if (!spake2_authenticator_) {
    return WAITING_MESSAGE;
  }
  return PairingAuthenticatorBase::state();
}

Authenticator::RejectionReason
PairingHostAuthenticator::rejection_reason() const {
  if (protocol_error_) {
    return PROTOCOL_ERROR;
  }
  return PairingAuthenticatorBase::rejection_reason();
}

void PairingHostAuthenticator::CreateSpakeAuthenticatorWithPin(
    State initial_state,
    const base::Closure& resume_callback) {
  spake2_authenticator_ =
      create_base_authenticator_callback_.Run(pin_, initial_state);
  resume_callback.Run();
}

void PairingHostAuthenticator::ProcessMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  if (!spake2_authenticator_) {
    std::string client_id;

    const buzz::XmlElement* pairing_tag = message->FirstNamed(kPairingInfoTag);
    if (pairing_tag) {
      client_id = pairing_tag->Attr(kClientIdAttribute);
    }

    if (client_id.empty()) {
      LOG(ERROR) << "No client id specified.";
      protocol_error_ = true;
      return;
    }

    waiting_for_paired_secret_ = true;
    pairing_registry_->GetPairing(
        client_id,
        base::Bind(&PairingHostAuthenticator::ProcessMessageWithPairing,
                   weak_factory_.GetWeakPtr(),
                   base::Owned(new buzz::XmlElement(*message)),
                   resume_callback));
    return;
  }

  PairingAuthenticatorBase::ProcessMessage(message, resume_callback);
}

void PairingHostAuthenticator::AddPairingElements(buzz::XmlElement* message) {
  // Nothing to do here
}

void PairingHostAuthenticator::ProcessMessageWithPairing(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback,
    PairingRegistry::Pairing pairing) {
  waiting_for_paired_secret_ = false;
  std::string paired_secret = pairing.shared_secret();
  if (paired_secret.empty()) {
    VLOG(0) << "Unknown client id";
    error_message_ = "unknown-client-id";
  }

  using_paired_secret_ = !paired_secret.empty();
  if (using_paired_secret_) {
    spake2_authenticator_ =
        create_base_authenticator_callback_.Run(paired_secret, WAITING_MESSAGE);
    PairingAuthenticatorBase::ProcessMessage(message, resume_callback);
  } else {
    spake2_authenticator_ =
        create_base_authenticator_callback_.Run(pin_, MESSAGE_READY);
    // The client's optimistic SPAKE message is using a Paired Secret to
    // which the host doesn't have access, so don't bother processing it.
    resume_callback.Run();
  }
}

}  // namespace protocol
}  // namespace remoting
