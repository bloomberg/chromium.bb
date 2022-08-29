// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CONTROLLER_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CONTROLLER_H_

#include <memory>

#include "ash/components/device_activity/device_active_use_case.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefRegistrySimple;
class PrefService;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash {
namespace device_activity {

class DeviceActivityClient;

struct ChromeDeviceMetadataParameters;

// Counts device actives in a privacy compliant way.
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) DeviceActivityController {
 public:
  // Retrieves a singleton instance.
  static DeviceActivityController* Get();

  // Registers local state preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Determines the total start up delay before starting device activity
  // reporting.
  static base::TimeDelta DetermineStartUpDelay(base::Time chrome_first_run_ts);

  // Determines the market segment from the loaded ChromeOS device policies.
  static MarketSegment GetMarketSegment(
      policy::DeviceMode device_mode,
      policy::MarketSegment device_market_segment);

  DeviceActivityController(
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::TimeDelta start_up_delay);
  DeviceActivityController(const DeviceActivityController&) = delete;
  DeviceActivityController& operator=(const DeviceActivityController&) = delete;
  ~DeviceActivityController();

 private:
  // Start Device Activity reporting.
  void Start(PrefService* local_state,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Stop Device Activity reporting.
  void Stop();

  void OnPsmDeviceActiveSecretFetched(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& psm_device_active_secret);

  void OnMachineStatisticsLoaded(
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& psm_device_active_secret);

  std::unique_ptr<DeviceActivityClient> da_client_network_;

  // Creates a copy of chrome parameters, which is owned throughout
  // |DeviceActivityController| object lifetime.
  const ChromeDeviceMetadataParameters chrome_passed_device_params_;

  // Singleton lives throughout class lifetime.
  chromeos::system::StatisticsProvider* const statistics_provider_;

  // Automatically cancels callbacks when the referent of weakptr gets
  // destroyed.
  base::WeakPtrFactory<DeviceActivityController> weak_factory_{this};
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CONTROLLER_H_
