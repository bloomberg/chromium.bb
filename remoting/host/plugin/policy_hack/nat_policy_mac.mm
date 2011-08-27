// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/policy_hack/nat_policy.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/compiler_specific.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/sys_string_conversions.h"
#include "base/values.h"

namespace {

struct BundledAppPolicy {
  Boolean is_valid;
  Boolean is_allowed;
  CFStringRef bundle_id;
};

}

namespace remoting {
namespace policy_hack {

// The MacOS version does not watch files (because there is potentially 9
// files to watch in three different locations) and because it is accepted
// practice on the Mac that the user must logout/login for policies to be
// applied. This will actually pick up policies every
// |kFallbackReloadDelayMinutes| which is sufficient for right now.
class NatPolicyMac : public NatPolicy {
 public:
  explicit NatPolicyMac(base::MessageLoopProxy* message_loop_proxy)
     : NatPolicy(message_loop_proxy) {
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

    // Since policy could be set for any of these browsers, assume the most
    // restrictive.
    BundledAppPolicy policies[3] = {
      { false, true, CFSTR("com.google.Chrome") },
      { false, true, CFSTR("com.chromium.Chromium") },
      { false, true, CFSTR("com.google.Chrome.canary") }
    };
    base::DictionaryValue policy;
    base::mac::ScopedCFTypeRef<CFStringRef> policy_key(
      base::SysUTF8ToCFStringRef(kNatPolicyName));
    bool is_allowed = true;
    bool is_valid = false;
    CFStringRef bundle_setting_policy = NULL;
    for (size_t i = 0; i < arraysize(policies); ++i) {
      if (CFPreferencesAppSynchronize(policies[i].bundle_id)) {
        policies[i].is_allowed = CFPreferencesGetAppBooleanValue(
            policy_key,
            policies[i].bundle_id,
            &policies[i].is_valid);
        if (policies[i].is_valid) {
          is_allowed &= policies[i].is_allowed;
          if (!is_allowed && bundle_setting_policy == NULL) {
            bundle_setting_policy = policies[i].bundle_id;
          }
          is_valid = true;
        }
      }
    }

    // Only set policy if a valid policy was found.
    if (is_valid) {
      policy.SetBoolean(kNatPolicyName, is_allowed);

      // Log if there is policy conflict.
      for (size_t i = 0; i < arraysize(policies); ++i) {
        if (policies[i].is_valid && policies[i].is_allowed != is_allowed) {
          LOG(WARNING) << base::SysCFStringRefToUTF8(policies[i].bundle_id)
                       << ":" << kNatPolicyName
                       << "(" << (policies[i].is_allowed ? "true" : "false")
                       << ") is being overridden by "
                       << base::SysCFStringRefToUTF8(bundle_setting_policy)
                       << ":" << kNatPolicyName
                       << "(" << (is_allowed ? "true" : "false") << ")";
        }
      }
    }

    // Set policy. Policy must be set (even if it is empty) so that the
    // default policy is picked up the first time reload is called.
    UpdateNatPolicy(&policy);

    // Reschedule task.
    ScheduleFallbackReloadTask();
  }
};

NatPolicy* NatPolicy::Create(base::MessageLoopProxy* message_loop_proxy) {
  return new NatPolicyMac(message_loop_proxy);
}

}  // namespace policy_hack
}  // namespace remoting
