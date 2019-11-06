// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_JOIN_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_JOIN_DELEGATE_H_

#include <string>

#include "base/callback.h"

namespace chromeos {

// Called on successful Active Directory domain join. Pass Active Directory
// realm.
using OnDomainJoinedCallback =
    base::OnceCallback<void(const std::string& realm)>;

// Delegate being used during enterprise enrollment to start Active Directory
// domain join flow. This is needed because we have to start the join flow from
// inside EnrollmentHandlerChromeOS and enrollment screen is not available
// there.
class ActiveDirectoryJoinDelegate {
 public:
  ActiveDirectoryJoinDelegate() = default;
  // Start the Active Directory domain join flow. |dm_token| will be stored in
  // the device policy. |domain_join_config| could be used to streamline the
  // flow.
  virtual void JoinDomain(const std::string& dm_token,
                          const std::string& domain_join_config,
                          OnDomainJoinedCallback on_joined_callback) = 0;

 protected:
  ~ActiveDirectoryJoinDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveDirectoryJoinDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ACTIVE_DIRECTORY_JOIN_DELEGATE_H_
