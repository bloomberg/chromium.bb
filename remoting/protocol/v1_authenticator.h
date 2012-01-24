// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_V1_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_V1_AUTHENTICATOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/authenticator.h"

namespace crypto {
class RSAPrivateKey;
}  // namespace base

namespace remoting {
namespace protocol {

class V1ClientAuthenticator : public Authenticator {
 public:
  V1ClientAuthenticator(const std::string& local_jid,
                        const std::string& shared_secret);
  virtual ~V1ClientAuthenticator();

  // Authenticator interface.
  virtual State state() const OVERRIDE;
  virtual RejectionReason rejection_reason() const OVERRIDE;
  virtual void ProcessMessage(const buzz::XmlElement* message) OVERRIDE;
  virtual scoped_ptr<buzz::XmlElement> GetNextMessage() OVERRIDE;
  virtual scoped_ptr<ChannelAuthenticator>
      CreateChannelAuthenticator() const OVERRIDE;

 private:
  std::string local_jid_;
  std::string shared_secret_;
  std::string remote_cert_;
  State state_;
  RejectionReason rejection_reason_;

  DISALLOW_COPY_AND_ASSIGN(V1ClientAuthenticator);
};

class V1HostAuthenticator : public Authenticator {
 public:
  // Doesn't take ownership of |local_private_key|.
  V1HostAuthenticator(const std::string& local_cert,
                      const crypto::RSAPrivateKey& local_private_key,
                      const std::string& shared_secret,
                      const std::string& remote_jid);
  virtual ~V1HostAuthenticator();

  // Authenticator interface.
  virtual State state() const OVERRIDE;
  virtual RejectionReason rejection_reason() const OVERRIDE;
  virtual void ProcessMessage(const buzz::XmlElement* message) OVERRIDE;
  virtual scoped_ptr<buzz::XmlElement> GetNextMessage() OVERRIDE;
  virtual scoped_ptr<ChannelAuthenticator>
      CreateChannelAuthenticator() const OVERRIDE;

 private:
  std::string local_cert_;
  scoped_ptr<crypto::RSAPrivateKey> local_private_key_;
  std::string shared_secret_;
  std::string remote_jid_;
  State state_;
  RejectionReason rejection_reason_;

  DISALLOW_COPY_AND_ASSIGN(V1HostAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_V1_AUTHENTICATOR_H_
