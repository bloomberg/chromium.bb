// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_POLICY_HACK_NAT_POLICY_H_
#define REMOTING_HOST_POLICY_HACK_NAT_POLICY_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
class TimeDelta;
class WaitableEvent;
}  // namespace base

namespace remoting {
namespace policy_hack {

// Watches for changes to the managed remote access host NAT policies.
// If StartWatching() has been called, then before this object can be deleted,
// StopWatching() have completed (the provided |done| event must be signaled).
class NatPolicy {
 public:
  // Called with the current status of whether or not NAT traversal is enabled.
  typedef base::Callback<void(bool)> NatEnabledCallback;

  explicit NatPolicy(scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~NatPolicy();

  // This guarantees that the |nat_enabled_cb| is called at least once with
  // the current policy.  After that, |nat_enabled_cb| will be called whenever
  // a change to the nat policy is detected.
  virtual void StartWatching(const NatEnabledCallback& nat_enabled_cb);

  // Should be called after StartWatching() before the object is deleted. Calls
  // just wait for |done| to be signaled before deleting the object.
  virtual void StopWatching(base::WaitableEvent* done);

  // Implemented by each platform.  This message loop should be an IO message
  // loop.
  static NatPolicy* Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 protected:
  virtual void StartWatchingInternal() = 0;
  virtual void StopWatchingInternal() = 0;
  virtual void Reload() = 0;

  // Used to check if the class is on the right thread.
  bool OnPolicyThread() const;

  // Takes the policy dictionary from the OS specific store and extracts the
  // NAT traversal setting.
  void UpdateNatPolicy(base::DictionaryValue* new_policy);

  // Used for time-based reloads in case something goes wrong with the
  // notification system.
  void ScheduleFallbackReloadTask();
  void ScheduleReloadTask(const base::TimeDelta& delay);

  static const char kNatPolicyName[];

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  NatEnabledCallback nat_enabled_cb_;
  bool current_nat_enabled_state_;
  bool first_state_published_;

  // Allows us to cancel any inflight FileWatcher events or scheduled reloads.
  base::WeakPtrFactory<NatPolicy> weak_factory_;
};

}  // namespace policy_hack
}  // namespace remoting

#endif  // REMOTING_HOST_POLICY_HACK_NAT_POLICY_H_
