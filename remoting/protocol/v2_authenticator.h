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

namespace crypto {
class RSAPrivateKey;
}  // namespace crypto

namespace remoting {
namespace protocol {

class V2Authenticator : public Authenticator {
 public:
  static bool IsEkeMessage(const buzz::XmlElement* message);

  static V2Authenticator* CreateForClient(
      const std::string& shared_secret);

  static V2Authenticator* CreateForHost(
      const std::string& local_cert,
      const crypto::RSAPrivateKey& local_private_key,
      const std::string& shared_secret);

  virtual ~V2Authenticator();

  // Authenticator interface.
  virtual State state() const OVERRIDE;
  virtual void ProcessMessage(const buzz::XmlElement* message) OVERRIDE;
  virtual buzz::XmlElement* GetNextMessage() OVERRIDE;
  virtual ChannelAuthenticator* CreateChannelAuthenticator() const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(V2AuthenticatorTest, InvalidSecret);

  V2Authenticator(crypto::P224EncryptedKeyExchange::PeerType type,
                  const std::string& shared_secret);

  bool is_host_side() const;

  // Used only for HOST authenticators.
  std::string local_cert_;
  scoped_ptr<crypto::RSAPrivateKey> local_private_key_;
  bool certificate_sent_;

  // Used only for CLIENT authenticators.
  std::string remote_cert_;

  // Used for both HOST and CLIENT authenticators.
  crypto::P224EncryptedKeyExchange key_exchange_impl_;
  State state_;
  std::queue<std::string> pending_messages_;
  std::string auth_key_;

  DISALLOW_COPY_AND_ASSIGN(V2Authenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_V2_AUTHENTICATOR_H_
