// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/com_security.h"

#include <objidl.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/win/windows_version.h"
#include "remoting/host/win/security_descriptor.h"

namespace remoting {

bool InitializeComSecurity(const std::string& security_descriptor,
                           const std::string& mandatory_label,
                           bool activate_as_activator) {
  std::string sddl = security_descriptor;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    sddl += mandatory_label;
  }

  // Convert the SDDL description into a security descriptor in absolute format.
  ScopedSd relative_sd = ConvertSddlToSd(sddl);
  if (!relative_sd) {
    PLOG(ERROR) << "Failed to create a security descriptor";
    return false;
  }
  ScopedSd absolute_sd;
  ScopedAcl dacl;
  ScopedSid group;
  ScopedSid owner;
  ScopedAcl sacl;
  if (!MakeScopedAbsoluteSd(relative_sd, &absolute_sd, &dacl, &group, &owner,
                            &sacl)) {
    PLOG(ERROR) << "MakeScopedAbsoluteSd() failed";
    return false;
  }

  DWORD capabilities = EOAC_DYNAMIC_CLOAKING;
  if (!activate_as_activator)
    capabilities |= EOAC_DISABLE_AAA;

  // Apply the security descriptor and default security settings. See
  // InitializeComSecurity's declaration for details.
  HRESULT result = CoInitializeSecurity(
      absolute_sd.get(),
      -1,       // Let COM choose which authentication services to register.
      NULL,     // See above.
      NULL,     // Reserved, must be NULL.
      RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IDENTIFY,
      NULL,     // Default authentication information is not provided.
      capabilities,
      NULL);    /// Reserved, must be NULL
  if (FAILED(result)) {
    LOG(ERROR) << "CoInitializeSecurity() failed, result=0x"
               << std::hex << result << std::dec << ".";
    return false;
  }

  return true;
}

} // namespace remoting
