// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_AFFILIATED_REMOTE_COMMANDS_INVALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_AFFILIATED_REMOTE_COMMANDS_INVALIDATOR_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"

namespace policy {

class CloudPolicyCore;
class RemoteCommandsInvalidatorImpl;

// This is a wrapper class to be used for device commands and device-local
// account commands.
class AffiliatedRemoteCommandsInvalidator
    : public AffiliatedInvalidationServiceProvider::Consumer {
 public:
  AffiliatedRemoteCommandsInvalidator(
      CloudPolicyCore* core,
      AffiliatedInvalidationServiceProvider* invalidation_service_provider,
      PolicyInvalidationScope scope);
  ~AffiliatedRemoteCommandsInvalidator() override;

  // AffiliatedInvalidationServiceProvider::Consumer:
  void OnInvalidationServiceSet(
      invalidation::InvalidationService* invalidation_service) override;

 private:
  CloudPolicyCore* const core_;
  AffiliatedInvalidationServiceProvider* const invalidation_service_provider_;

  std::unique_ptr<RemoteCommandsInvalidatorImpl> invalidator_;

  const PolicyInvalidationScope scope_;

  DISALLOW_COPY_AND_ASSIGN(AffiliatedRemoteCommandsInvalidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_AFFILIATED_REMOTE_COMMANDS_INVALIDATOR_H_
