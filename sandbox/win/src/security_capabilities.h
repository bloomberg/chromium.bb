// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SECURITY_CAPABILITIES_H_
#define SANDBOX_SRC_SECURITY_CAPABILITIES_H_

#include <windows.h>

#include <vector>

#include "sandbox/win/src/sid.h"

namespace sandbox {

struct SecurityCapabilities : public SECURITY_CAPABILITIES {
 public:
  explicit SecurityCapabilities(const Sid& package_sid);
  SecurityCapabilities(const Sid& package_sid,
                       const std::vector<Sid>& capabilities);
  ~SecurityCapabilities();

 private:
  std::vector<Sid> capabilities_;
  std::vector<SID_AND_ATTRIBUTES> capability_sids_;
  Sid package_sid_;
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SECURITY_CAPABILITIES_H_