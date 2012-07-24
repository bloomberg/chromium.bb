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

  // The names of policies with boolean values.
  static const char* const kBooleanPolicyNames[];
  static const int kBooleanPolicyNamesNum;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  PolicyCallback policy_callback_;

  scoped_ptr<base::DictionaryValue> old_policies_;

  // Allows us to cancel any inflight FileWatcher events or scheduled reloads.
  base::WeakPtrFactory<PolicyWatcher> weak_factory_;
};

}  // namespace policy_hack
}  // namespace remoting

#endif  // REMOTING_HOST_POLICY_HACK_POLICY_WATCHER_H_
