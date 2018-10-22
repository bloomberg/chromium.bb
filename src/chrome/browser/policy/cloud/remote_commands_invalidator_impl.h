// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_REMOTE_COMMANDS_INVALIDATOR_IMPL_H_
#define CHROME_BROWSER_POLICY_CLOUD_REMOTE_COMMANDS_INVALIDATOR_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/policy/cloud/remote_commands_invalidator.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace policy {

// Implementation of invalidator for remote commands services. This class
// listens to events from CloudPolicyCore and CloudPolicyStore and builds
// with RemoteCommandsInvalidator to complete the tasks.
class RemoteCommandsInvalidatorImpl : public RemoteCommandsInvalidator,
                                      public CloudPolicyCore::Observer,
                                      public CloudPolicyStore::Observer {
 public:
  explicit RemoteCommandsInvalidatorImpl(CloudPolicyCore* core);

  // RemoteCommandsInvalidator:
  void OnInitialize() override;
  void OnShutdown() override;
  void OnStart() override;
  void OnStop() override;
  void DoRemoteCommandsFetch() override;

  // CloudPolicyCore::Observer:
  void OnCoreConnected(CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override;
  void OnCoreDisconnecting(CloudPolicyCore* core) override;
  void OnRemoteCommandsServiceStarted(CloudPolicyCore* core) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

 private:
  CloudPolicyCore* const core_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCommandsInvalidatorImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_REMOTE_COMMANDS_INVALIDATOR_IMPL_H_
