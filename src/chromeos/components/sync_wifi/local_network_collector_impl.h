// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_LOCAL_NETWORK_COLLECTOR_IMPL_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_LOCAL_NETWORK_COLLECTOR_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/components/sync_wifi/local_network_collector.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace sync_pb {
class WifiConfigurationSpecifics;
}

namespace chromeos {

class NetworkMetadataStore;

namespace sync_wifi {

// Handles the retrieval, filtering, and conversion of local network
// configurations to syncable protos.  Local networks are retrieved from Shill
// via the cros_network_config mojo interface, and passwords come directly from
// ShillServiceClient.
class LocalNetworkCollectorImpl
    : public LocalNetworkCollector,
      public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  // LocalNetworkCollector:

  // |cros_network_config| and |network_metadata_store| must outlive this class.
  explicit LocalNetworkCollectorImpl(
      network_config::mojom::CrosNetworkConfig* cros_network_config);
  ~LocalNetworkCollectorImpl() override;

  // Can only execute one request at a time.
  void GetAllSyncableNetworks(ProtoListCallback callback) override;

  // Can be called on multiple networks simultaneously.
  void GetSyncableNetwork(const std::string& guid,
                          ProtoCallback callback) override;

  base::Optional<NetworkIdentifier> GetNetworkIdentifierFromGuid(
      const std::string& guid) override;

  void SetNetworkMetadataStore(
      base::WeakPtr<NetworkMetadataStore> network_metadata_store) override;

  // CrosNetworkConfigObserver:
  void OnNetworkStateListChanged() override;
  void OnActiveNetworksChanged(
      std::vector<
          network_config::mojom::NetworkStatePropertiesPtr> /* networks */)
      override {}
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr /* network */)
      override {}
  void OnDeviceStateListChanged() override {}
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}

 private:
  std::string InitializeRequest();
  bool IsEligible(
      const network_config::mojom::NetworkStatePropertiesPtr& network);
  void StartGetNetworkDetails(
      const network_config::mojom::NetworkStateProperties* network,
      const std::string& request_guid);
  void OnGetManagedPropertiesResult(
      sync_pb::WifiConfigurationSpecifics proto,
      const std::string& request_guid,
      network_config::mojom::ManagedPropertiesPtr properties);

  // Callback for shill's GetWiFiPassphrase method.  |proto| should contain the
  // partially filled in proto representation of the network, |is_one_off|
  // should be true when GetSyncableNetwork is used rather than
  // GetAllSyncableNetworks and |passphrase| will come from shill.
  void OnGetWiFiPassphraseResult(sync_pb::WifiConfigurationSpecifics proto,
                                 const std::string& request_guid,
                                 const std::string& passphrase);

  // An empty |request_guid| implies that this is part of a request for a list
  // while a populated |request_guid| implies a one off network request.
  void OnGetWiFiPassphraseError(const NetworkIdentifier& id,
                                const std::string& request_guid,
                                const std::string& error_name,
                                const std::string& error_message);

  void OnNetworkFinished(const NetworkIdentifier& id,
                         const std::string& request_guid);
  void OnRequestFinished(const std::string& request_guid);
  void OnGetNetworkList(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks);

  network_config::mojom::CrosNetworkConfig* cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
  std::vector<network_config::mojom::NetworkStatePropertiesPtr> mojo_networks_;
  base::WeakPtr<NetworkMetadataStore> network_metadata_store_;

  base::flat_map<std::string, std::vector<sync_pb::WifiConfigurationSpecifics>>
      request_guid_to_complete_protos_;
  base::flat_map<std::string, base::flat_set<NetworkIdentifier>>
      request_guid_to_in_flight_networks_;
  base::flat_map<std::string, ProtoCallback> request_guid_to_single_callback_;
  base::flat_map<std::string, ProtoListCallback> request_guid_to_list_callback_;

  base::WeakPtrFactory<LocalNetworkCollectorImpl> weak_ptr_factory_{this};
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_LOCAL_NETWORK_COLLECTOR_IMPL_H_
