// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_V2_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_V2_AUTHENTICATOR_H_

#include <string>
#include <queue>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/p224_spake.h"
#include "remoting/protocol/authenticator.h"

namespace remoting {

class RsaKeyPair;

namespace protocol {

class V2Authenticator : public Authenticator {
 public:
  static bool IsEkeMessage(const buzz::XmlElement* message);

  static scoped_ptr<Authenticator> CreateForClient(
      const std::string& shared_secret,
      State initial_state);

  static scoped_ptr<Authenticator> CreateForHost(
      const std::string& local_cert,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& shared_secret,
      State initial_state);

  virtual ~V2Authenticator();

  // Authenticator interface.
  virtual State state() const OVERRIDE;
  virtual bool started() const OVERRIDE;
  virtual RejectionReason rejection_reason() const OVERRIDE;
  virtual void ProcessMessage(const buzz::XmlElement* message,
                              const base::Closure& resume_callback) OVERRIDE;
  virtual scoped_ptr<buzz::XmlElement> GetNextMessage() OVERRIDE;
  virtual scoped_ptr<ChannelAuthenticator>
      CreateChannelAuthenticator() const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(V2AuthenticatorTest, InvalidSecret);

  V2Authenticator(crypto::P224EncryptedKeyExchange::PeerType type,
                  const std::string& shared_secret,
                  State initial_state);

  virtual void ProcessMessageInternal(const buzz::XmlElement* message);

  bool is_host_side() const;

  // Used only for host authenticators.
  std::string local_cert_;
  scoped_refptr<RsaKeyPair> local_key_pair_;
  bool certificate_sent_;

  // Used only for client authenticators.
  std::string remote_cert_;

  // Used for both host and client authenticators.
  crypto::P224EncryptedKeyExchange key_exchange_impl_;
  State state_;
  bool started_;
  RejectionReason rejection_reason_;
  std::queue<std::string> pending_messages_;
  std::string auth_key_;

  DISALLOW_COPY_AND_ASSIGN(V2Authenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_V2_AUTHENTICATOR_H_
