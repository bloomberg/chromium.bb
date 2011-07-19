// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ACCESS_VERIFIER_H_
#define REMOTING_HOST_ACCESS_VERIFIER_H_

#include <string>

#include "base/basictypes.h"

namespace remoting {

// AccessVerifier is used by ChromotingHost to verify that access to
// the host. There are two implementations: SelfAccessVerifier is used
// in the Me2Me scenario, SupportAccessVerifier is used for Me2Mom.
class AccessVerifier {
 public:
  AccessVerifier() { }
  virtual ~AccessVerifier() { }

  virtual bool VerifyPermissions(const std::string& client_jid,
                                 const std::string& encoded_client_token) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_ACCESS_VERIFIER_H_
