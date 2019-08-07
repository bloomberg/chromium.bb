// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SIGNIN_SCREEN_POLICY_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SIGNIN_SCREEN_POLICY_PROVIDER_H_

#include <string>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"

namespace chromeos {

// A managed policy that guards which extensions can be loaded on
// sign-in screen.
class SigninScreenPolicyProvider
    : public extensions::ManagementPolicy::Provider {
 public:
  SigninScreenPolicyProvider();
  ~SigninScreenPolicyProvider() override;

  // extensions::ManagementPolicy::Provider:
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const extensions::Extension* extension,
                   base::string16* error) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninScreenPolicyProvider);
};

std::unique_ptr<base::AutoReset<bool>>
GetScopedSigninScreenPolicyProviderDisablerForTesting();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SIGNIN_SCREEN_POLICY_PROVIDER_H_
