// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/me2me_host_authenticator_factory.h"

#include "base/base64.h"
#include "base/string_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/negotiating_host_authenticator.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

namespace {

// Authenticator that accepts one message and rejects connection after that.
class RejectingAuthenticator : public Authenticator {
 public:
  RejectingAuthenticator()
      : state_(WAITING_MESSAGE) {
  }
  virtual ~RejectingAuthenticator() {
  }

  virtual State state() const OVERRIDE {
    return state_;
  }

  virtual RejectionReason rejection_reason() const OVERRIDE {
    DCHECK_EQ(state_, REJECTED);
    return INVALID_CREDENTIALS;
  }

  virtual void ProcessMessage(const buzz::XmlElement* message,
                              const base::Closure& resume_callback) OVERRIDE {
    DCHECK_EQ(state_, WAITING_MESSAGE);
    state_ = REJECTED;
    resume_callback.Run();
  }

  virtual scoped_ptr<buzz::XmlElement> GetNextMessage() OVERRIDE {
    NOTREACHED();
    return scoped_ptr<buzz::XmlElement>(NULL);
  }

  virtual scoped_ptr<ChannelAuthenticator>
  CreateChannelAuthenticator() const OVERRIDE {
    NOTREACHED();
    return scoped_ptr<ChannelAuthenticator>(NULL);
  }

 protected:
  State state_;
};

}  // namespace

// static
scoped_ptr<AuthenticatorFactory>
Me2MeHostAuthenticatorFactory::CreateWithSharedSecret(
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    const SharedSecretHash& shared_secret_hash) {
  scoped_ptr<Me2MeHostAuthenticatorFactory> result(
      new Me2MeHostAuthenticatorFactory());
  result->local_cert_ = local_cert;
  result->key_pair_ = key_pair;
  result->shared_secret_hash_ = shared_secret_hash;
  return scoped_ptr<AuthenticatorFactory>(result.Pass());
}


// static
scoped_ptr<AuthenticatorFactory>
Me2MeHostAuthenticatorFactory::CreateWithThirdPartyAuth(
    const std::string& local_cert,
    scoped_refptr<RsaKeyPair> key_pair,
    scoped_ptr<ThirdPartyHostAuthenticator::TokenValidatorFactory>
        token_validator_factory) {
  scoped_ptr<Me2MeHostAuthenticatorFactory> result(
      new Me2MeHostAuthenticatorFactory());
  result->local_cert_ = local_cert;
  result->key_pair_ = key_pair;
  result->token_validator_factory_ = token_validator_factory.Pass();
  return scoped_ptr<AuthenticatorFactory>(result.Pass());
}

// static
scoped_ptr<AuthenticatorFactory>
    Me2MeHostAuthenticatorFactory::CreateRejecting() {
  return scoped_ptr<AuthenticatorFactory>(new Me2MeHostAuthenticatorFactory());
}

Me2MeHostAuthenticatorFactory::Me2MeHostAuthenticatorFactory() {
}

Me2MeHostAuthenticatorFactory::~Me2MeHostAuthenticatorFactory() {
}

scoped_ptr<Authenticator> Me2MeHostAuthenticatorFactory::CreateAuthenticator(
    const std::string& local_jid,
    const std::string& remote_jid,
    const buzz::XmlElement* first_message) {

  size_t slash_pos = local_jid.find('/');
  if (slash_pos == std::string::npos) {
    LOG(DFATAL) << "Invalid local JID:" << local_jid;
    return scoped_ptr<Authenticator>(new RejectingAuthenticator());
  }

  // Verify that the client's jid is an ASCII string, and then check
  // that the client has the same bare jid as the host, i.e. client's
  // full JID starts with host's bare jid. Comparison is case
  // insensitive.
  if (!IsStringASCII(remote_jid) ||
      !StartsWithASCII(remote_jid, local_jid.substr(0, slash_pos + 1), false)) {
    LOG(ERROR) << "Rejecting incoming connection from " << remote_jid;
    return scoped_ptr<Authenticator>(new RejectingAuthenticator());
  }

  if (!local_cert_.empty() && key_pair_) {
    if (token_validator_factory_) {
      return NegotiatingHostAuthenticator::CreateWithThirdPartyAuth(
          local_cert_, key_pair_,
          token_validator_factory_->CreateTokenValidator(
              local_jid, remote_jid));
    }

    return NegotiatingHostAuthenticator::CreateWithSharedSecret(
        local_cert_, key_pair_, shared_secret_hash_.value,
        shared_secret_hash_.hash_function);
  }

  return scoped_ptr<Authenticator>(new RejectingAuthenticator());
}

}  // namespace protocol
}  // namespace remoting
