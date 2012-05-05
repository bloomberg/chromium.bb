// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ME2ME_HOST_AUTHENTICATOR_FACTORY_H_
#define REMOTING_PROTOCOL_ME2ME_HOST_AUTHENTICATOR_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/authenticator.h"

namespace crypto {
class RSAPrivateKey;
}  // namespace crypto

namespace remoting {
namespace protocol {

class Me2MeHostAuthenticatorFactory : public AuthenticatorFactory {
 public:
  // Doesn't take ownership of |local_private_key|.
  Me2MeHostAuthenticatorFactory(
      const std::string& local_jid,
      const std::string& local_cert,
      const crypto::RSAPrivateKey& local_private_key,
      const SharedSecretHash& shared_secret_hash);
  virtual ~Me2MeHostAuthenticatorFactory();

  // AuthenticatorFactory interface.
  virtual scoped_ptr<Authenticator> CreateAuthenticator(
      const std::string& remote_jid,
      const buzz::XmlElement* first_message) OVERRIDE;

 private:
  std::string local_jid_prefix_;
  std::string local_cert_;
  scoped_ptr<crypto::RSAPrivateKey> local_private_key_;
  SharedSecretHash shared_secret_hash_;

  DISALLOW_COPY_AND_ASSIGN(Me2MeHostAuthenticatorFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ME2ME_HOST_AUTHENTICATOR_FACTORY_H_
