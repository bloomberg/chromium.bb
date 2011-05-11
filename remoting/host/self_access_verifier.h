// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SELF_ACCESS_VERIFIER_H_
#define REMOTING_HOST_SELF_ACCESS_VERIFIER_H_

#include "remoting/host/access_verifier.h"

#include "base/compiler_specific.h"

namespace remoting {

class HostConfig;

namespace protocol {
class ClientAuthToken;
}  // namespace protocol

// SelfAccessVerifier is used by to verify that the client has access
// to the host in the Me2Me scenario. Currently it
//
//   1) Checks that host and client have the same bare JID.
//   2) Verifies that the access token can be decoded.
//
// TODO(sergeyu): Remove the bare-JID check, and instead ask the directory to
// perform user authorization.
class SelfAccessVerifier : public AccessVerifier {
 public:
  SelfAccessVerifier();
  virtual ~SelfAccessVerifier();

  bool Init(HostConfig* config);

  // AccessVerifier interface.
  virtual bool VerifyPermissions(
      const std::string& client_jid,
      const std::string& encoded_client_token) OVERRIDE;

 private:
  bool DecodeClientAuthToken(const std::string& encoded_client_token,
                             protocol::ClientAuthToken* client_token);

  std::string host_jid_prefix_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(SelfAccessVerifier);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SELF_ACCESS_VERIFIER_H_
