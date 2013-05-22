// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/it2me_host_authenticator_factory.h"

#include "base/logging.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/negotiating_host_authenticator.h"

namespace remoting {
namespace protocol {

It2MeHostAuthenticatorFactory::It2MeHostAuthenticatorFactory(
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& shared_secret)
    : local_cert_(local_cert),
      key_pair_(key_pair),
      shared_secret_(shared_secret) {
}

It2MeHostAuthenticatorFactory::~It2MeHostAuthenticatorFactory() {
}

scoped_ptr<Authenticator> It2MeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& local_jid,
    const std::string& remote_jid,
    const buzz::XmlElement* first_message) {
  return NegotiatingHostAuthenticator::CreateWithSharedSecret(
      local_cert_, key_pair_, shared_secret_, AuthenticationMethod::NONE, NULL);
}

}  // namespace protocol
}  // namespace remoting
