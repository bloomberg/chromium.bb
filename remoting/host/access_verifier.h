// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ACCESS_VERIFIER_H_
#define REMOTING_HOST_ACCESS_VERIFIER_H_

#include <string>

#include "base/basictypes.h"

namespace remoting {

namespace protocol {
class ClientAuthToken;
}  // namespace protocol

class HostConfig;

// AccessVerifier is used by to verify that the client has access to the host.
// Currently it
//
//   1) Checks that host and client have the same bare JID.
//   2) Verifies that the access token can be decoded.
//
// TODO(sergeyu): Remove the bare-JID check, and instead ask the directory to
// perform user authorization.
class AccessVerifier {
 public:
  AccessVerifier();
  bool Init(HostConfig* config);
  bool VerifyPermissions(const std::string& client_jid,
                         const std::string& encoded_client_token);

 private:
  bool DecodeClientAuthToken(const std::string& encoded_client_token,
                             protocol::ClientAuthToken* client_token);

  std::string host_jid_prefix_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(AccessVerifier);
};

}  // namespace remoting

#endif  // REMOTING_HOST_ACCESS_VERIFIER_H_
