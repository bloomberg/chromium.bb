// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_CLIENT_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_PAIRING_CLIENT_AUTHENTICATOR_H_

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/pairing_authenticator_base.h"

namespace remoting {
namespace protocol {

class PairingClientAuthenticator : public PairingAuthenticatorBase {
 public:
  PairingClientAuthenticator(
      const std::string& client_id,
      const std::string& paired_secret,
      const FetchSecretCallback& fetch_pin_callback,
      const std::string& authentication_tag);
  ~PairingClientAuthenticator() override;

 private:
  // PairingAuthenticatorBase interface.
  void CreateV2AuthenticatorWithPIN(
      State initial_state,
      const SetAuthenticatorCallback& callback) override;
  void AddPairingElements(buzz::XmlElement* message) override;

  void OnPinFetched(State initial_state,
                    const SetAuthenticatorCallback& callback,
                    const std::string& pin);

  // Protocol state.
  bool sent_client_id_;
  std::string client_id_;
  const std::string& paired_secret_;
  FetchSecretCallback fetch_pin_callback_;
  std::string authentication_tag_;

  base::WeakPtrFactory<PairingClientAuthenticator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PairingClientAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_H_
