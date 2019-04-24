// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TETHER_FAKE_TETHER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_TETHER_FAKE_TETHER_SERVICE_H_

#include "chrome/browser/chromeos/tether/tether_service.h"

// A stub of TetherService that provides an easy way to develop for Tether on
// non-Chromebooks or without a Tether host. To use, see
// chromeos::switches::kTetherStub for more details.
class FakeTetherService : public TetherService {
 public:
  FakeTetherService(
      Profile* profile,
      chromeos::PowerManagerClient* power_manager_client,
      chromeos::device_sync::DeviceSyncClient* device_sync_client,
      chromeos::secure_channel::SecureChannelClient* secure_channel_client,
      chromeos::multidevice_setup::MultiDeviceSetupClient*
          multidevice_setup_client,
      chromeos::NetworkStateHandler* network_state_handler,
      session_manager::SessionManager* session_manager);

  // TetherService:
  void StartTetherIfPossible() override;
  void StopTetherIfNecessary() override;

  void set_num_tether_networks(int num_tether_networks) {
    num_tether_networks_ = num_tether_networks;
  }

 protected:
  // TetherService:
  bool HasSyncedTetherHosts() const override;

 private:
  int num_tether_networks_ = 1;

  DISALLOW_COPY_AND_ASSIGN(FakeTetherService);
};

#endif  // CHROME_BROWSER_CHROMEOS_TETHER_FAKE_TETHER_SERVICE_H_
