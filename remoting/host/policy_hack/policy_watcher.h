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

namespace remoting {
namespace policy_hack {

// Watches for changes to the managed remote access host policies.
// If StartWatching() has been called, then before this object can be deleted,
// StopWatching() have completed (the provided |done| event must be signaled).
class PolicyWatcher {
 public:
  // Called first with all policies, and subsequently with any changed policies.
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue>)>
      PolicyCallback;

  explicit PolicyWatcher(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~PolicyWatcher();

  // This guarantees that the |policy_callback| is called at least once with
  // the current policies.  After that, |policy_callback| will be called
  // whenever a change to any policy is detected. It will then be called only
  // with the changed policies.
  virtual void StartWatching(const PolicyCallback& policy_callback);

  // Should be called after StartWatching() before the object is deleted. Calls
  // just wait for |done| to be signaled before deleting the object.
  virtual void StopWatching(base::WaitableEvent* done);

  // Implemented by each platform.  This message loop should be an IO message
  // loop.
  static PolicyWatcher* Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // The name of the NAT traversal policy.
  static const char kNatPolicyName[];

  // The name of the policy for requiring 2-factor authentication.
  static const char kHostRequireTwoFactorPolicyName[];

  // The name of the host domain policy.
  static const char kHostDomainPolicyName[];

  // The name of the username policy. This policy is ignored on Windows.
  // This policy is currently considered 'internal only' and so is not
  // documented in policy_templates.json.
  static const char kHostMatchUsernamePolicyName[];

  // The name of the policy that controls the host talkgadget prefix.
  static const char kHostTalkGadgetPrefixPolicyName[];

  // The name of the policy for requiring curtain-mode.
  static const char kHostRequireCurtainPolicyName[];

  // The names of the policies for token authentication URLs.
  static const char kHostTokenUrlPolicyName[];
  static const char kHostTokenValidationUrlPolicyName[];
  static const char kHostTokenValidationCertIssuerPolicyName[];

  // The name of the policy for disabling PIN-less authentication.
  static const char kHostAllowClientPairing[];

  // The name of the policy for disabling gnubbyd forwarding.
  static const char kHostAllowGnubbyAuthPolicyName[];

  // The name of the policy for allowing use of relay servers.
  static const char kRelayPolicyName[];

  // The name of the policy that restricts the range of host UDP ports.
  static const char kUdpPortRangePolicyName[];

  // The name of the policy for overriding policies, for use in testing.
  static const char kHostDebugOverridePoliciesName[];

 protected:
  virtual void StartWatchingInternal() = 0;
  virtual void StopWatchingInternal() = 0;
  virtual void Reload() = 0;

  // Used to check if the class is on the right thread.
  bool OnPolicyWatcherThread() const;

  // Takes the policy dictionary from the OS specific store and extracts the
  // relevant policies.
  void UpdatePolicies(const base::DictionaryValue* new_policy);

  // Used for time-based reloads in case something goes wrong with the
  // notification system.
  void ScheduleFallbackReloadTask();
  void ScheduleReloadTask(const base::TimeDelta& delay);

  // Returns a DictionaryValue containing the default values for each policy.
  const base::DictionaryValue& Defaults() const;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  PolicyCallback policy_callback_;

  scoped_ptr<base::DictionaryValue> old_policies_;
  scoped_ptr<base::DictionaryValue> default_values_;
  scoped_ptr<base::DictionaryValue> bad_type_values_;

  // Allows us to cancel any inflight FileWatcher events or scheduled reloads.
  base::WeakPtrFactory<PolicyWatcher> weak_factory_;
};

}  // namespace policy_hack
}  // namespace remoting

#endif  // REMOTING_HOST_POLICY_HACK_POLICY_WATCHER_H_
