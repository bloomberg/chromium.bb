// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_ANOMALY_DETECTOR_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_ANOMALY_DETECTOR_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/dbus/anomaly_detector_client.h"

namespace chromeos {

// FakeAnomalyDetectorClient is a fake implementation of AnomalyDetectorClient
// used for testing.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeAnomalyDetectorClient
    : public AnomalyDetectorClient {
 public:
  FakeAnomalyDetectorClient();
  FakeAnomalyDetectorClient(const FakeAnomalyDetectorClient&) = delete;
  FakeAnomalyDetectorClient& operator=(const FakeAnomalyDetectorClient&) =
      delete;
  ~FakeAnomalyDetectorClient() override;

  // AnomalyDetectorClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsGuestFileCorruptionSignalConnected() override;

  void set_guest_file_corruption_signal_connected(bool connected);
  void NotifyGuestFileCorruption(
      const anomaly_detector::GuestFileCorruptionSignal& signal);

 protected:
  void Init(dbus::Bus* bus) override;

 private:
  bool is_container_started_signal_connected_ = true;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_ANOMALY_DETECTOR_CLIENT_H_
