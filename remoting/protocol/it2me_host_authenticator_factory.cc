// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/it2me_host_authenticator_factory.h"

#include "base/logging.h"
#include "crypto/rsa_private_key.h"
#include "remoting/protocol/v1_authenticator.h"
#include "remoting/protocol/negotiating_authenticator.h"

namespace remoting {
namespace protocol {

It2MeHostAuthenticatorFactory::It2MeHostAuthenticatorFactory(
    const std::string& local_cert,
    const crypto::RSAPrivateKey& local_private_key,
    const std::string& shared_secret)
    : local_cert_(local_cert),
      local_private_key_(local_private_key.Copy()),
      shared_secret_(shared_secret) {
}

It2MeHostAuthenticatorFactory::~It2MeHostAuthenticatorFactory() {
}

scoped_ptr<Authenticator> It2MeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& local_jid,
    const std::string& remote_jid,
    const buzz::XmlElement* first_message) {
  if (NegotiatingAuthenticator::IsNegotiableMessage(first_message)) {
    return NegotiatingAuthenticator::CreateForHost(
        local_cert_, *local_private_key_, shared_secret_,
        AuthenticationMethod::NONE);
  }

  return scoped_ptr<Authenticator>(new V1HostAuthenticator(
      local_cert_, *local_private_key_, shared_secret_, remote_jid));
}

}  // namespace protocol
}  // namespace remoting
