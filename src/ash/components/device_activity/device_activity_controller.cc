// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_controller.h"

#include "ash/components/device_activity/daily_use_case_impl.h"
#include "ash/components/device_activity/device_active_use_case.h"
#include "ash/components/device_activity/device_activity_client.h"
#include "ash/components/device_activity/fresnel_pref_names.h"
#include "ash/components/device_activity/monthly_use_case_impl.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/version_info/channel.h"
#include "google_apis/google_api_keys.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {
DeviceActivityController* g_ash_device_activity_controller = nullptr;

// Production edge server for reporting device actives.
// TODO(https://crbug.com/1267432): Enable passing base url as a runtime flag.
const char kFresnelBaseUrl[] = "https://crosfresnel-pa.googleapis.com";

// Count the number of PSM device active secret that is set.
const char kDeviceActiveControllerPsmDeviceActiveSecretIsSet[] =
    "Ash.DeviceActiveController.PsmDeviceActiveSecretIsSet";

void RecordPsmDeviceActiveSecretIsSet(bool is_set) {
  base::UmaHistogramBoolean(kDeviceActiveControllerPsmDeviceActiveSecretIsSet,
                            is_set);
}

class PsmDelegateImpl : public PsmDelegate {
 public:
  PsmDelegateImpl() = default;
  PsmDelegateImpl(const PsmDelegateImpl&) = delete;
  PsmDelegateImpl& operator=(const PsmDelegateImpl&) = delete;
  ~PsmDelegateImpl() override = default;

  // PsmDelegate:
  rlwe::StatusOr<std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>>
  CreatePsmClient(
      psm_rlwe::RlweUseCase use_case,
      const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) override {
    return psm_rlwe::PrivateMembershipRlweClient::Create(use_case,
                                                         plaintext_ids);
  }
};

}  // namespace

DeviceActivityController* DeviceActivityController::Get() {
  return g_ash_device_activity_controller;
}

// static
void DeviceActivityController::RegisterPrefs(PrefRegistrySimple* registry) {
  const base::Time unix_epoch = base::Time::UnixEpoch();
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownDailyPingTimestamp,
                             unix_epoch);
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownMonthlyPingTimestamp,
                             unix_epoch);
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownAllTimePingTimestamp,
                             unix_epoch);
}

// static
base::TimeDelta DeviceActivityController::DetermineStartUpDelay(
    base::Time chrome_first_run_ts) {
  // |random_delay| picks a random minute between [0, 29] inclusive (30 buckets)
  // to delay start. This will distribute the high qps during certain times,
  // across 30 equally probable buckets.
  base::TimeDelta random_delay = base::Minutes(base::RandInt(0, 29));

  // Wait at least 10 minutes from the first chrome run sentinel file creation
  // time. This creation time is used as an indicator of when the device last
  // reset (powerwashed/recovery/RMA). PSM servers take 10 minutes from CheckIn
  // to return the correct response for CheckMembership requests, since the PSM
  // servers need to update their cache.
  //
  // This delay avoids the scenario where a device checks in, powerwashes, and
  // on device start up, gets the wrong check membership response.
  base::TimeDelta delay_on_first_chrome_run;
  base::Time current_ts = base::Time::Now();
  if (current_ts < (chrome_first_run_ts + base::Minutes(10))) {
    delay_on_first_chrome_run =
        chrome_first_run_ts + base::Minutes(10) - current_ts;
  }

  return delay_on_first_chrome_run + random_delay;
}

DeviceActivityController::DeviceActivityController(
    version_info::Channel chromeos_channel,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::TimeDelta start_up_delay)
    : statistics_provider_(
          chromeos::system::StatisticsProvider::GetInstance()) {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerConstructor);

  DCHECK(!g_ash_device_activity_controller);
  g_ash_device_activity_controller = this;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&device_activity::DeviceActivityController::Start,
                     weak_factory_.GetWeakPtr(), chromeos_channel, local_state,
                     url_loader_factory),
      start_up_delay);
}

DeviceActivityController::~DeviceActivityController() {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerDestructor);

  DCHECK_EQ(this, g_ash_device_activity_controller);
  Stop();
  g_ash_device_activity_controller = nullptr;
}

void DeviceActivityController::Start(
    version_info::Channel chromeos_channel,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerStart);

  // Wrap with callback from |psm_device_active_secret_| retrieval using
  // |SessionManagerClient| DBus.
  chromeos::SessionManagerClient::Get()->GetPsmDeviceActiveSecret(
      base::BindOnce(&device_activity::DeviceActivityController::
                         OnPsmDeviceActiveSecretFetched,
                     weak_factory_.GetWeakPtr(), chromeos_channel, local_state,
                     url_loader_factory));
}

void DeviceActivityController::OnPsmDeviceActiveSecretFetched(
    version_info::Channel chromeos_channel,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& psm_device_active_secret) {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerOnPsmDeviceActiveSecretFetched);

  // In order for the device actives to be reported, the psm device active
  // secret must be retrieved successfully.
  if (psm_device_active_secret.empty()) {
    RecordPsmDeviceActiveSecretIsSet(false);
    VLOG(1) << "Can not generate PSM id without the psm device secret "
               "being defined.";
    return;
  }

  RecordPsmDeviceActiveSecretIsSet(true);

  // Continue when machine statistics are loaded, to avoid blocking.
  statistics_provider_->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
      &device_activity::DeviceActivityController::OnMachineStatisticsLoaded,
      weak_factory_.GetWeakPtr(), chromeos_channel, local_state,
      url_loader_factory, psm_device_active_secret));
}

void DeviceActivityController::OnMachineStatisticsLoaded(
    version_info::Channel chromeos_channel,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& psm_device_active_secret) {
  DeviceActivityClient::RecordDeviceActivityMethodCalled(
      DeviceActivityClient::DeviceActivityMethod::
          kDeviceActivityControllerOnMachineStatisticsLoaded);

  // Initialize all device active use cases, sorted by
  // smallest to largest window. i.e. Daily > Monthly > First Active.
  std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases;
  use_cases.push_back(std::make_unique<DailyUseCaseImpl>(
      psm_device_active_secret, chromeos_channel, local_state));
  use_cases.push_back(std::make_unique<MonthlyUseCaseImpl>(
      psm_device_active_secret, chromeos_channel, local_state));

  da_client_network_ = std::make_unique<DeviceActivityClient>(
      chromeos::NetworkHandler::Get()->network_state_handler(),
      url_loader_factory, std::make_unique<PsmDelegateImpl>(),
      std::make_unique<base::RepeatingTimer>(), kFresnelBaseUrl,
      google_apis::GetFresnelAPIKey(), std::move(use_cases));
}

void DeviceActivityController::Stop() {
  if (da_client_network_) {
    da_client_network_.reset();
  }
}

}  // namespace device_activity
}  // namespace ash
