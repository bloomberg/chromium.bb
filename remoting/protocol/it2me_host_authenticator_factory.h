// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_IT2ME_HOST_AUTHENTICATOR_FACTORY_H_
#define REMOTING_PROTOCOL_IT2ME_HOST_AUTHENTICATOR_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/authenticator.h"

namespace crypto {
class RSAPrivateKey;
}  // namespace crypto

namespace remoting {
namespace protocol {

// It2MeHostAuthenticatorFactory implements AuthenticatorFactory and
// understands both the V2 and legacy V1 authentication mechanisms.
class It2MeHostAuthenticatorFactory : public AuthenticatorFactory {
 public:
  It2MeHostAuthenticatorFactory(
      const std::string& local_cert,
      const crypto::RSAPrivateKey& local_private_key,
      const std::string& shared_secret);
  virtual ~It2MeHostAuthenticatorFactory();

  // AuthenticatorFactory interface.
  virtual scoped_ptr<Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid,
      const buzz::XmlElement* first_message) OVERRIDE;

 private:
  std::string local_cert_;
  scoped_ptr<crypto::RSAPrivateKey> local_private_key_;
  std::string shared_secret_;

  DISALLOW_COPY_AND_ASSIGN(It2MeHostAuthenticatorFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_IT2ME_HOST_AUTHENTICATOR_FACTORY_H_
