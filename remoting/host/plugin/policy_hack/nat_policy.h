// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PLUGIN_POLICY_HACK_NAT_POLICY_H_
#define REMOTING_HOST_PLUGIN_POLICY_HACK_NAT_POLICY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace base {
class MessageLoopProxy;
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

  NatPolicy() {}
  virtual ~NatPolicy() {}

  // This guarantees that the |nat_enabled_cb| is called at least once with
  // the current policy.  After that, |nat_enabled_cb| will be called whenever
  // a change to the nat policy is detected.
  virtual void StartWatching(const NatEnabledCallback& nat_enabled_cb) = 0;

  // Should be called after StartWatching() before the object is deleted. Calls
  // just wait for |done| to be signaled before deleting the object.
  virtual void StopWatching(base::WaitableEvent* done) = 0;

  // Implemented by each platform.  This message loop should be an IO message
  // loop on linux.
  //
  // TODO(ajwong): figure out the right message loop for win/mac.
  static NatPolicy* Create(base::MessageLoopProxy* message_loop_proxy);
};

}  // namespace policy_hack
}  // namespace remoting

#endif  // REMOTING_HOST_PLUGIN_POLICY_HACK_NAT_POLICY_H_
