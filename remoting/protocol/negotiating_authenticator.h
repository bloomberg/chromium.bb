// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NEGOTIATING_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_NEGOTIATING_AUTHENTICATOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/authentication_method.h"

namespace crypto {
class RSAPrivateKey;
}  // namespace crypto

namespace remoting {
namespace protocol {

class NegotiatingAuthenticator : public Authenticator {
 public:
  virtual ~NegotiatingAuthenticator();

  static bool IsNegotiableMessage(const buzz::XmlElement* message);

  static scoped_ptr<Authenticator> CreateForClient(
      const std::string& authentication_tag,
      const std::string& shared_secret,
      const std::vector<AuthenticationMethod>& methods);

  static scoped_ptr<Authenticator> CreateForHost(
      const std::string& local_cert,
      const crypto::RSAPrivateKey& local_private_key,
      const std::string& shared_secret_hash,
      AuthenticationMethod::HashFunction hash_function);

  // Authenticator interface.
  virtual State state() const OVERRIDE;
  virtual RejectionReason rejection_reason() const OVERRIDE;
  virtual void ProcessMessage(const buzz::XmlElement* message,
                              const base::Closure& resume_callback) OVERRIDE;
  virtual scoped_ptr<buzz::XmlElement> GetNextMessage() OVERRIDE;
  virtual scoped_ptr<ChannelAuthenticator>
      CreateChannelAuthenticator() const OVERRIDE;

 private:
  NegotiatingAuthenticator(Authenticator::State initial_state);

  void AddMethod(const AuthenticationMethod& method);
  void CreateAuthenticator(State initial_state);
  void UpdateState(const base::Closure& resume_callback);

  bool is_host_side() const;

  // Used only for host authenticators.
  std::string local_cert_;
  scoped_ptr<crypto::RSAPrivateKey> local_private_key_;
  bool certificate_sent_;
  std::string shared_secret_hash_;

  // Used only for client authenticators.
  std::string remote_cert_;
  std::string authentication_tag_;
  std::string shared_secret_;

  // Used for both host and client authenticators.
  std::vector<AuthenticationMethod> methods_;
  AuthenticationMethod current_method_;
  scoped_ptr<Authenticator> current_authenticator_;
  State state_;
  RejectionReason rejection_reason_;

  DISALLOW_COPY_AND_ASSIGN(NegotiatingAuthenticator);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_NEGOTIATING_AUTHENTICATOR_H_
