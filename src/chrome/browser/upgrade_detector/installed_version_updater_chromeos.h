// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_UPDATER_CHROMEOS_H_
#define CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_UPDATER_CHROMEOS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chromeos/dbus/update_engine_client.h"

class BuildState;

// Observes the system UpdateEngineClient for updates that require a device
// restart. Update information is pushed to the given BuildState as it happens.
class InstalledVersionUpdater : public chromeos::UpdateEngineClient::Observer {
 public:
  explicit InstalledVersionUpdater(BuildState* build_state);
  InstalledVersionUpdater(const InstalledVersionUpdater&) = delete;
  InstalledVersionUpdater& operator=(const InstalledVersionUpdater&) = delete;
  ~InstalledVersionUpdater() override;

  // chromeos::UpdateEngineClient::Observer:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

 private:
  // A callback run upon fetching either the current or target channel name
  // following a rollback.
  void OnChannel(bool is_current_channel, const std::string& channel_name);

  SEQUENCE_CHECKER(sequence_checker_);
  BuildState* const build_state_;

  // Holds the current channel following an enterprise rollback while waiting
  // for the target channel to be retrieved.
  base::Optional<std::string> current_channel_;

  // Holds the target channel following an enterprise rollback while waiting for
  // the current channel to be retrieved.
  base::Optional<std::string> target_channel_;

  // Holds a new version received from the update engine client following an
  // enterprise rollback while the current and target channels are being
  // retrieved.
  base::Optional<std::string> new_version_;

  base::WeakPtrFactory<InstalledVersionUpdater> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UPGRADE_DETECTOR_INSTALLED_VERSION_UPDATER_CHROMEOS_H_
