// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_client_authenticator.h"

#include "base/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

PairingClientAuthenticator::PairingClientAuthenticator(
    const ClientAuthenticationConfig& client_auth_config,
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback)
    : client_auth_config_(client_auth_config),
      create_base_authenticator_callback_(create_base_authenticator_callback),
      weak_factory_(this) {
  spake2_authenticator_ = create_base_authenticator_callback_.Run(
      client_auth_config.pairing_secret, MESSAGE_READY);
  using_paired_secret_ = true;
}

PairingClientAuthenticator::~PairingClientAuthenticator() {}

Authenticator::State PairingClientAuthenticator::state() const {
  if (waiting_for_pin_)
    return PROCESSING_MESSAGE;
  return PairingAuthenticatorBase::state();
}

void PairingClientAuthenticator::CreateSpakeAuthenticatorWithPin(
    State initial_state,
    const base::Closure& resume_callback) {
  DCHECK(!waiting_for_pin_);
  waiting_for_pin_ = true;
  client_auth_config_.fetch_secret_callback.Run(
      true,
      base::Bind(&PairingClientAuthenticator::OnPinFetched,
                 weak_factory_.GetWeakPtr(), initial_state, resume_callback));
}

void PairingClientAuthenticator::AddPairingElements(buzz::XmlElement* message) {
  // If the client id and secret have not yet been sent, do so now. Note that
  // in this case the V2Authenticator is being used optimistically to send the
  // first message of the SPAKE exchange since we don't yet know whether or not
  // the host will accept the client id or request that we fall back to the PIN.
  if (!sent_client_id_) {
    buzz::XmlElement* pairing_tag = new buzz::XmlElement(kPairingInfoTag);
    pairing_tag->AddAttr(kClientIdAttribute,
                         client_auth_config_.pairing_client_id);
    message->AddElement(pairing_tag);
    sent_client_id_ = true;
  }
}

void PairingClientAuthenticator::OnPinFetched(
    State initial_state,
    const base::Closure& resume_callback,
    const std::string& pin) {
  DCHECK(waiting_for_pin_);
  DCHECK(!spake2_authenticator_);
  waiting_for_pin_ = false;
  spake2_authenticator_ = create_base_authenticator_callback_.Run(
      GetSharedSecretHash(client_auth_config_.host_id, pin), initial_state);
  resume_callback.Run();
}

}  // namespace protocol
}  // namespace remoting
