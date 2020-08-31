// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/wifi_configuration_bridge.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/components/sync_wifi/local_network_collector.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/network_type_conversions.h"
#include "chromeos/components/sync_wifi/synced_network_metrics_logger.h"
#include "chromeos/components/sync_wifi/synced_network_updater.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_metadata_store.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace sync_wifi {

namespace {

std::unique_ptr<syncer::EntityData> GenerateWifiEntityData(
    const sync_pb::WifiConfigurationSpecifics& proto) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_wifi_configuration()->CopyFrom(proto);
  entity_data->name = NetworkIdentifier::FromProto(proto).SerializeToString();
  return entity_data;
}
}  // namespace

WifiConfigurationBridge::WifiConfigurationBridge(
    SyncedNetworkUpdater* synced_network_updater,
    LocalNetworkCollector* local_network_collector,
    NetworkConfigurationHandler* network_configuration_handler,
    SyncedNetworkMetricsLogger* metrics_recorder,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory create_store_callback)
    : ModelTypeSyncBridge(std::move(change_processor)),
      synced_network_updater_(synced_network_updater),
      local_network_collector_(local_network_collector),
      network_configuration_handler_(network_configuration_handler),
      metrics_recorder_(metrics_recorder),
      network_metadata_store_(nullptr) {
  std::move(create_store_callback)
      .Run(syncer::WIFI_CONFIGURATIONS,
           base::BindOnce(&WifiConfigurationBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
  if (network_configuration_handler_) {
    network_configuration_handler_->AddObserver(this);
  }
}

WifiConfigurationBridge::~WifiConfigurationBridge() {
  if (network_metadata_store_) {
    network_metadata_store_->RemoveObserver(this);
  }
  if (network_configuration_handler_) {
    network_configuration_handler_->RemoveObserver(this);
  }
}

std::unique_ptr<syncer::MetadataChangeList>
WifiConfigurationBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

base::Optional<syncer::ModelError> WifiConfigurationBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList change_list) {
  DCHECK(entries_.empty());
  DCHECK(local_network_collector_);

  local_network_collector_->GetAllSyncableNetworks(
      base::BindOnce(&WifiConfigurationBridge::OnGetAllSyncableNetworksResult,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(metadata_change_list), std::move(change_list)));

  return base::nullopt;
}

void WifiConfigurationBridge::OnGetAllSyncableNetworksResult(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList change_list,
    std::vector<sync_pb::WifiConfigurationSpecifics> local_network_list) {
  // To merge local and synced networks we add all local networks that don't
  // exist in sync to the server and all synced networks that don't exist
  // locally to Shill.  For networks which exist on both lists, we compare the
  // last connected timestamp and take the newer configuration.

  NET_LOG(EVENT) << "Merging " << local_network_list.size() << " local and "
                 << change_list.size() << " synced networks.";
  base::flat_map<NetworkIdentifier, sync_pb::WifiConfigurationSpecifics>
      sync_networks;
  base::flat_map<NetworkIdentifier, sync_pb::WifiConfigurationSpecifics>
      local_networks;

  // Iterate through incoming changes from sync and populate the sync_networks
  // map.
  for (std::unique_ptr<syncer::EntityChange>& change : change_list) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      // Don't delete any local networks during the initial merge when sync is
      // first enabled.
      continue;
    }

    const sync_pb::WifiConfigurationSpecifics& proto =
        change->data().specifics.wifi_configuration();
    NetworkIdentifier id = NetworkIdentifier::FromProto(proto);
    if (sync_networks.contains(id) &&
        sync_networks[id].last_connected_timestamp() >
            proto.last_connected_timestamp()) {
      continue;
    }
    sync_networks[id] = proto;
  }

  // Iterate through local networks and add to sync where appropriate.
  for (sync_pb::WifiConfigurationSpecifics& proto : local_network_list) {
    NetworkIdentifier id = NetworkIdentifier::FromProto(proto);
    if (sync_networks.contains(id) &&
        sync_networks[id].last_connected_timestamp() >
            proto.last_connected_timestamp()) {
      continue;
    }

    local_networks[id] = proto;
    std::unique_ptr<syncer::EntityData> entity_data =
        GenerateWifiEntityData(proto);
    std::string storage_key = GetStorageKey(*entity_data);

    // Upload the local network configuration to sync.  This could be a new
    // configuration or an update to an existing one.
    change_processor()->Put(storage_key, std::move(entity_data),
                            metadata_change_list.get());
    entries_[storage_key] = proto;
  }

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  // Iterate through synced networks and update local stack where appropriate.
  for (auto& kv : sync_networks) {
    NetworkIdentifier& id = kv.first;
    sync_pb::WifiConfigurationSpecifics& proto = kv.second;

    if (local_networks.contains(id) &&
        local_networks[id].last_connected_timestamp() >
            proto.last_connected_timestamp()) {
      continue;
    }

    // Update the local network stack to have the synced network configuration.
    synced_network_updater_->AddOrUpdateNetwork(proto);

    // Save the proto to the sync data store to keep track of all synced
    // networks on device.  This gets loaded into |entries_| next time the
    // bridge is initialized.
    batch->WriteData(id.SerializeToString(), proto.SerializeAsString());
    entries_[id.SerializeToString()] = proto;
  }

  // Mark the changes as processed.
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));
  metrics_recorder_->RecordTotalCount(entries_.size());
}

base::Optional<syncer::ModelError> WifiConfigurationBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  NET_LOG(EVENT) << "Applying  " << entity_changes.size()
                 << " pending changes.";

  // TODO(jonmann) Don't override synced network configurations that are newer
  // than the local configurations.
  for (std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      auto it = entries_.find(change->storage_key());
      if (it != entries_.end()) {
        entries_.erase(it);
        batch->DeleteData(change->storage_key());
        synced_network_updater_->RemoveNetwork(
            NetworkIdentifier::DeserializeFromString(change->storage_key()));
      } else {
        NET_LOG(EVENT) << "Received delete request for network which is not "
                          "tracked by sync.";
      }
      continue;
    }

    auto& specifics = change->data().specifics.wifi_configuration();
    synced_network_updater_->AddOrUpdateNetwork(specifics);

    batch->WriteData(change->storage_key(), specifics.SerializeAsString());
    entries_[change->storage_key()] = std::move(specifics);
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));
  metrics_recorder_->RecordTotalCount(entries_.size());

  return base::nullopt;
}

void WifiConfigurationBridge::GetData(StorageKeyList storage_keys,
                                      DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& id : storage_keys) {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
      continue;
    }
    batch->Put(id, GenerateWifiEntityData(it->second));
  }
  std::move(callback).Run(std::move(batch));
}

void WifiConfigurationBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& entry : entries_) {
    batch->Put(entry.first, GenerateWifiEntityData(entry.second));
  }
  std::move(callback).Run(std::move(batch));
}

std::string WifiConfigurationBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string WifiConfigurationBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return NetworkIdentifier::FromProto(
             entity_data.specifics.wifi_configuration())
      .SerializeToString();
}

void WifiConfigurationBridge::OnStoreCreated(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&WifiConfigurationBridge::OnReadAllData,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::OnReadAllData(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> records) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (syncer::ModelTypeStore::Record& record : *records) {
    sync_pb::WifiConfigurationSpecifics data;
    if (record.id.empty() || !data.ParseFromString(record.value)) {
      NET_LOG(EVENT) << "Unable to parse proto for entry with key: "
                     << record.id;
      continue;
    }
    entries_[record.id] = std::move(data);
  }

  metrics_recorder_->RecordTotalCount(entries_.size());
  store_->ReadAllMetadata(
      base::BindOnce(&WifiConfigurationBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::OnReadAllMetadata(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void WifiConfigurationBridge::OnCommit(
    const base::Optional<syncer::ModelError>& error) {
  if (error)
    change_processor()->ReportError(*error);
}

void WifiConfigurationBridge::Commit(
    std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&WifiConfigurationBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

std::vector<NetworkIdentifier> WifiConfigurationBridge::GetAllIdsForTesting() {
  std::vector<NetworkIdentifier> ids;
  for (const auto& entry : entries_)
    ids.push_back(NetworkIdentifier::FromProto(entry.second));

  return ids;
}

void WifiConfigurationBridge::OnFirstConnectionToNetwork(
    const std::string& guid) {
  if (network_metadata_store_->GetIsConfiguredBySync(guid)) {
    // Don't have to upload a configuration that came from sync.
    NET_LOG(EVENT) << "Not uploading newly configured network "
                   << NetworkGuidId(guid) << ", it was added by sync.";
    return;
  }

  NET_LOG(EVENT) << "Syncing newly configured network " << NetworkGuidId(guid);
  local_network_collector_->GetSyncableNetwork(
      guid, base::BindOnce(&WifiConfigurationBridge::SaveNetworkToSync,
                           weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::OnNetworkUpdate(
    const std::string& guid,
    base::DictionaryValue* set_properties) {
  if (!set_properties)
    return;

  if (network_metadata_store_->GetIsConfiguredBySync(guid)) {
    // Don't have to upload a configuration that came from sync.
    NET_LOG(EVENT) << "Not uploading change to " << NetworkGuidId(guid)
                   << ", modified network was configured "
                      "by sync.";
    return;
  }
  if (!set_properties->HasKey(shill::kAutoConnectProperty) &&
      !set_properties->HasKey(shill::kPriorityProperty) &&
      !set_properties->HasKey(shill::kProxyConfigProperty) &&
      !set_properties->FindPath(
          base::StringPrintf("%s.%s", shill::kStaticIPConfigProperty,
                             shill::kNameServersProperty))) {
    NET_LOG(EVENT) << "Not uploading change to " << NetworkGuidId(guid)
                   << ", modified network field(s) are not synced.";
    return;
  }

  NET_LOG(EVENT) << "Updating sync with changes to " << NetworkGuidId(guid);
  local_network_collector_->GetSyncableNetwork(
      guid, base::BindOnce(&WifiConfigurationBridge::SaveNetworkToSync,
                           weak_ptr_factory_.GetWeakPtr()));
}

void WifiConfigurationBridge::SaveNetworkToSync(
    base::Optional<sync_pb::WifiConfigurationSpecifics> proto) {
  if (!proto) {
    return;
  }

  std::unique_ptr<syncer::EntityData> entity_data =
      GenerateWifiEntityData(*proto);
  auto id = NetworkIdentifier::FromProto(*proto);
  std::string storage_key = GetStorageKey(*entity_data);

  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(storage_key, proto->SerializeAsString());
  change_processor()->Put(storage_key, std::move(entity_data),
                          batch->GetMetadataChangeList());
  entries_[storage_key] = *proto;
  Commit(std::move(batch));
  NET_LOG(EVENT) << "Saved network "
                 << NetworkId(NetworkStateFromNetworkIdentifier(id))
                 << "to sync.";
  metrics_recorder_->RecordTotalCount(entries_.size());
}

void WifiConfigurationBridge::OnBeforeConfigurationRemoved(
    const std::string& service_path,
    const std::string& guid) {
  base::Optional<NetworkIdentifier> id =
      local_network_collector_->GetNetworkIdentifierFromGuid(guid);
  if (!id) {
    return;
  }

  NET_LOG(EVENT) << "Storing metadata for " << NetworkPathId(service_path)
                 << " in preparation for removal.";
  std::string storage_key = id->SerializeToString();
  if (entries_.contains(storage_key))
    pending_deletes_[guid] = storage_key;
}

void WifiConfigurationBridge::OnConfigurationRemoved(
    const std::string& service_path,
    const std::string& network_guid) {
  if (!pending_deletes_.contains(network_guid)) {
    NET_LOG(EVENT) << "Configuraiton " << network_guid
                   << " removed with no matching saved metadata.";
    return;
  }

  std::string storage_key = pending_deletes_[network_guid];
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->DeleteData(storage_key);
  change_processor()->Delete(storage_key, batch->GetMetadataChangeList());
  entries_.erase(storage_key);
  Commit(std::move(batch));
  NET_LOG(EVENT) << "Removed network from sync.";
}

void WifiConfigurationBridge::SetNetworkMetadataStore(
    base::WeakPtr<NetworkMetadataStore> network_metadata_store) {
  if (network_metadata_store_) {
    network_metadata_store->RemoveObserver(this);
  }
  network_metadata_store_ = network_metadata_store;
  network_metadata_store->AddObserver(this);
}

}  // namespace sync_wifi

}  // namespace chromeos
