// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BACKGROUND_EID_GENERATOR_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BACKGROUND_EID_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/clock.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/secure_channel/data_with_timestamp.h"

namespace cryptauth {
class BeaconSeed;
}  // namespace cryptauth

namespace chromeos {

namespace secure_channel {

class RawEidGenerator;

// Generates ephemeral ID (EID) values that are broadcast for background BLE
// advertisements in the ProximityAuth protocol.
//
// Background BLE advertisements, because they're generally being advertised for
// extended periods of time, use a frequently rotating EID rotation scheme, for
// privacy reasons (EIDs should rotate more frequently to prevent others from
// tracking this device or user).
//
// When advertising in background mode, we offload advertising to the hardware
// in order to conserve battery. We assume, however, that the scanning side is
// not bound by battery constraints.
//
// For the inverse of this model, in which advertising is neither privacy- nor
// battery-sensitive, see ForegroundEidGenerator.
class BackgroundEidGenerator {
 public:
  BackgroundEidGenerator();
  virtual ~BackgroundEidGenerator();

  // Returns a list of the nearest EIDs from the current time. Note that the
  // list of EIDs is sorted from earliest timestamp to latest.
  virtual std::vector<DataWithTimestamp> GenerateNearestEids(
      const std::vector<cryptauth::BeaconSeed>& beacon_seed) const;

  // Given an incoming background advertisement with
  // |advertisement_service_data|, identifies which device (if any) sent the
  // advertisement. Returns a device ID which identifies the device. If no
  // device can be identified, returns an empty string.
  virtual std::string IdentifyRemoteDeviceByAdvertisement(
      const std::string& advertisement_service_data,
      const multidevice::RemoteDeviceRefList& remote_devices) const;

 private:
  friend class SecureChannelBackgroundEidGeneratorTest;
  BackgroundEidGenerator(std::unique_ptr<RawEidGenerator> raw_eid_generator,
                         base::Clock* clock);

  // Helper function to generate the EID for any |timestamp_ms|, properly
  // calculating the start of the period. Returns nullptr if |timestamp_ms| is
  // outside the range of |beacon_seeds|.
  std::unique_ptr<DataWithTimestamp> GenerateEid(
      int64_t timestamp_ms,
      const std::vector<cryptauth::BeaconSeed>& beacon_seeds) const;

  std::unique_ptr<RawEidGenerator> raw_eid_generator_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundEidGenerator);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BACKGROUND_EID_GENERATOR_H_
