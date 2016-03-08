// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_client_authenticator.h"

#include "base/bind.h"
#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/channel_authenticator.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

PairingClientAuthenticator::PairingClientAuthenticator(
    const std::string& client_id,
    const std::string& paired_secret,
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
    const FetchSecretCallback& fetch_pin_callback,
    const std::string& authentication_tag)
    : client_id_(client_id),
      paired_secret_(paired_secret),
      create_base_authenticator_callback_(create_base_authenticator_callback),
      fetch_pin_callback_(fetch_pin_callback),
      authentication_tag_(authentication_tag),
      weak_factory_(this) {
  spake2_authenticator_ =
      create_base_authenticator_callback_.Run(paired_secret_, MESSAGE_READY);
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
  fetch_pin_callback_.Run(
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
    pairing_tag->AddAttr(kClientIdAttribute, client_id_);
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
      ApplySharedSecretHashFunction(HashFunction::HMAC_SHA256,
                                    authentication_tag_, pin),
      initial_state);
  resume_callback.Run();
}

}  // namespace protocol
}  // namespace remoting
