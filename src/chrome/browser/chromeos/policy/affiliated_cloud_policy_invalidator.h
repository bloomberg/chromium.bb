// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATED_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATED_CLOUD_POLICY_INVALIDATOR_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"

namespace invalidation {
class InvalidationService;
}

namespace policy {

class CloudPolicyCore;
class CloudPolicyInvalidator;

// This class provides policy invalidations for a |CloudPolicyCore| that wishes
// to use a shared |InvalidationService| affiliated with the device's enrollment
// domain. This is the case e.g. for device policy and device-local account
// policy.
// It relies on an |AffiliatedInvalidationServiceProvider| to provide it with
// access to a shared |InvalidationService| to back the
// |CloudPolicyInvalidator|. Whenever the shared |InvalidationService| changes,
// the |CloudPolicyInvalidator| is destroyed and re-created.
class AffiliatedCloudPolicyInvalidator
    : public AffiliatedInvalidationServiceProvider::Consumer {
 public:
  AffiliatedCloudPolicyInvalidator(
      PolicyInvalidationScope scope,
      CloudPolicyCore* core,
      AffiliatedInvalidationServiceProvider* invalidation_service_provider);
  ~AffiliatedCloudPolicyInvalidator() override;

  // AffiliatedInvalidationServiceProvider::Consumer:
  void OnInvalidationServiceSet(
      invalidation::InvalidationService* invalidation_service) override;

  CloudPolicyInvalidator* GetInvalidatorForTest() const;

 private:
  // Create a |CloudPolicyInvalidator| backed by the |invalidation_service|.
  void CreateInvalidator(
      invalidation::InvalidationService* invalidation_service);

  // Destroy the current |CloudPolicyInvalidator|, if any.
  void DestroyInvalidator();

  const PolicyInvalidationScope scope_;
  CloudPolicyCore* const core_;

  AffiliatedInvalidationServiceProvider* const invalidation_service_provider_;

  // The highest invalidation version that was handled already.
  int64_t highest_handled_invalidation_version_;

  // The current |CloudPolicyInvalidator|. nullptr if no connected invalidation
  // service is available.
  std::unique_ptr<CloudPolicyInvalidator> invalidator_;

  DISALLOW_COPY_AND_ASSIGN(AffiliatedCloudPolicyInvalidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATED_CLOUD_POLICY_INVALIDATOR_H_
