// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HPS_FAKE_HPS_DBUS_CLIENT_H_
#define CHROMEOS_DBUS_HPS_FAKE_HPS_DBUS_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

// Fake implementation of HpsDBusClient. Allows callers to set a response value
// and count the number of calls to GetResultHpsNotify.
class COMPONENT_EXPORT(HPS) FakeHpsDBusClient : public HpsDBusClient {
 public:
  FakeHpsDBusClient();
  ~FakeHpsDBusClient() override;

  FakeHpsDBusClient(const FakeHpsDBusClient&) = delete;
  FakeHpsDBusClient& operator=(const FakeHpsDBusClient&) = delete;

  // Returns the fake global instance if initialized. May return null.
  static FakeHpsDBusClient* Get();

  // HpsDBusClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void GetResultHpsNotify(GetResultHpsNotifyCallback cb) override;
  void EnableHpsSense(const hps::FeatureConfig& config) override;
  void DisableHpsSense() override;
  void EnableHpsNotify(const hps::FeatureConfig& config) override;
  void DisableHpsNotify() override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback cb) override;

  // Methods for co-ordinating GetResultHpsNotify calls in tests.

  void set_hps_notify_result(absl::optional<hps::HpsResult> result) {
    hps_notify_result_ = result;
  }

  int hps_notify_count() const { return hps_notify_count_; }

  // Methods for co-ordinating WaitForServiceToBeAvailable calls in tests.
  void set_hps_service_is_available(bool is_available) {
    hps_service_is_available_ = is_available;
  }

  // Methods for co-ordinating notify enable/disable in tests.
  int enable_hps_notify_count() const { return enable_hps_notify_count_; }
  int disable_hps_notify_count() const { return disable_hps_notify_count_; }

  // Methods for co-ordinating sense enable/disable in tests.
  int enable_hps_sense_count() const { return enable_hps_sense_count_; }
  int disable_hps_sense_count() const { return disable_hps_sense_count_; }

  // Simulte HpsService restart.
  void Restart();

  // Resets all parameters; used in unittests.
  void Reset();

 private:
  absl::optional<hps::HpsResult> hps_notify_result_;
  int hps_notify_count_ = 0;
  int enable_hps_notify_count_ = 0;
  int disable_hps_notify_count_ = 0;
  int enable_hps_sense_count_ = 0;
  int disable_hps_sense_count_ = 0;
  bool hps_service_is_available_ = false;

  base::ObserverList<Observer> observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_HPS_FAKE_HPS_DBUS_CLIENT_H_
