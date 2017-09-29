// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SID_H_
#define SANDBOX_SRC_SID_H_

#include <windows.h>

namespace sandbox {

enum WellKnownCapabilities {
  kInternetClient,
  kInternetClientServer,
  kRegistryRead,
  kLpacCryptoServices,
  kEnterpriseAuthentication,
  kPrivateNetworkClientServer,
  kMaxWellKnownCapability
};

// This class is used to hold and generate SIDS.
class Sid {
 public:
  // Constructors initializing the object with the SID passed.
  // This is a converting constructor. It is not explicit.
  Sid(PSID sid);
  Sid(const SID* sid);
  Sid(WELL_KNOWN_SID_TYPE type);

  // Create a Sid from an AppContainer capability name. The name can be
  // completely arbitrary.
  static Sid FromNamedCapability(const wchar_t* capability_name);
  // Create a Sid from a known capability enumeration value.
  static Sid FromKnownCapability(WellKnownCapabilities capability);
  // Create a Sid from a SDDL format string, such as S-1-1-0.
  static Sid FromSddlString(const wchar_t* sddl_sid);

  // Returns sid_.
  PSID GetPSID() const;

  // Gets whether the sid is valid.
  bool IsValid() const;

 private:
  Sid();
  BYTE sid_[SECURITY_MAX_SID_SIZE];
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SID_H_
