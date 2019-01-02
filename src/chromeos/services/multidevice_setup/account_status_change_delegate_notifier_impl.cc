// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"

#include <set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/time/clock.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/host_status_provider_impl.h"
#include "chromeos/services/multidevice_setup/setup_flow_completion_recorder.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/cryptauth/software_feature_state.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const int64_t kTimestampNotSet = 0;
const char kNoHost[] = "";

}  // namespace

// static
AccountStatusChangeDelegateNotifierImpl::Factory*
    AccountStatusChangeDelegateNotifierImpl::Factory::test_factory_ = nullptr;

// static
AccountStatusChangeDelegateNotifierImpl::Factory*
AccountStatusChangeDelegateNotifierImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void AccountStatusChangeDelegateNotifierImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

AccountStatusChangeDelegateNotifierImpl::Factory::~Factory() = default;

std::unique_ptr<AccountStatusChangeDelegateNotifier>
AccountStatusChangeDelegateNotifierImpl::Factory::BuildInstance(
    HostStatusProvider* host_status_provider,
    PrefService* pref_service,
    SetupFlowCompletionRecorder* setup_flow_completion_recorder,
    base::Clock* clock) {
  return base::WrapUnique(new AccountStatusChangeDelegateNotifierImpl(
      host_status_provider, pref_service, setup_flow_completion_recorder,
      clock));
}

// static
void AccountStatusChangeDelegateNotifierImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  // Records the timestamps (in milliseconds since UNIX Epoch, aka JavaTime) of
  // the last instance the delegate was notified for each of the changes listed
  // in the class description.
  registry->RegisterInt64Pref(kNewUserPotentialHostExistsPrefName,
                              kTimestampNotSet);
  registry->RegisterInt64Pref(kExistingUserHostSwitchedPrefName,
                              kTimestampNotSet);
  registry->RegisterInt64Pref(kExistingUserChromebookAddedPrefName,
                              kTimestampNotSet);
  registry->RegisterStringPref(
      kHostDeviceIdFromMostRecentHostStatusUpdatePrefName, kNoHost);
}

AccountStatusChangeDelegateNotifierImpl::
    ~AccountStatusChangeDelegateNotifierImpl() {
  host_status_provider_->RemoveObserver(this);
}

void AccountStatusChangeDelegateNotifierImpl::OnDelegateSet() {
  CheckForMultiDeviceEvents(host_status_provider_->GetHostWithStatus());
}

// static
const char AccountStatusChangeDelegateNotifierImpl::
    kNewUserPotentialHostExistsPrefName[] =
        "multidevice_setup.new_user_potential_host_exists";

// static
const char AccountStatusChangeDelegateNotifierImpl::
    kExistingUserHostSwitchedPrefName[] =
        "multidevice_setup.existing_user_host_switched";

// static
const char AccountStatusChangeDelegateNotifierImpl::
    kExistingUserChromebookAddedPrefName[] =
        "multidevice_setup.existing_user_chromebook_added";

// static
const char AccountStatusChangeDelegateNotifierImpl::
    kHostDeviceIdFromMostRecentHostStatusUpdatePrefName[] =
        "multidevice_setup.host_device_id_from_most_recent_sync";

AccountStatusChangeDelegateNotifierImpl::
    AccountStatusChangeDelegateNotifierImpl(
        HostStatusProvider* host_status_provider,
        PrefService* pref_service,
        SetupFlowCompletionRecorder* setup_flow_completion_recorder,
        base::Clock* clock)
    : host_status_provider_(host_status_provider),
      pref_service_(pref_service),
      setup_flow_completion_recorder_(setup_flow_completion_recorder),
      clock_(clock) {
  host_device_id_from_most_recent_update_ =
      LoadHostDeviceIdFromEndOfPreviousSession();
  host_status_provider_->AddObserver(this);
}

void AccountStatusChangeDelegateNotifierImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  CheckForMultiDeviceEvents(host_status_with_device);
}

void AccountStatusChangeDelegateNotifierImpl::CheckForMultiDeviceEvents(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  if (!delegate()) {
    PA_LOG(INFO) << "AccountStatusChangeDelegateNotifierImpl::"
                 << "CheckForMultiDeviceEvents(): Tried to check for potential "
                 << "events, but no delegate was set.";
    return;
  }

  // Track and update host info.
  base::Optional<std::string> host_device_id_before_update =
      host_device_id_from_most_recent_update_;

  // Check if a host has been set.
  if (host_status_with_device.host_device()) {
    host_device_id_from_most_recent_update_ =
        host_status_with_device.host_device()->GetDeviceId();
    pref_service_->SetString(
        kHostDeviceIdFromMostRecentHostStatusUpdatePrefName,
        *host_device_id_from_most_recent_update_);
  } else {
    // No host set.
    host_device_id_from_most_recent_update_.reset();
  }

  CheckForNewUserPotentialHostExistsEvent(host_status_with_device);
  CheckForExistingUserHostSwitchedEvent(host_status_with_device,
                                        host_device_id_before_update);
  CheckForExistingUserChromebookAddedEvent(host_device_id_before_update);
}

void AccountStatusChangeDelegateNotifierImpl::
    CheckForNewUserPotentialHostExistsEvent(
        const HostStatusProvider::HostStatusWithDevice&
            host_status_with_device) {
  // We only check for new user events if there is no enabled host.
  if (host_device_id_from_most_recent_update_)
    return;

  // If the observer has been notified of this event before, the user is not
  // new.
  if (pref_service_->GetInt64(kNewUserPotentialHostExistsPrefName) !=
      kTimestampNotSet) {
    return;
  }

  // kEligibleHostExistsButNoHostSet is the only HostStatus that can describe
  // a new user.
  if (host_status_with_device.host_status() !=
      mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    return;
  }

  delegate()->OnPotentialHostExistsForNewUser();
  pref_service_->SetInt64(kNewUserPotentialHostExistsPrefName,
                          clock_->Now().ToJavaTime());
}

void AccountStatusChangeDelegateNotifierImpl::
    CheckForExistingUserHostSwitchedEvent(
        const HostStatusProvider::HostStatusWithDevice& host_status_with_device,
        const base::Optional<std::string>& host_device_id_before_update) {
  // The host switched event requires both a pre-update and a post-update host.
  if (!host_device_id_from_most_recent_update_ ||
      !host_device_id_before_update) {
    return;
  }
  // If the host stayed the same, there was no switch.
  if (*host_device_id_from_most_recent_update_ == *host_device_id_before_update)
    return;

  delegate()->OnConnectedHostSwitchedForExistingUser(
      host_status_with_device.host_device()->name());
  pref_service_->SetInt64(kExistingUserHostSwitchedPrefName,
                          clock_->Now().ToJavaTime());
}

void AccountStatusChangeDelegateNotifierImpl::
    CheckForExistingUserChromebookAddedEvent(
        const base::Optional<std::string>& host_device_id_before_update) {
  // The Chromebook added event requires that a set host was found by the
  // update, i.e. there was no host before the host status update but afterward
  // there is a set host.
  if (!host_device_id_from_most_recent_update_ || host_device_id_before_update)
    return;

  delegate()->OnNewChromebookAddedForExistingUser();
  pref_service_->SetInt64(kExistingUserChromebookAddedPrefName,
                          clock_->Now().ToJavaTime());
}

base::Optional<std::string> AccountStatusChangeDelegateNotifierImpl::
    LoadHostDeviceIdFromEndOfPreviousSession() {
  std::string host_device_id_from_most_recent_update = pref_service_->GetString(
      kHostDeviceIdFromMostRecentHostStatusUpdatePrefName);
  if (host_device_id_from_most_recent_update.empty())
    return base::nullopt;
  return host_device_id_from_most_recent_update;
}

}  // namespace multidevice_setup

}  // namespace chromeos
