// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/network_aware_enrollment_scheduler.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/services/device_sync/persistent_enrollment_scheduler.h"

namespace chromeos {

namespace device_sync {

// static
NetworkAwareEnrollmentScheduler::Factory*
    NetworkAwareEnrollmentScheduler::Factory::test_factory_ = nullptr;

// static
NetworkAwareEnrollmentScheduler::Factory*
NetworkAwareEnrollmentScheduler::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void NetworkAwareEnrollmentScheduler::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NetworkAwareEnrollmentScheduler::Factory::~Factory() = default;

std::unique_ptr<CryptAuthEnrollmentScheduler>
NetworkAwareEnrollmentScheduler::Factory::BuildInstance(
    CryptAuthEnrollmentScheduler::Delegate* delegate,
    PrefService* pref_service,
    NetworkStateHandler* network_state_handler) {
  std::unique_ptr<NetworkAwareEnrollmentScheduler> network_aware_scheduler =
      base::WrapUnique(
          new NetworkAwareEnrollmentScheduler(delegate, network_state_handler));

  std::unique_ptr<CryptAuthEnrollmentScheduler> network_unaware_scheduler =
      PersistentEnrollmentScheduler::Factory::Get()->BuildInstance(
          network_aware_scheduler.get(), pref_service);

  network_aware_scheduler->network_unaware_scheduler_ =
      std::move(network_unaware_scheduler);

  return network_aware_scheduler;
}

NetworkAwareEnrollmentScheduler::NetworkAwareEnrollmentScheduler(
    CryptAuthEnrollmentScheduler::Delegate* delegate,
    NetworkStateHandler* network_state_handler)
    : CryptAuthEnrollmentScheduler(delegate),
      network_state_handler_(network_state_handler) {
  DCHECK(network_state_handler_);
  network_state_handler_->AddObserver(this, FROM_HERE);
}

NetworkAwareEnrollmentScheduler::~NetworkAwareEnrollmentScheduler() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void NetworkAwareEnrollmentScheduler::RequestEnrollmentNow() {
  network_unaware_scheduler_->RequestEnrollmentNow();
}

void NetworkAwareEnrollmentScheduler::HandleEnrollmentResult(
    const CryptAuthEnrollmentResult& enrollment_result) {
  network_unaware_scheduler_->HandleEnrollmentResult(enrollment_result);
}

base::Optional<base::Time>
NetworkAwareEnrollmentScheduler::GetLastSuccessfulEnrollmentTime() const {
  return network_unaware_scheduler_->GetLastSuccessfulEnrollmentTime();
}

base::TimeDelta NetworkAwareEnrollmentScheduler::GetRefreshPeriod() const {
  return network_unaware_scheduler_->GetRefreshPeriod();
}

base::TimeDelta
NetworkAwareEnrollmentScheduler::GetTimeToNextEnrollmentRequest() const {
  return network_unaware_scheduler_->GetTimeToNextEnrollmentRequest();
}

bool NetworkAwareEnrollmentScheduler::IsWaitingForEnrollmentResult() const {
  return network_unaware_scheduler_->IsWaitingForEnrollmentResult() &&
         !pending_client_directive_policy_reference_.has_value();
}

size_t NetworkAwareEnrollmentScheduler::GetNumConsecutiveFailures() const {
  return network_unaware_scheduler_->GetNumConsecutiveFailures();
}

void NetworkAwareEnrollmentScheduler::OnEnrollmentRequested(
    const base::Optional<cryptauthv2::PolicyReference>&
        client_directive_policy_reference) {
  if (DoesMachineHaveNetworkConnectivity()) {
    NotifyEnrollmentRequested(client_directive_policy_reference);
    return;
  }

  PA_LOG(INFO) << "NetworkAwareEnrollmentScheduler::OnEnrollmentRequested(): "
               << "Enrollment scheduled while the device is offline. Waiting "
               << "for online connectivity before making request.";
  pending_client_directive_policy_reference_ =
      client_directive_policy_reference;
}

void NetworkAwareEnrollmentScheduler::DefaultNetworkChanged(
    const NetworkState* network) {
  // If enrollment has not been requested, there is nothing to do.
  if (!pending_client_directive_policy_reference_)
    return;

  // The updated default network may not be online.
  if (!DoesMachineHaveNetworkConnectivity())
    return;

  // Now that the device has connectivity, request enrollment. Note that the
  // |pending_client_directive_policy_reference_| field is cleared before the
  // delegate is notified to ensure internal consistency.
  base::Optional<cryptauthv2::PolicyReference> policy_reference_for_enrollment =
      *pending_client_directive_policy_reference_;
  pending_client_directive_policy_reference_.reset();
  NotifyEnrollmentRequested(policy_reference_for_enrollment);
}

void NetworkAwareEnrollmentScheduler::OnShuttingDown() {
  DCHECK(network_state_handler_);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  network_state_handler_ = nullptr;
}

bool NetworkAwareEnrollmentScheduler::DoesMachineHaveNetworkConnectivity() {
  if (!network_state_handler_)
    return false;

  // TODO(khorimoto): IsConnectedState() can still return true if connected to a
  // captive portal; use the "online" boolean once we fetch data via the
  // networking Mojo API. See https://crbug.com/862420.
  const NetworkState* default_network =
      network_state_handler_->DefaultNetwork();
  return default_network && default_network->IsConnectedState();
}

}  // namespace device_sync

}  // namespace chromeos
