// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_CLIENT_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_PAIRING_CLIENT_AUTHENTICATOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/pairing_authenticator_base.h"

namespace remoting {
namespace protocol {

class PairingClientAuthenticator : public PairingAuthenticatorBase {
 public:
  PairingClientAuthenticator(
      const ClientAuthenticationConfig& client_auth_config,
      const CreateBaseAuthenticatorCallback&
          create_base_authenticator_callback);
  ~PairingClientAuthenticator() override;

  // Authenticator interface.
  State state() const override;

 private:
  // PairingAuthenticatorBase overrides.
  void CreateSpakeAuthenticatorWithPin(
      State initial_state,
      const base::Closure& resume_callback) override;
  void AddPairingElements(buzz::XmlElement* message) override;

  void OnPinFetched(State initial_state,
                    const base::Closure& resume_callback,
                    const std::string& pin);

  ClientAuthenticationConfig client_auth_config_;
  CreateBaseAuthenticatorCallback create_base_authenticator_callback_;

  // Set to true after client_id is sent to the host.
  bool sent_client_id_ = false;

  // Set to true if a PIN-based authenticator has been requested but has not
  // yet been set.
  bool waiting_for_pin_ = false;

  base::WeakPtrFactory<PairingClientAuthenticator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PairingClientAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_H_
