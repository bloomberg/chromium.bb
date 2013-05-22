// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_host_authenticator.h"

#include "base/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/v2_authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

PairingHostAuthenticator::PairingHostAuthenticator(
    scoped_refptr<PairingRegistry> pairing_registry,
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& pin)
    : pairing_registry_(pairing_registry),
      local_cert_(local_cert),
      key_pair_(key_pair),
      pin_(pin),
      protocol_error_(false),
      weak_factory_(this) {
}

PairingHostAuthenticator::~PairingHostAuthenticator() {
}

Authenticator::State PairingHostAuthenticator::state() const {
  if (protocol_error_) {
    return REJECTED;
  } else if (!v2_authenticator_) {
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

void PairingHostAuthenticator::CreateV2AuthenticatorWithPIN(
    State initial_state,
    const SetAuthenticatorCallback& callback) {
  callback.Run(V2Authenticator::CreateForHost(
      local_cert_, key_pair_, pin_, initial_state));
}

void PairingHostAuthenticator::ProcessMessage(
    const buzz::XmlElement* message,
    const base::Closure& resume_callback) {
  if (!v2_authenticator_) {
    std::string client_id;
    std::string paired_secret;

    const buzz::XmlElement* pairing_tag = message->FirstNamed(kPairingInfoTag);
    if (pairing_tag) {
      client_id = pairing_tag->Attr(kClientIdAttribute);
    }

    if (client_id.empty()) {
      LOG(ERROR) << "No client id specified.";
      protocol_error_ = true;
    } else {
      paired_secret = pairing_registry_->GetSecret(client_id);
      if (paired_secret.empty()) {
        LOG(INFO) << "Unknown client id";
        error_message_ = "unknown-client-id";
      }
    }

    using_paired_secret_ = !paired_secret.empty();
    if (using_paired_secret_) {
      v2_authenticator_  = V2Authenticator::CreateForHost(
          local_cert_, key_pair_, paired_secret, WAITING_MESSAGE);
    } else {
      v2_authenticator_ = V2Authenticator::CreateForHost(
          local_cert_, key_pair_, pin_, MESSAGE_READY);
      // The client's optimistic SPAKE message is using a Paired Secret to
      // which the host doesn't have access, so don't bother processing it.
      resume_callback.Run();
      return;
    }
  }

  PairingAuthenticatorBase::ProcessMessage(message, resume_callback);
}

void PairingHostAuthenticator::AddPairingElements(buzz::XmlElement* message) {
  // Nothing to do here
}

}  // namespace protocol
}  // namespace remoting
