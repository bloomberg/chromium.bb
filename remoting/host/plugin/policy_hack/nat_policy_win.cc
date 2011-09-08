// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from various classes in
// src/chrome/browser/policy. In particular, look at
//
//   configuration_policy_provider_delegate_win.{h,cc}
//   configuration_policy_loader_win.{h,cc}
//
// This is a reduction of the functionality in those classes.

#include "remoting/host/plugin/policy_hack/nat_policy.h"

#include <userenv.h>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "base/win/object_watcher.h"

// userenv.dll is required for RegisterGPNotification().
#pragma comment(lib, "userenv.lib")

using base::win::RegKey;

namespace remoting {
namespace policy_hack {

namespace {

const wchar_t kRegistrySubKey[] = L"SOFTWARE\\Policies\\Google\\Chrome";

}  // namespace

class NatPolicyWin :
  public NatPolicy,
  public base::win::ObjectWatcher::Delegate {
 public:
  explicit NatPolicyWin(base::MessageLoopProxy* message_loop_proxy)
      : NatPolicy(message_loop_proxy),
        user_policy_changed_event_(false, false),
        machine_policy_changed_event_(false, false),
        user_policy_watcher_failed_(false),
        machine_policy_watcher_failed_(false) {
  }

  virtual ~NatPolicyWin() {
  }

  virtual void StartWatchingInternal() OVERRIDE {
    DCHECK(OnPolicyThread());

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
    DCHECK(OnPolicyThread());

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
    DCHECK(OnPolicyThread());

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

  bool GetRegistryPolicyInteger(const string16& value_name,
                                uint32* result) const {
    DWORD value = 0;
    RegKey policy_key(HKEY_LOCAL_MACHINE, kRegistrySubKey, KEY_READ);
    if (policy_key.ReadValueDW(value_name.c_str(), &value) == ERROR_SUCCESS) {
      *result = value;
      return true;
    }

    if (policy_key.Open(HKEY_CURRENT_USER, kRegistrySubKey, KEY_READ) ==
        ERROR_SUCCESS) {
      if (policy_key.ReadValueDW(value_name.c_str(), &value) == ERROR_SUCCESS) {
        *result = value;
        return true;
      }
    }
    return false;
  }

  bool GetRegistryPolicyBoolean(const string16& value_name,
                                bool* result) const {
    uint32 local_result = 0;
    bool ret = GetRegistryPolicyInteger(value_name, &local_result);
    if (ret)
      *result = local_result != 0;
    return ret;
  }

  base::DictionaryValue* Load() {
    base::DictionaryValue* policy = new base::DictionaryValue();

    bool bool_value;
    const string16 name(ASCIIToUTF16(kNatPolicyName));
    if (GetRegistryPolicyBoolean(name, &bool_value)) {
      policy->SetBoolean(kNatPolicyName, bool_value);
    }

    return policy;
  }

  // Post a reload notification and update the watch machinery.
  void Reload() {
    DCHECK(OnPolicyThread());
    SetupWatches();
    scoped_ptr<DictionaryValue> new_policy(Load());
    UpdateNatPolicy(new_policy.get());
  }

  // ObjectWatcher::Delegate overrides:
  virtual void OnObjectSignaled(HANDLE object) {
    DCHECK(OnPolicyThread());
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

NatPolicy* NatPolicy::Create(base::MessageLoopProxy* message_loop_proxy) {
  return new NatPolicyWin(message_loop_proxy);
}

}  // namespace policy_hack
}  // namespace remoting
