// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NEGOTIATING_HOST_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_NEGOTIATING_HOST_AUTHENTICATOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/negotiating_authenticator_base.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

// Host-side implementation of NegotiatingAuthenticatorBase.
// See comments in negotiating_authenticator_base.h for a general explanation.
class NegotiatingHostAuthenticator : public NegotiatingAuthenticatorBase {
 public:
  // Creates a host authenticator, using a fixed shared secret/PIN hash.
  NegotiatingHostAuthenticator(
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& shared_secret_hash,
      AuthenticationMethod::HashFunction hash_function);

  virtual ~NegotiatingHostAuthenticator();

  // Overriden from Authenticator.
  virtual void ProcessMessage(const buzz::XmlElement* message,
                              const base::Closure& resume_callback) OVERRIDE;
  virtual scoped_ptr<buzz::XmlElement> GetNextMessage() OVERRIDE;

 private:
  // (Asynchronously) creates an authenticator, and stores it in
  // |current_authenticator_|. Authenticators that can be started in either
  // state will be created in |preferred_initial_state|.
  // |resume_callback| is called after |current_authenticator_| is set.
  void CreateAuthenticator(Authenticator::State preferred_initial_state,
                           const base::Closure& resume_callback);

  std::string local_cert_;
  scoped_refptr<RsaKeyPair> local_key_pair_;
  std::string shared_secret_hash_;

  DISALLOW_COPY_AND_ASSIGN(NegotiatingHostAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_NEGOTIATING_HOST_AUTHENTICATOR_H_
