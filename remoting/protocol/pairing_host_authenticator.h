// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_HOST_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_PAIRING_HOST_AUTHENTICATOR_H_

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/pairing_authenticator_base.h"
#include "remoting/protocol/pairing_registry.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

class PairingRegistry;

class PairingHostAuthenticator : public PairingAuthenticatorBase {
 public:
  PairingHostAuthenticator(
      scoped_refptr<PairingRegistry> pairing_registry,
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& pin);
  ~PairingHostAuthenticator() override;

  // Authenticator interface.
  State state() const override;
  RejectionReason rejection_reason() const override;
  void ProcessMessage(const buzz::XmlElement* message,
                      const base::Closure& resume_callback) override;

 private:
  // PairingAuthenticatorBase interface.
  void CreateV2AuthenticatorWithPIN(
      State initial_state,
      const SetAuthenticatorCallback& callback) override;
  void AddPairingElements(buzz::XmlElement* message) override;

  // Continue processing a protocol message once the pairing information for
  // the client id has been received.
  void ProcessMessageWithPairing(const buzz::XmlElement* message,
                                 const base::Closure& resume_callback,
                                 PairingRegistry::Pairing pairing);

  // Protocol state.
  scoped_refptr<PairingRegistry> pairing_registry_;
  std::string local_cert_;
  scoped_refptr<RsaKeyPair> key_pair_;
  const std::string& pin_;
  bool protocol_error_;
  bool waiting_for_paired_secret_;

  base::WeakPtrFactory<PairingHostAuthenticator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PairingHostAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_H_
