// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_POLICY_WATCHER_H_
#define REMOTING_HOST_POLICY_WATCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/policy/core/common/policy_service.h"

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
}  // namespace base

namespace policy {
class AsyncPolicyLoader;
class ConfigurationPolicyProvider;
class Schema;
class SchemaRegistry;
}  // namespace policy

namespace remoting {

// Watches for changes to the managed remote access host policies.
class PolicyWatcher : public policy::PolicyService::Observer,
                      public base::NonThreadSafe {
 public:
  // Called first with all policies, and subsequently with any changed policies.
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)>
      PolicyUpdatedCallback;

  // Called after detecting malformed policies.
  typedef base::Callback<void()> PolicyErrorCallback;

  ~PolicyWatcher() override;

  // This guarantees that the |policy_updated_callback| is called at least once
  // with the current policies.  After that, |policy_updated_callback| will be
  // called whenever a change to any policy is detected. It will then be called
  // only with the changed policies.
  //
  // |policy_error_callback| will be called when malformed policies are detected
  // (i.e. wrong type of policy value, or unparseable files under
  // /etc/opt/chrome/policies/managed).
  // When called, the |policy_error_callback| is responsible for mitigating the
  // security risk of running with incorrectly formulated policies (by either
  // shutting down or locking down the host).
  // After calling |policy_error_callback| PolicyWatcher will continue watching
  // for policy changes and will call |policy_updated_callback| when the error
  // is recovered from and may call |policy_error_callback| when new errors are
  // found.
  virtual void StartWatching(
      const PolicyUpdatedCallback& policy_updated_callback,
      const PolicyErrorCallback& policy_error_callback);

  // Specify a |policy_service| to borrow (on Chrome OS, from the browser
  // process) or specify nullptr to internally construct and use a new
  // PolicyService (on other OS-es). PolicyWatcher must be used on the thread on
  // which it is created. |policy_service| is called on the same thread.
  //
  // When |policy_service| is null, then |file_task_runner| is used for reading
  // the policy from files / registry / preferences (which are blocking
  // operations). |file_task_runner| should be of TYPE_IO type.
  //
  // When |policy_service| is specified then |file_task_runner| argument is
  // ignored and 1) BrowserThread::UI is used for PolicyUpdatedCallback and
  // PolicyErrorCallback and 2) BrowserThread::FILE is used for reading the
  // policy from files / registry / preferences (although (2) is just an
  // implementation detail and should likely be ignored outside of
  // PolicyWatcher).
  static scoped_ptr<PolicyWatcher> Create(
      policy::PolicyService* policy_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner);

 private:
  friend class PolicyWatcherTest;

  // Gets Chromoting schema stored inside |owned_schema_registry_|.
  const policy::Schema* GetPolicySchema() const;

  // Simplifying wrapper around Schema::Normalize.
  // - Returns false if |dict| is invalid (i.e. contains mistyped policy
  // values).
  // - Returns true if |dict| was valid or got normalized.
  bool NormalizePolicies(base::DictionaryValue* dict);

  // Stores |new_policies| into |old_policies_|.  Returns dictionary with items
  // from |new_policies| that are different from the old |old_policies_|.
  scoped_ptr<base::DictionaryValue> StoreNewAndReturnChangedPolicies(
      scoped_ptr<base::DictionaryValue> new_policies);

  // Signals policy error to the registered |PolicyErrorCallback|.
  void SignalPolicyError();

  // |policy_service_task_runner| is the task runner where it is safe
  // to call |policy_service_| methods and where we expect to get callbacks
  // from |policy_service_|.
  PolicyWatcher(
      policy::PolicyService* policy_service,
      scoped_ptr<policy::PolicyService> owned_policy_service,
      scoped_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider,
      scoped_ptr<policy::SchemaRegistry> owned_schema_registry);

  // Creates PolicyWatcher that wraps the owned |async_policy_loader| with an
  // appropriate PolicySchema.
  //
  // |policy_service_task_runner| is passed through to the constructor of
  // PolicyWatcher.
  static scoped_ptr<PolicyWatcher> CreateFromPolicyLoader(
      scoped_ptr<policy::AsyncPolicyLoader> async_policy_loader);

  // PolicyService::Observer interface.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;
  void OnPolicyServiceInitialized(policy::PolicyDomain domain) override;

  PolicyUpdatedCallback policy_updated_callback_;
  PolicyErrorCallback policy_error_callback_;

  scoped_ptr<base::DictionaryValue> old_policies_;
  scoped_ptr<base::DictionaryValue> default_values_;

  policy::PolicyService* policy_service_;

  // Order of fields below is important to ensure destruction takes object
  // dependencies into account:
  // - |owned_policy_service_| uses |owned_policy_provider_|
  // - |owned_policy_provider_| uses |owned_schema_registry_|
  scoped_ptr<policy::SchemaRegistry> owned_schema_registry_;
  scoped_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider_;
  scoped_ptr<policy::PolicyService> owned_policy_service_;

  DISALLOW_COPY_AND_ASSIGN(PolicyWatcher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_POLICY_WATCHER_H_
