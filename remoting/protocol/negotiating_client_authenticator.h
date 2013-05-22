// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NEGOTIATING_CLIENT_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_NEGOTIATING_CLIENT_AUTHENTICATOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/negotiating_authenticator_base.h"
#include "remoting/protocol/third_party_client_authenticator.h"

namespace remoting {
namespace protocol {

// Client-side implementation of NegotiatingAuthenticatorBase.
// See comments in negotiating_authenticator_base.h for a general explanation.
class NegotiatingClientAuthenticator : public NegotiatingAuthenticatorBase {
 public:
  // TODO(jamiewalch): Pass ClientConfig instead of separate parameters.
  NegotiatingClientAuthenticator(
      const std::string& client_pairing_id,
      const std::string& shared_secret,
      const std::string& authentication_tag,
      const FetchSecretCallback& fetch_secret_callback,
      scoped_ptr<ThirdPartyClientAuthenticator::TokenFetcher> token_fetcher_,
      const std::vector<AuthenticationMethod>& methods);

  virtual ~NegotiatingClientAuthenticator();

  // Overriden from Authenticator.
  virtual void ProcessMessage(const buzz::XmlElement* message,
                              const base::Closure& resume_callback) OVERRIDE;
  virtual scoped_ptr<buzz::XmlElement> GetNextMessage() OVERRIDE;

 private:
  // (Asynchronously) creates an authenticator, and stores it in
  // |current_authenticator_|. Authenticators that can be started in either
  // state will be created in |preferred_initial_state|.
  // |resume_callback| is called after |current_authenticator_| is set.
  void CreateAuthenticatorForCurrentMethod(
      Authenticator::State preferred_initial_state,
      const base::Closure& resume_callback);

  // If possible, create a preferred authenticator ready to send an
  // initial message optimistically to the host. The host is free to
  // ignore the client's preferred authenticator and initial message
  // and to instead reply with an alternative method. See the comments
  // in negotiating_authenticator_base.h for more details.
  //
  // Sets |current_authenticator_| and |current_method_| iff the client
  // has a preferred authenticator that can optimistically send an initial
  // message.
  void CreatePreferredAuthenticator();

  // Creates a V2Authenticator in state |initial_state| with the given
  // |shared_secret|, then runs |resume_callback|.
  void CreateV2AuthenticatorWithSecret(
      Authenticator::State initial_state,
      const base::Closure& resume_callback,
      const std::string& shared_secret);

  // Used for pairing authenticators
  std::string client_pairing_id_;
  std::string shared_secret_;

  // Used for all authenticators.
  std::string authentication_tag_;

  // Used for shared secret authenticators.
  FetchSecretCallback fetch_secret_callback_;

  // Used for third party authenticators.
  scoped_ptr<ThirdPartyClientAuthenticator::TokenFetcher> token_fetcher_;

  // Internal NegotiatingClientAuthenticator data.
  bool method_set_by_host_;
  base::WeakPtrFactory<NegotiatingClientAuthenticator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NegotiatingClientAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_NEGOTIATING_CLIENT_AUTHENTICATOR_H_
