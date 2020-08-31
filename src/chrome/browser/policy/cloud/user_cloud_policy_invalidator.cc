// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator.h"

#include <memory>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "content/public/browser/notification_source.h"

namespace {

invalidation::ProfileInvalidationProvider* GetInvalidationProvider(
    Profile* profile) {
  return invalidation::ProfileInvalidationProviderFactory::GetForProfile(
      profile);
}

}  // namespace

namespace policy {

UserCloudPolicyInvalidator::UserCloudPolicyInvalidator(
    Profile* profile,
    CloudPolicyManager* policy_manager)
    : CloudPolicyInvalidator(PolicyInvalidationScope::kUser,
                             policy_manager->core(),
                             base::ThreadTaskRunnerHandle::Get(),
                             base::DefaultClock::GetInstance(),
                             0 /* highest_handled_invalidation_version */),
      profile_(profile) {
  DCHECK(profile);

  // Register for notification that profile creation is complete. The
  // invalidator must not be initialized before then because the invalidation
  // service cannot be started because it depends on components initialized
  // after this object is instantiated.
  // TODO(stepco): Delayed initialization can be removed once the request
  // context can be accessed during profile-keyed service creation. Tracked by
  // bug 286209.
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::Source<Profile>(profile));
}

void UserCloudPolicyInvalidator::Shutdown() {
  CloudPolicyInvalidator::Shutdown();
}

void UserCloudPolicyInvalidator::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Initialize now that profile creation is complete and the invalidation
  // service can safely be initialized.
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_ADDED, type);
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      GetInvalidationProvider(profile_);
  if (!invalidation_provider)
    return;
  Initialize(invalidation_provider->GetInvalidationServiceForCustomSender(
      policy::kPolicyFCMInvalidationSenderID));
}

}  // namespace policy
