// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/policy_hack/policy_watcher.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/compiler_specific.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

namespace remoting {
namespace policy_hack {

// The MacOS version does not watch files because it is accepted
// practice on the Mac that the user must logout/login for policies to be
// applied. This will actually pick up policies every
// |kFallbackReloadDelayMinutes| which is sufficient for right now.
class PolicyWatcherMac : public PolicyWatcher {
 public:
  explicit PolicyWatcherMac(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : PolicyWatcher(task_runner) {
  }

  virtual ~PolicyWatcherMac() {
  }

 protected:
  virtual void StartWatchingInternal() OVERRIDE {
    Reload();
  }

  virtual void StopWatchingInternal() OVERRIDE {
  }

  virtual void Reload() OVERRIDE {
    DCHECK(OnPolicyWatcherThread());
    base::DictionaryValue policy;

    CFStringRef policy_bundle_id = CFSTR("com.google.Chrome");
    if (CFPreferencesAppSynchronize(policy_bundle_id)) {
      for (base::DictionaryValue::Iterator i(Defaults());
           i.HasNext(); i.Advance()) {
        const std::string& policy_name = i.key();
        base::mac::ScopedCFTypeRef<CFStringRef> policy_key(
            base::SysUTF8ToCFStringRef(policy_name));

        if (i.value().GetType() == base::DictionaryValue::TYPE_BOOLEAN) {
          Boolean valid = false;
          bool value = CFPreferencesGetAppBooleanValue(policy_key,
                                                       policy_bundle_id,
                                                       &valid);
          if (valid) {
            policy.SetBoolean(policy_name, value);
          }
        }

        if (i.value().GetType() == base::DictionaryValue::TYPE_STRING) {
          base::mac::ScopedCFTypeRef<CFPropertyListRef> property_list(
              CFPreferencesCopyAppValue(policy_key, policy_bundle_id));
          if (property_list.get() != NULL) {
            CFStringRef policy_value = base::mac::CFCast<CFStringRef>(
                property_list.get());
            if (policy_value != NULL) {
              policy.SetString(policy_name,
                               base::SysCFStringRefToUTF8(policy_value));
            }
          }
        }

      }
    }

    // Set policy. Policy must be set (even if it is empty) so that the
    // default policy is picked up the first time reload is called.
    UpdatePolicies(&policy);

    // Reschedule task.
    ScheduleFallbackReloadTask();
  }
};

PolicyWatcher* PolicyWatcher::Create(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return new PolicyWatcherMac(task_runner);
}

}  // namespace policy_hack
}  // namespace remoting
