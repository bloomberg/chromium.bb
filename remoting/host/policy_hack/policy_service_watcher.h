// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_POLICY_HACK_POLICY_SERVICE_WATCHER_H_
#define REMOTING_HOST_POLICY_HACK_POLICY_SERVICE_WATCHER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/policy/core/common/policy_service.h"
#include "remoting/host/policy_hack/policy_watcher.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace policy {
class AsyncPolicyLoader;
class ConfigurationPolicyProvider;
class SchemaRegistry;
}  // namespace policy

namespace remoting {
namespace policy_hack {

// TODO(lukasza): Merge PolicyServiceWatcher with PolicyWatcher class.

// PolicyServiceWatcher is a concrete implementation of PolicyWatcher that wraps
// an instance of PolicyService.
class PolicyServiceWatcher : public PolicyWatcher,
                             public policy::PolicyService::Observer {
 public:
  // Constructor for the case when |policy_service| is borrowed.
  //
  // |policy_service_task_runner| is the task runner where it is safe
  // to call |policy_service| methods and where we expect to get callbacks
  // from |policy_service|.
  PolicyServiceWatcher(const scoped_refptr<base::SingleThreadTaskRunner>&
                           policy_service_task_runner,
                       policy::PolicyService* policy_service);

  // Constructor for the case when |policy_service| is owned (and uses also
  // owned |owned_policy_provider| and |owned_schema_registry|.
  //
  // |policy_service_task_runner| is the task runner where it is safe
  // to call |policy_service| methods and where we expect to get callbacks
  // from |policy_service|.
  PolicyServiceWatcher(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          policy_service_task_runner,
      scoped_ptr<policy::PolicyService> owned_policy_service,
      scoped_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider,
      scoped_ptr<policy::SchemaRegistry> owned_schema_registry);

  ~PolicyServiceWatcher() override;

  // Creates PolicyServiceWatcher that wraps the owned |async_policy_loader|
  // with an appropriate PolicySchema.
  //
  // |policy_service_task_runner| is passed through to the constructor
  // of PolicyServiceWatcher.
  static scoped_ptr<PolicyServiceWatcher> CreateFromPolicyLoader(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          policy_service_task_runner,
      scoped_ptr<policy::AsyncPolicyLoader> async_policy_loader);

  // PolicyService::Observer interface.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;
  void OnPolicyServiceInitialized(policy::PolicyDomain domain) override;

 protected:
  // PolicyWatcher overrides.
  void StartWatchingInternal() override;
  void StopWatchingInternal() override;

 private:
  policy::PolicyService* policy_service_;

  // Order of fields below is important to ensure destruction takes object
  // dependencies into account:
  // - |owned_policy_service_| uses |owned_policy_provider_|
  // - |owned_policy_provider_| uses |owned_schema_registry_|
  scoped_ptr<policy::SchemaRegistry> owned_schema_registry_;
  scoped_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider_;
  scoped_ptr<policy::PolicyService> owned_policy_service_;

  DISALLOW_COPY_AND_ASSIGN(PolicyServiceWatcher);
};

}  // namespace policy_hack
}  // namespace remoting

#endif  // REMOTING_HOST_POLICY_HACK_POLICY_SERVICE_WATCHER_H_
