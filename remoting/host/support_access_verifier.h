// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SUPPORT_ACCESS_VERIFIER_H_
#define REMOTING_HOST_SUPPORT_ACCESS_VERIFIER_H_

#include <string>

#include "remoting/host/access_verifier.h"

#include "base/compiler_specific.h"

namespace remoting {

class HostConfig;

// SupportAccessVerifier is used in Me2Mom scenario to verify that the
// client knows the host secret.
class SupportAccessVerifier : public AccessVerifier {
 public:
  SupportAccessVerifier();
  virtual ~SupportAccessVerifier();

  bool Init();
  const std::string& host_secret() const { return host_secret_; }

  // AccessVerifier interface.
  virtual bool VerifyPermissions(
      const std::string& client_jid,
      const std::string& encoded_client_token) OVERRIDE;

  void OnMe2MomHostRegistered(bool successful, const std::string& support_id);

 private:
  bool initialized_;
  std::string host_secret_;
  std::string support_id_;

  DISALLOW_COPY_AND_ASSIGN(SupportAccessVerifier);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SUPPORT_ACCESS_VERIFIER_H_
