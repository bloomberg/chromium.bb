// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_POLICY_HACK_POLICY_WATCHER_H_
#define REMOTING_HOST_POLICY_HACK_POLICY_WATCHER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace base {
class SingleThreadTaskRunner;
class TimeDelta;
class WaitableEvent;
}  // namespace base

namespace policy {
class PolicyService;
}  // namespace policy

namespace remoting {
namespace policy_hack {

// Watches for changes to the managed remote access host policies.
// If StartWatching() has been called, then before this object can be deleted,
// StopWatching() have completed (the provided |done| event must be signaled).
class PolicyWatcher {
 public:
  // Called first with all policies, and subsequently with any changed policies.
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)>
      PolicyUpdatedCallback;

  // TODO(lukasza): PolicyErrorCallback never gets called by
  // PolicyServiceWatcher.  Need to either 1) remove error-handling from
  // PolicyWatcher or 2) add error-handling around PolicyService
  // 2a) Add policy name/type validation via policy::Schema::Normalize.
  // 2b) Consider exposing parsing errors from policy::ConfigDirPolicyLoader.

  // Called after detecting malformed policies.
  typedef base::Callback<void()> PolicyErrorCallback;

  // Derived classes specify which |task_runner| should be used for calling
  // their StartWatchingInternal and StopWatchingInternal methods.
  // Derived classes promise back to call UpdatePolicies and other instance
  // methods on the same |task_runner|.
  explicit PolicyWatcher(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  virtual ~PolicyWatcher();

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
  //
  // See |Create| method's description for comments about which thread will
  // be used to run the callbacks.
  virtual void StartWatching(
      const PolicyUpdatedCallback& policy_updated_callback,
      const PolicyErrorCallback& policy_error_callback);

  // Should be called after StartWatching() before the object is deleted. Calls
  // should wait for |stopped_callback| to be called before deleting it.
  virtual void StopWatching(const base::Closure& stopped_callback);

  // Specify a |policy_service| to borrow (on Chrome OS, from the browser
  // process) or specify nullptr to internally construct and use a new
  // PolicyService (on other OS-es).
  //
  // When |policy_service| is null, then |task_runner| is used for reading the
  // policy from files / registry / preferences.  PolicyUpdatedCallback and
  // PolicyErrorCallback will be called on the same |task_runner|.
  // |task_runner| should be of TYPE_IO type.
  //
  // When |policy_service| is specified then |task_runner| argument is ignored
  // and 1) BrowserThread::UI is used for PolicyUpdatedCallback and
  // PolicyErrorCallback and 2) BrowserThread::FILE is used for reading the
  // policy from files / registry / preferences (although (2) is just an
  // implementation detail and should likely be ignored outside of
  // PolicyWatcher).
  static scoped_ptr<PolicyWatcher> Create(
      policy::PolicyService* policy_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

 protected:
  virtual void StartWatchingInternal() = 0;
  virtual void StopWatchingInternal() = 0;

  // Used to check if the class is on the right thread.
  bool OnPolicyWatcherThread() const;

  // Takes the policy dictionary from the OS specific store and extracts the
  // relevant policies.
  void UpdatePolicies(const base::DictionaryValue* new_policy);

  // Signals policy error to the registered |PolicyErrorCallback|.
  void SignalPolicyError();

  // Called whenever a transient error occurs during reading of policy files.
  // This will increment a counter, and will trigger a call to
  // SignalPolicyError() only after a threshold count is reached.
  // The counter is reset whenever policy has been successfully read.
  void SignalTransientPolicyError();

  // Returns a DictionaryValue containing the default values for each policy.
  const base::DictionaryValue& Defaults() const;

 private:
  void StopWatchingOnPolicyWatcherThread();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  PolicyUpdatedCallback policy_updated_callback_;
  PolicyErrorCallback policy_error_callback_;
  int transient_policy_error_retry_counter_;

  scoped_ptr<base::DictionaryValue> old_policies_;
  scoped_ptr<base::DictionaryValue> default_values_;
  scoped_ptr<base::DictionaryValue> bad_type_values_;

  // Allows us to cancel any inflight FileWatcher events or scheduled reloads.
  base::WeakPtrFactory<PolicyWatcher> weak_factory_;
};

}  // namespace policy_hack
}  // namespace remoting

#endif  // REMOTING_HOST_POLICY_HACK_POLICY_WATCHER_H_
