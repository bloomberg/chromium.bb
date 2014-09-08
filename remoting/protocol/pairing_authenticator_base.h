// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_BASE_H_
#define REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_BASE_H_

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

// The pairing authenticator builds on top of V2Authenticator to add
// support for PIN-less authentication via device pairing:
//
// * If a client device is already paired, it includes in the initial
//   authentication message a Client Id and the first SPAKE message
//   using the Paired Secret and HMAC_SHA256.
// * If the host recognizes the Client Id, it looks up the corresponding
//   Paired Secret and continue the SPAKE exchange.
// * If it does not recognize the Client Id, it initiates a SPAKE exchange
//   with HMAC_SHA256 using the PIN as the shared secret. The initial
//   message of this exchange includes an an error message, which
//   informs the client that the PIN-less connection failed and causes
//   it to prompt the user for a PIN to use for authentication
//   instead.
// * If, at any point, the SPAKE exchange fails with the Paired Secret,
//   the endpoint that detects the failure initiates a new SPAKE exchange
//   using the PIN, and includes an error message to instruct the peer
//   to do likewise.
//
// If a client device is not already paired, but supports pairing, then
// the V2Authenticator is used instead of this class. Only the method name
// differs, which the client uses to determine that pairing should be offered
// to the user (see NegotiatingHostAuthenticator::CreateAuthenticator and
// NegotiatingClientAuthenticator::CreateAuthenticatorForCurrentMethod).
class PairingAuthenticatorBase : public Authenticator {
 public:
  PairingAuthenticatorBase();
  virtual ~PairingAuthenticatorBase();

  // Authenticator interface.
  virtual State state() const OVERRIDE;
  virtual bool started() const OVERRIDE;
  virtual RejectionReason rejection_reason() const OVERRIDE;
  virtual void ProcessMessage(const buzz::XmlElement* message,
                              const base::Closure& resume_callback) OVERRIDE;
  virtual scoped_ptr<buzz::XmlElement> GetNextMessage() OVERRIDE;
  virtual scoped_ptr<ChannelAuthenticator>
      CreateChannelAuthenticator() const OVERRIDE;

 protected:
  typedef base::Callback<void(scoped_ptr<Authenticator> authenticator)>
      SetAuthenticatorCallback;

  static const buzz::StaticQName kPairingInfoTag;
  static const buzz::StaticQName kClientIdAttribute;

  // Create a V2 authenticator in the specified state, prompting the user for
  // the PIN first if necessary.
  virtual void CreateV2AuthenticatorWithPIN(
      State initial_state,
      const SetAuthenticatorCallback& callback) = 0;

  // Amend an authenticator message, for example to add client- or host-specific
  // elements to it.
  virtual void AddPairingElements(buzz::XmlElement* message) = 0;

  // A non-fatal error message that derived classes should set in order to
  // cause the peer to be notified that pairing has failed and that it should
  // fall back on PIN authentication. This string need not be human-readable,
  // nor is it currently used other than being logged.
  std::string error_message_;

  // The underlying V2 authenticator, created with either the PIN or the
  // Paired Secret by the derived class.
  scoped_ptr<Authenticator> v2_authenticator_;

  // Derived classes must set this to True if the underlying authenticator is
  // using the Paired Secret.
  bool using_paired_secret_;

 private:
  // Helper methods for ProcessMessage and GetNextMessage
  void MaybeAddErrorMessage(buzz::XmlElement* message);
  bool HasErrorMessage(const buzz::XmlElement* message) const;
  void CheckForFailedSpakeExchange(const base::Closure& resume_callback);
  void SetAuthenticatorAndProcessMessage(
      const buzz::XmlElement* message,
      const base::Closure& resume_callback,
      scoped_ptr<Authenticator> authenticator);

  // Set to true if a PIN-based authenticator has been requested but has not
  // yet been set.
  bool waiting_for_authenticator_;

  base::WeakPtrFactory<PairingAuthenticatorBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PairingAuthenticatorBase);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_H_
