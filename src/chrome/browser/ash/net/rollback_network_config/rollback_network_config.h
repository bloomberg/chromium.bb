// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_NETWORK_CONFIG_H_
#define CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_NETWORK_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/services/rollback_network_config/public/mojom/rollback_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash {

// Implementation of rollback network config service.
// Handles import and export of rollback network configuration in managed onc
// format. Import will re-configure all given networks, policy and non-policy
// configured if ownership was not taken yet.
// Once ownership is taken (and possibly) policies are applied, only
// the device settings that were imported remain, the rest is deleted.
// Note: Import expects ONC dictionaries to be valid, do not use with
// non validated input.
// If implot or export are called multiple times, all but the latest request
// will be cancelled.
class RollbackNetworkConfig
    : public chromeos::rollback_network_config::mojom::RollbackNetworkConfig {
 public:
  using ExportCallback = chromeos::rollback_network_config::mojom::
      RollbackNetworkConfig::RollbackConfigExportCallback;
  using ImportCallback = chromeos::rollback_network_config::mojom::
      RollbackNetworkConfig::RollbackConfigImportCallback;

  RollbackNetworkConfig();
  RollbackNetworkConfig(const RollbackNetworkConfig&) = delete;
  RollbackNetworkConfig& operator=(const RollbackNetworkConfig&) = delete;
  ~RollbackNetworkConfig() override;

  void BindReceiver(
      mojo::PendingReceiver<
          chromeos::rollback_network_config::mojom::RollbackNetworkConfig>
          receiver);

  void RollbackConfigImport(const std::string& config,
                            ImportCallback callback) override;
  void RollbackConfigExport(ExportCallback callback) override;

  void fake_ownership_taken_for_testing();

 private:
  class Importer;
  class Exporter;
  std::unique_ptr<Importer> importer_;
  std::unique_ptr<Exporter> exporter_;

  mojo::ReceiverSet<
      chromeos::rollback_network_config::mojom::RollbackNetworkConfig>
      receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_NETWORK_CONFIG_H_
