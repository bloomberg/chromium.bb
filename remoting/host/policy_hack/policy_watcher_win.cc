// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from various classes in
// src/chrome/browser/policy. In particular, look at
//
//   configuration_policy_provider_delegate_win.{h,cc}
//   configuration_policy_loader_win.{h,cc}
//
// This is a reduction of the functionality in those classes.

#include "remoting/host/policy_hack/policy_watcher.h"

#include <userenv.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "base/win/object_watcher.h"
#include "base/win/registry.h"

// userenv.dll is required for RegisterGPNotification().
#pragma comment(lib, "userenv.lib")

using base::win::RegKey;

namespace remoting {
namespace policy_hack {

namespace {

const wchar_t kRegistrySubKey[] = L"SOFTWARE\\Policies\\Google\\Chrome";

}  // namespace

class PolicyWatcherWin :
  public PolicyWatcher,
  public base::win::ObjectWatcher::Delegate {
 public:
  explicit PolicyWatcherWin(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : PolicyWatcher(task_runner),
        user_policy_changed_event_(false, false),
        machine_policy_changed_event_(false, false),
        user_policy_watcher_failed_(false),
        machine_policy_watcher_failed_(false) {
  }

  virtual ~PolicyWatcherWin() {
  }

  virtual void StartWatchingInternal() OVERRIDE {
    DCHECK(OnPolicyWatcherThread());

    if (!RegisterGPNotification(user_policy_changed_event_.handle(), false)) {
      PLOG(WARNING) << "Failed to register user group policy notification";
      user_policy_watcher_failed_ = true;
    }

    if (!RegisterGPNotification(machine_policy_changed_event_.handle(), true)) {
      PLOG(WARNING) << "Failed to register machine group policy notification.";
      machine_policy_watcher_failed_ = true;
    }

    Reload();
  }

  virtual void StopWatchingInternal() OVERRIDE {
    DCHECK(OnPolicyWatcherThread());

    if (!UnregisterGPNotification(user_policy_changed_event_.handle())) {
      PLOG(WARNING) << "Failed to unregister user group policy notification";
    }

    if (!UnregisterGPNotification(machine_policy_changed_event_.handle())) {
      PLOG(WARNING) <<
          "Failed to unregister machine group policy notification.";
    }

    user_policy_watcher_.StopWatching();
    machine_policy_watcher_.StopWatching();
  }

 private:
  // Updates the watchers and schedules the reload task if appropriate.
  void SetupWatches() {
    DCHECK(OnPolicyWatcherThread());

    if (!user_policy_watcher_failed_ &&
        !user_policy_watcher_.GetWatchedObject() &&
        !user_policy_watcher_.StartWatching(
            user_policy_changed_event_.handle(), this)) {
      LOG(WARNING) << "Failed to start watch for user policy change event";
      user_policy_watcher_failed_ = true;
    }

    if (!machine_policy_watcher_failed_ &&
        !machine_policy_watcher_.GetWatchedObject() &&
        !machine_policy_watcher_.StartWatching(
            machine_policy_changed_event_.handle(), this)) {
      LOG(WARNING) << "Failed to start watch for machine policy change event";
      machine_policy_watcher_failed_ = true;
     }

    if (user_policy_watcher_failed_ || machine_policy_watcher_failed_) {
      ScheduleFallbackReloadTask();
    }
  }

  bool GetRegistryPolicyString(const std::string& value_name,
                               std::string* result) const {
    // presubmit: allow wstring
    std::wstring value_name_wide = base::UTF8ToWide(value_name);
    // presubmit: allow wstring
    std::wstring value;
    RegKey policy_key(HKEY_LOCAL_MACHINE, kRegistrySubKey, KEY_READ);
    if (policy_key.ReadValue(value_name_wide.c_str(), &value) ==
        ERROR_SUCCESS) {
      *result = base::WideToUTF8(value);
      return true;
    }

    if (policy_key.Open(HKEY_CURRENT_USER, kRegistrySubKey, KEY_READ) ==
      ERROR_SUCCESS) {
      if (policy_key.ReadValue(value_name_wide.c_str(), &value) ==
          ERROR_SUCCESS) {
        *result = base::WideToUTF8(value);
        return true;
      }
    }
    return false;
  }

  bool GetRegistryPolicyInteger(const std::string& value_name,
                                uint32* result) const {
    // presubmit: allow wstring
    std::wstring value_name_wide = base::UTF8ToWide(value_name);
    DWORD value = 0;
    RegKey policy_key(HKEY_LOCAL_MACHINE, kRegistrySubKey, KEY_READ);
    if (policy_key.ReadValueDW(value_name_wide.c_str(), &value) ==
        ERROR_SUCCESS) {
      *result = value;
      return true;
    }

    if (policy_key.Open(HKEY_CURRENT_USER, kRegistrySubKey, KEY_READ) ==
        ERROR_SUCCESS) {
      if (policy_key.ReadValueDW(value_name_wide.c_str(), &value) ==
          ERROR_SUCCESS) {
        *result = value;
        return true;
      }
    }
    return false;
  }

  bool GetRegistryPolicyBoolean(const std::string& value_name,
                                bool* result) const {
    uint32 local_result = 0;
    bool ret = GetRegistryPolicyInteger(value_name, &local_result);
    if (ret)
      *result = local_result != 0;
    return ret;
  }

  scoped_ptr<base::DictionaryValue> Load() {
    scoped_ptr<base::DictionaryValue> policy(new base::DictionaryValue());

    for (base::DictionaryValue::Iterator i(Defaults());
         !i.IsAtEnd(); i.Advance()) {
      const std::string& policy_name = i.key();
      if (i.value().GetType() == base::DictionaryValue::TYPE_BOOLEAN) {
        bool bool_value;
        if (GetRegistryPolicyBoolean(policy_name, &bool_value)) {
          policy->SetBoolean(policy_name, bool_value);
        }
      }
      if (i.value().GetType() == base::DictionaryValue::TYPE_STRING) {
        std::string string_value;
        if (GetRegistryPolicyString(policy_name, &string_value)) {
          policy->SetString(policy_name, string_value);
        }
      }
    }
    return policy.Pass();
  }

  // Post a reload notification and update the watch machinery.
  void Reload() {
    DCHECK(OnPolicyWatcherThread());
    SetupWatches();
    scoped_ptr<base::DictionaryValue> new_policy(Load());
    UpdatePolicies(new_policy.get());
  }

  // ObjectWatcher::Delegate overrides:
  virtual void OnObjectSignaled(HANDLE object) {
    DCHECK(OnPolicyWatcherThread());
    DCHECK(object == user_policy_changed_event_.handle() ||
           object == machine_policy_changed_event_.handle())
        << "unexpected object signaled policy reload, obj = "
        << std::showbase << std::hex << object;
    Reload();
  }

  base::WaitableEvent user_policy_changed_event_;
  base::WaitableEvent machine_policy_changed_event_;
  base::win::ObjectWatcher user_policy_watcher_;
  base::win::ObjectWatcher machine_policy_watcher_;
  bool user_policy_watcher_failed_;
  bool machine_policy_watcher_failed_;
};

PolicyWatcher* PolicyWatcher::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return new PolicyWatcherWin(task_runner);
}

}  // namespace policy_hack
}  // namespace remoting
