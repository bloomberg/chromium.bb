// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ACCESS_VERIFIER_H_
#define REMOTING_HOST_ACCESS_VERIFIER_H_

#include <string>

#include "base/basictypes.h"

namespace remoting {

class HostConfig;

// AccessVerifier is used by to verify that the client has access to the host.
// Currently it just checks that host and client have the same bare JID.
//
// TODO(sergeyu): AccessVerifier should query directory to verify user
// permissions.
class AccessVerifier {
 public:
  AccessVerifier();
  bool Init(HostConfig* config);
  bool VerifyPermissions(const std::string& client_jid);

 private:
  std::string host_jid_prefix_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(AccessVerifier);
};

}  // namespace remoting

#endif  // REMOTING_HOST_ACCESS_VERIFIER_H_
