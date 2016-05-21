// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/it2me_host_authenticator_factory.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/negotiating_host_authenticator.h"
#include "remoting/protocol/rejecting_authenticator.h"

namespace remoting {
namespace protocol {

It2MeHostAuthenticatorFactory::It2MeHostAuthenticatorFactory(
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& access_code_hash,
    const std::string& required_client_domain)
    : local_cert_(local_cert),
      key_pair_(key_pair),
      access_code_hash_(access_code_hash),
      required_client_domain_(required_client_domain) {}

It2MeHostAuthenticatorFactory::~It2MeHostAuthenticatorFactory() {}

std::unique_ptr<Authenticator>
It2MeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& local_jid,
    const std::string& remote_jid) {
  // Check the client domain policy.
  if (!required_client_domain_.empty()) {
    std::string client_username = remote_jid;
    size_t pos = client_username.find('/');
    if (pos != std::string::npos) {
      client_username.replace(pos, std::string::npos, "");
    }
    if (!base::EndsWith(client_username,
                        std::string("@") + required_client_domain_,
                        base::CompareCase::INSENSITIVE_ASCII)) {
      LOG(ERROR) << "Rejecting incoming connection from " << remote_jid
                 << ": Domain mismatch.";
      return base::WrapUnique(
          new RejectingAuthenticator(Authenticator::INVALID_ACCOUNT));
    }
  }

  return NegotiatingHostAuthenticator::CreateWithSharedSecret(
      local_jid, remote_jid, local_cert_, key_pair_, access_code_hash_,
      nullptr);
}

}  // namespace protocol
}  // namespace remoting
