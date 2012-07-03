// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/policy_hack/nat_policy.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/compiler_specific.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/sys_string_conversions.h"
#include "base/values.h"

namespace remoting {
namespace policy_hack {

// The MacOS version does not watch files because it is accepted
// practice on the Mac that the user must logout/login for policies to be
// applied. This will actually pick up policies every
// |kFallbackReloadDelayMinutes| which is sufficient for right now.
class NatPolicyMac : public NatPolicy {
 public:
  explicit NatPolicyMac(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
     : NatPolicy(task_runner) {
  }

  virtual ~NatPolicyMac() {
  }

 protected:
  virtual void StartWatchingInternal() OVERRIDE {
    Reload();
  }

  virtual void StopWatchingInternal() OVERRIDE {
  }

  virtual void Reload() OVERRIDE {
    DCHECK(OnPolicyThread());
    base::DictionaryValue policy;

    CFStringRef policy_bundle_id = CFSTR("com.google.Chrome");
    if (CFPreferencesAppSynchronize(policy_bundle_id)) {
      base::mac::ScopedCFTypeRef<CFStringRef> policy_key(
          base::SysUTF8ToCFStringRef(kNatPolicyName));
      Boolean valid = false;
      bool allowed = CFPreferencesGetAppBooleanValue(policy_key,
                                                     policy_bundle_id,
                                                     &valid);
      if (valid) {
        policy.SetBoolean(kNatPolicyName, allowed);
      }
    }

    // Set policy. Policy must be set (even if it is empty) so that the
    // default policy is picked up the first time reload is called.
    UpdateNatPolicy(&policy);

    // Reschedule task.
    ScheduleFallbackReloadTask();
  }
};

NatPolicy* NatPolicy::Create(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return new NatPolicyMac(task_runner);
}

}  // namespace policy_hack
}  // namespace remoting
