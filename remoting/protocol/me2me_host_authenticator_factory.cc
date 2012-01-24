// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/me2me_host_authenticator_factory.h"

#include "base/base64.h"
#include "base/string_util.h"
#include "crypto/rsa_private_key.h"
#include "remoting/protocol/v1_authenticator.h"
#include "remoting/protocol/v2_authenticator.h"

namespace remoting {
namespace protocol {


bool SharedSecretHash::Parse(const std::string& as_string) {
  size_t separator = as_string.find(':');
  if (separator == std::string::npos)
    return false;

  std::string function_name = as_string.substr(0, separator);
  if (function_name == "plain") {
    hash_function = AuthenticationMethod::NONE;
  } else if (function_name == "hmac") {
    hash_function = AuthenticationMethod::HMAC_SHA256;
  } else {
    return false;
  }

  if (!base::Base64Decode(as_string.substr(separator + 1), &value)) {
    return false;
  }

  return true;
}

Me2MeHostAuthenticatorFactory::Me2MeHostAuthenticatorFactory(
    const std::string& local_jid,
    const std::string& local_cert,
    const crypto::RSAPrivateKey& local_private_key,
    const SharedSecretHash& shared_secret_hash)
    : local_cert_(local_cert),
      local_private_key_(local_private_key.Copy()),
      shared_secret_hash_(shared_secret_hash) {
  // Verify that |local_jid| is bare.
  DCHECK_EQ(local_jid.find('/'), std::string::npos);
  local_jid_prefix_ = local_jid + '/';
}

Me2MeHostAuthenticatorFactory::~Me2MeHostAuthenticatorFactory() {
}

scoped_ptr<Authenticator> Me2MeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& remote_jid,
    const buzz::XmlElement* first_message) {
  // Reject incoming connection if the client's jid is not an ASCII string.
  if (!IsStringASCII(remote_jid)) {
    LOG(ERROR) << "Rejecting incoming connection from " << remote_jid;
    return scoped_ptr<Authenticator>(NULL);
  }

  // Check that the client has the same bare jid as the host, i.e.
  // client's full JID starts with host's bare jid. Comparison is case
  // insensitive.
  if (!StartsWithASCII(remote_jid, local_jid_prefix_, false)) {
    LOG(ERROR) << "Rejecting incoming connection from " << remote_jid;
    return scoped_ptr<Authenticator>(NULL);
  }

  if (V2Authenticator::IsEkeMessage(first_message)) {
    return V2Authenticator::CreateForHost(
        local_cert_, *local_private_key_, shared_secret_hash_.value);
  }

  // TODO(sergeyu): Old clients still use V1 auth protocol. Remove
  // this once we are done migrating to V2. crbug.com/110483 .
  return scoped_ptr<Authenticator>(new V1HostAuthenticator(
      local_cert_, *local_private_key_, "", remote_jid));
}

}  // namespace protocol
}  // namespace remoting
