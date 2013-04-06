// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ME2ME_HOST_AUTHENTICATOR_FACTORY_H_
#define REMOTING_PROTOCOL_ME2ME_HOST_AUTHENTICATOR_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/third_party_host_authenticator.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

class Me2MeHostAuthenticatorFactory : public AuthenticatorFactory {
 public:
  // Create a factory that dispenses shared secret authenticators.
  static scoped_ptr<AuthenticatorFactory> CreateWithSharedSecret(
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const SharedSecretHash& shared_secret_hash);
  // Create a factory that dispenses third party authenticators.
  static scoped_ptr<AuthenticatorFactory> CreateWithThirdPartyAuth(
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      scoped_ptr<ThirdPartyHostAuthenticator::TokenValidatorFactory>
          token_validator_factory);
  // Create a factory that dispenses rejecting authenticators (used when the
  // host config/policy is inconsistent)
  static scoped_ptr<AuthenticatorFactory> CreateRejecting();

  Me2MeHostAuthenticatorFactory();
  virtual ~Me2MeHostAuthenticatorFactory();

  // AuthenticatorFactory interface.
  virtual scoped_ptr<Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid,
      const buzz::XmlElement* first_message) OVERRIDE;

 private:
  // Used for all host authenticators.
  std::string local_cert_;
  scoped_refptr<RsaKeyPair> key_pair_;

  // Used only for shared secret host authenticators.
  SharedSecretHash shared_secret_hash_;

  // Used only for third party host authenticators.
  scoped_ptr<ThirdPartyHostAuthenticator::TokenValidatorFactory>
      token_validator_factory_;

  DISALLOW_COPY_AND_ASSIGN(Me2MeHostAuthenticatorFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ME2ME_HOST_AUTHENTICATOR_FACTORY_H_
