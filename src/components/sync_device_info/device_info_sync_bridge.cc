// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_sync_bridge.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "components/sync/base/time.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/local_device_info_util.h"

namespace syncer {

using base::Time;
using base::TimeDelta;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;
using sync_pb::FeatureSpecificFields;
using sync_pb::ModelTypeState;
using sync_pb::SharingSpecificFields;

using Record = ModelTypeStore::Record;
using RecordList = ModelTypeStore::RecordList;
using WriteBatch = ModelTypeStore::WriteBatch;
using ClientIdToSpecifics =
    std::map<std::string, std::unique_ptr<sync_pb::DeviceInfoSpecifics>>;

namespace {

constexpr base::TimeDelta kExpirationThreshold = base::TimeDelta::FromDays(56);

// Find the timestamp for the last time this |device_info| was edited.
Time GetLastUpdateTime(const DeviceInfoSpecifics& specifics) {
  if (specifics.has_last_updated_timestamp()) {
    return ProtoTimeToTime(specifics.last_updated_timestamp());
  } else {
    return Time();
  }
}

TimeDelta GetPulseIntervalFromSpecifics(const DeviceInfoSpecifics& specifics) {
  if (specifics.has_pulse_interval_in_minutes()) {
    return TimeDelta::FromMinutes(specifics.pulse_interval_in_minutes());
  }
  // If the interval is not set on the specifics it must be an old device, so we
  // fall back to the value used by old devices. We really do not want to use
  // the default int value of 0.
  return TimeDelta::FromDays(1);
}

base::Optional<DeviceInfo::SharingInfo> SpecificsToSharingInfo(
    const DeviceInfoSpecifics& specifics) {
  if (!specifics.has_sharing_fields()) {
    return base::nullopt;
  }

  std::set<SharingSpecificFields::EnabledFeatures> enabled_features;
  for (int i = 0; i < specifics.sharing_fields().enabled_features_size(); ++i) {
    enabled_features.insert(specifics.sharing_fields().enabled_features(i));
  }
  return DeviceInfo::SharingInfo(
      {specifics.sharing_fields().vapid_fcm_token(),
       specifics.sharing_fields().vapid_p256dh(),
       specifics.sharing_fields().vapid_auth_secret()},
      {specifics.sharing_fields().sender_id_fcm_token_v2(),
       specifics.sharing_fields().sender_id_p256dh_v2(),
       specifics.sharing_fields().sender_id_auth_secret_v2()},
      std::move(enabled_features));
}

// Converts DeviceInfoSpecifics into a freshly allocated DeviceInfo.
std::unique_ptr<DeviceInfo> SpecificsToModel(
    const DeviceInfoSpecifics& specifics) {
  base::SysInfo::HardwareInfo hardware_info;
  hardware_info.model = specifics.model();
  hardware_info.manufacturer = specifics.manufacturer();

  return std::make_unique<DeviceInfo>(
      specifics.cache_guid(), specifics.client_name(),
      specifics.chrome_version(), specifics.sync_user_agent(),
      specifics.device_type(), specifics.signin_scoped_device_id(),
      hardware_info, ProtoTimeToTime(specifics.last_updated_timestamp()),
      GetPulseIntervalFromSpecifics(specifics),
      specifics.feature_fields().send_tab_to_self_receiving_enabled(),
      SpecificsToSharingInfo(specifics));
}

// Allocate a EntityData and copies |specifics| into it.
std::unique_ptr<EntityData> CopyToEntityData(
    const DeviceInfoSpecifics& specifics) {
  auto entity_data = std::make_unique<EntityData>();
  *entity_data->specifics.mutable_device_info() = specifics;
  entity_data->name = specifics.client_name();
  return entity_data;
}

// Converts a local DeviceInfo into a freshly allocated DeviceInfoSpecifics.
std::unique_ptr<DeviceInfoSpecifics> MakeLocalDeviceSpecifics(
    const DeviceInfo& info) {
  auto hardware_info = info.hardware_info();
  auto specifics = std::make_unique<DeviceInfoSpecifics>();
  specifics->set_cache_guid(info.guid());
  specifics->set_client_name(info.client_name());
  specifics->set_chrome_version(info.chrome_version());
  specifics->set_sync_user_agent(info.sync_user_agent());
  specifics->set_device_type(info.device_type());
  specifics->set_signin_scoped_device_id(info.signin_scoped_device_id());
  specifics->set_model(hardware_info.model);
  specifics->set_manufacturer(hardware_info.manufacturer);
  // The local device should have not been updated yet. Set the last updated
  // timestamp to now.
  DCHECK(info.last_updated_timestamp() == base::Time());
  specifics->set_last_updated_timestamp(TimeToProtoTime(Time::Now()));
  specifics->set_pulse_interval_in_minutes(info.pulse_interval().InMinutes());

  FeatureSpecificFields* feature_fields = specifics->mutable_feature_fields();
  feature_fields->set_send_tab_to_self_receiving_enabled(
      info.send_tab_to_self_receiving_enabled());

  const base::Optional<DeviceInfo::SharingInfo>& sharing_info =
      info.sharing_info();
  if (sharing_info) {
    SharingSpecificFields* sharing_fields = specifics->mutable_sharing_fields();
    sharing_fields->set_vapid_fcm_token(
        sharing_info->vapid_target_info.fcm_token);
    sharing_fields->set_vapid_p256dh(sharing_info->vapid_target_info.p256dh);
    sharing_fields->set_vapid_auth_secret(
        sharing_info->vapid_target_info.auth_secret);
    sharing_fields->set_sender_id_fcm_token_v2(
        sharing_info->sender_id_target_info.fcm_token);
    sharing_fields->set_sender_id_p256dh_v2(
        sharing_info->sender_id_target_info.p256dh);
    sharing_fields->set_sender_id_auth_secret_v2(
        sharing_info->sender_id_target_info.auth_secret);
    for (sync_pb::SharingSpecificFields::EnabledFeatures feature :
         sharing_info->enabled_features) {
      sharing_fields->add_enabled_features(feature);
    }
  }

  return specifics;
}

// Parses the content of |record_list| into |*all_data|. The output
// parameter is first for binding purposes.
base::Optional<ModelError> ParseSpecificsOnBackendSequence(
    ClientIdToSpecifics* all_data,
    std::string* local_personalizable_device_name,
    std::unique_ptr<ModelTypeStore::RecordList> record_list) {
  DCHECK(all_data);
  DCHECK(all_data->empty());
  DCHECK(local_personalizable_device_name);
  DCHECK(record_list);

  // For convenience, we get the user personalized local device name here,
  // since we're running on the backend sequence, because the function is
  // blocking.
  *local_personalizable_device_name = GetPersonalizableDeviceNameBlocking();

  for (const Record& r : *record_list) {
    std::unique_ptr<DeviceInfoSpecifics> specifics =
        std::make_unique<DeviceInfoSpecifics>();
    if (!specifics->ParseFromString(r.value)) {
      return ModelError(FROM_HERE, "Failed to deserialize specifics.");
    }

    all_data->emplace(specifics->cache_guid(), std::move(specifics));
  }

  return base::nullopt;
}

}  // namespace

DeviceInfoSyncBridge::DeviceInfoSyncBridge(
    std::unique_ptr<MutableLocalDeviceInfoProvider> local_device_info_provider,
    OnceModelTypeStoreFactory store_factory,
    std::unique_ptr<ModelTypeChangeProcessor> change_processor,
    std::unique_ptr<DeviceInfoPrefs> device_info_prefs)
    : ModelTypeSyncBridge(std::move(change_processor)),
      local_device_info_provider_(std::move(local_device_info_provider)),
      device_info_prefs_(std::move(device_info_prefs)) {
  DCHECK(local_device_info_provider_);
  DCHECK(device_info_prefs_);

  // Provider must not be initialized, the bridge takes care.
  DCHECK(!local_device_info_provider_->GetLocalDeviceInfo());

  std::move(store_factory)
      .Run(DEVICE_INFO, base::BindOnce(&DeviceInfoSyncBridge::OnStoreCreated,
                                       weak_ptr_factory_.GetWeakPtr()));
}

DeviceInfoSyncBridge::~DeviceInfoSyncBridge() {}

LocalDeviceInfoProvider* DeviceInfoSyncBridge::GetLocalDeviceInfoProvider() {
  return local_device_info_provider_.get();
}

void DeviceInfoSyncBridge::RefreshLocalDeviceInfo() {
  SendLocalData();
}

void DeviceInfoSyncBridge::OnSyncStarting(
    const DataTypeActivationRequest& request) {
  // Store the cache GUID, mainly in case MergeSyncData() is executed later.
  local_cache_guid_ = request.cache_guid;
  // Garbage-collect old local cache GUIDs, for privacy reasons.
  device_info_prefs_->GarbageCollectExpiredCacheGuids();
  // Add the cache guid to the local prefs.
  device_info_prefs_->AddLocalCacheGuid(local_cache_guid_);
  // SyncMode determines the client name in GetLocalClientName().
  sync_mode_ = request.sync_mode;

  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  // Local device's client name needs to updated if OnSyncStarting is called
  // after local device has already been initialized since the client name
  // depends on |sync_mode_|.
  local_device_info_provider_->UpdateClientName(GetLocalClientName());
  ReconcileLocalAndStored();
}

std::unique_ptr<MetadataChangeList>
DeviceInfoSyncBridge::CreateMetadataChangeList() {
  return WriteBatch::CreateMetadataChangeList();
}

base::Optional<ModelError> DeviceInfoSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(all_data_.empty());
  DCHECK(!local_cache_guid_.empty());

  local_device_info_provider_->Initialize(
      local_cache_guid_, GetLocalClientName(), local_hardware_info_);

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const auto& change : entity_data) {
    const DeviceInfoSpecifics& specifics =
        change->data().specifics.device_info();
    DCHECK_EQ(change->storage_key(), specifics.cache_guid());

    // Each device is the authoritative source for itself, ignore any remote
    // changes that have a cache guid that is or was this local device.
    if (device_info_prefs_->IsRecentLocalCacheGuid(change->storage_key())) {
      continue;
    }

    StoreSpecifics(std::make_unique<DeviceInfoSpecifics>(specifics),
                   batch.get());
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  // Complete batch with local data and commit.
  SendLocalDataWithBatch(std::move(batch));
  return base::nullopt;
}

base::Optional<ModelError> DeviceInfoSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  bool has_changes = false;
  for (const std::unique_ptr<EntityChange>& change : entity_changes) {
    const std::string guid = change->storage_key();
    // Each device is the authoritative source for itself, ignore any remote
    // changes that have a cache guid that is or was this local device.
    if (device_info_prefs_->IsRecentLocalCacheGuid(guid)) {
      continue;
    }

    if (change->type() == EntityChange::ACTION_DELETE) {
      // This should never get exercised as no client issues tombstones.
      has_changes |= DeleteSpecifics(guid, batch.get());
    } else {
      const DeviceInfoSpecifics& specifics =
          change->data().specifics.device_info();
      DCHECK(guid == specifics.cache_guid());
      StoreSpecifics(std::make_unique<DeviceInfoSpecifics>(specifics),
                     batch.get());
      has_changes = true;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  CommitAndNotify(std::move(batch), has_changes);
  return base::nullopt;
}

void DeviceInfoSyncBridge::GetData(StorageKeyList storage_keys,
                                   DataCallback callback) {
  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& key : storage_keys) {
    const auto& iter = all_data_.find(key);
    if (iter != all_data_.end()) {
      DCHECK_EQ(key, iter->second->cache_guid());
      batch->Put(key, CopyToEntityData(*iter->second));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void DeviceInfoSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& kv : all_data_) {
    batch->Put(kv.first, CopyToEntityData(*kv.second));
  }
  std::move(callback).Run(std::move(batch));
}

std::string DeviceInfoSyncBridge::GetClientTag(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return DeviceInfoUtil::SpecificsToTag(entity_data.specifics.device_info());
}

std::string DeviceInfoSyncBridge::GetStorageKey(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return entity_data.specifics.device_info().cache_guid();
}

void DeviceInfoSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  if (!delete_metadata_change_list) {
    return;
  }

  // Sync is being disabled, so the local DeviceInfo is no longer valid and
  // should be cleared.
  local_device_info_provider_->Clear();
  local_cache_guid_.clear();
  pulse_timer_.Stop();

  // Remove all local data, if sync is being disabled, the user has expressed
  // their desire to not have knowledge about other devices.
  store_->DeleteAllDataAndMetadata(base::DoNothing());
  if (!all_data_.empty()) {
    all_data_.clear();
    NotifyObservers();
  }
}

bool DeviceInfoSyncBridge::IsSyncing() const {
  return !all_data_.empty();
}

std::unique_ptr<DeviceInfo> DeviceInfoSyncBridge::GetDeviceInfo(
    const std::string& client_id) const {
  const ClientIdToSpecifics::const_iterator iter = all_data_.find(client_id);
  if (iter == all_data_.end()) {
    return nullptr;
  }
  return SpecificsToModel(*iter->second);
}

std::vector<std::unique_ptr<DeviceInfo>>
DeviceInfoSyncBridge::GetAllDeviceInfo() const {
  std::vector<std::unique_ptr<DeviceInfo>> list;
  for (auto iter = all_data_.begin(); iter != all_data_.end(); ++iter) {
    list.push_back(SpecificsToModel(*iter->second));
  }
  return list;
}

void DeviceInfoSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceInfoSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

int DeviceInfoSyncBridge::CountActiveDevices() const {
  return CountActiveDevices(Time::Now());
}

bool DeviceInfoSyncBridge::IsRecentLocalCacheGuid(
    const std::string& cache_guid) const {
  return device_info_prefs_->IsRecentLocalCacheGuid(cache_guid);
}

bool DeviceInfoSyncBridge::IsPulseTimerRunningForTest() const {
  return pulse_timer_.IsRunning();
}

void DeviceInfoSyncBridge::ForcePulseForTest() {
  SendLocalData();
}

void DeviceInfoSyncBridge::NotifyObservers() {
  for (auto& observer : observers_)
    observer.OnDeviceInfoChange();
}

void DeviceInfoSyncBridge::StoreSpecifics(
    std::unique_ptr<DeviceInfoSpecifics> specifics,
    WriteBatch* batch) {
  const std::string guid = specifics->cache_guid();
  batch->WriteData(guid, specifics->SerializeAsString());
  all_data_[guid] = std::move(specifics);
}

bool DeviceInfoSyncBridge::DeleteSpecifics(const std::string& guid,
                                           WriteBatch* batch) {
  ClientIdToSpecifics::const_iterator iter = all_data_.find(guid);
  if (iter != all_data_.end()) {
    batch->DeleteData(guid);
    all_data_.erase(iter);
    return true;
  } else {
    return false;
  }
}

std::string DeviceInfoSyncBridge::GetLocalClientName() const {
  // |sync_mode_| may not be ready when this function is called.
  if (!sync_mode_) {
    auto device_it = all_data_.find(local_cache_guid_);
    if (device_it != all_data_.end()) {
      return device_it->second->client_name();
    }
  }

  if (sync_mode_ == SyncMode::kFull) {
    return local_personalizable_device_name_;
  }

  return local_hardware_info_.model;
}

void DeviceInfoSyncBridge::OnStoreCreated(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);

  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&DeviceInfoSyncBridge::OnHardwareInfoRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceInfoSyncBridge::OnHardwareInfoRetrieved(
    base::SysInfo::HardwareInfo hardware_info) {
  local_hardware_info_ = std::move(hardware_info);

#if defined(OS_CHROMEOS)
  // For ChromeOS the returned model values are product code names like Eve. We
  // want to use generic names like Chromebook.
  local_hardware_info_.model = GetChromeOSDeviceNameFromType();
#endif

  auto all_data = std::make_unique<ClientIdToSpecifics>();
  ClientIdToSpecifics* all_data_copy = all_data.get();

  auto local_personalizable_device_name = std::make_unique<std::string>();
  std::string* local_personalizable_device_name_copy =
      local_personalizable_device_name.get();

  store_->ReadAllDataAndPreprocess(
      base::BindOnce(&ParseSpecificsOnBackendSequence,
                     base::Unretained(all_data_copy),
                     base::Unretained(local_personalizable_device_name_copy)),
      base::BindOnce(&DeviceInfoSyncBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(all_data),
                     std::move(local_personalizable_device_name)));
}

void DeviceInfoSyncBridge::OnReadAllData(
    std::unique_ptr<ClientIdToSpecifics> all_data,
    std::unique_ptr<std::string> local_personalizable_device_name,
    const base::Optional<syncer::ModelError>& error) {
  DCHECK(all_data);
  DCHECK(local_personalizable_device_name);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  all_data_ = std::move(*all_data);

  local_personalizable_device_name_ =
      std::move(*local_personalizable_device_name);

  store_->ReadAllMetadata(
      base::BindOnce(&DeviceInfoSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceInfoSyncBridge::OnReadAllMetadata(
    const base::Optional<ModelError>& error,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  // In the regular case for sync being disabled, wait for MergeSyncData()
  // before initializing the LocalDeviceInfoProvider.
  if (!metadata_batch->GetModelTypeState().initial_sync_done() &&
      metadata_batch->GetAllMetadata().empty() && all_data_.empty()) {
    change_processor()->ModelReadyToSync(std::move(metadata_batch));
    return;
  }

  const std::string local_cache_guid_in_metadata =
      metadata_batch->GetModelTypeState().cache_guid();

  // Protect against corrupt local data.
  if (!metadata_batch->GetModelTypeState().initial_sync_done() ||
      local_cache_guid_in_metadata.empty() ||
      all_data_.count(local_cache_guid_in_metadata) == 0) {
    // Data or metadata is off. Just throw everything away and start clean.
    all_data_.clear();
    store_->DeleteAllDataAndMetadata(base::DoNothing());
    change_processor()->ModelReadyToSync(std::make_unique<MetadataBatch>());
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  // In rare cases a mismatch between cache GUIDs should cause all sync metadata
  // dropped. In that case, MergeSyncData() will eventually follow.
  if (!change_processor()->IsTrackingMetadata()) {
    // In this scenario, ApplyStopSyncChanges() should have been exercised.
    DCHECK(local_cache_guid_.empty());
    DCHECK(all_data_.empty());
    return;
  }

  // If OnSyncStarting() was already called then cache GUID must be the same.
  // Otherwise IsTrackingMetadata would return false due to cache GUID mismatch.
  DCHECK(local_cache_guid_.empty() ||
         local_cache_guid_ == local_cache_guid_in_metadata);
  // If sync already enabled (usual case without data corruption), we can
  // initialize the provider immediately.
  local_cache_guid_ = local_cache_guid_in_metadata;
  local_device_info_provider_->Initialize(
      local_cache_guid_, GetLocalClientName(), local_hardware_info_);

  // This probably isn't strictly needed, but in case the cache_guid has changed
  // we save the new one to prefs.
  device_info_prefs_->AddLocalCacheGuid(local_cache_guid_);
  ExpireOldEntries();
  ReconcileLocalAndStored();
}

void DeviceInfoSyncBridge::OnCommit(
    const base::Optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void DeviceInfoSyncBridge::ReconcileLocalAndStored() {
  const DeviceInfo* current_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  DCHECK(current_info);

  auto iter = all_data_.find(current_info->guid());
  DCHECK(iter != all_data_.end());

  // Convert to DeviceInfo for Equals function.
  if (current_info->Equals(*SpecificsToModel(*iter->second))) {
    const TimeDelta pulse_delay(DeviceInfoUtil::CalculatePulseDelay(
        GetLastUpdateTime(*iter->second), Time::Now()));
    if (!pulse_delay.is_zero()) {
      pulse_timer_.Start(FROM_HERE, pulse_delay,
                         base::BindOnce(&DeviceInfoSyncBridge::SendLocalData,
                                        base::Unretained(this)));
      return;
    }
  }
  SendLocalData();
}

void DeviceInfoSyncBridge::SendLocalData() {
  SendLocalDataWithBatch(store_->CreateWriteBatch());
}

void DeviceInfoSyncBridge::SendLocalDataWithBatch(
    std::unique_ptr<ModelTypeStore::WriteBatch> batch) {
  DCHECK(store_);
  DCHECK(local_device_info_provider_->GetLocalDeviceInfo());
  DCHECK(change_processor()->IsTrackingMetadata());

  std::unique_ptr<DeviceInfoSpecifics> specifics = MakeLocalDeviceSpecifics(
      *local_device_info_provider_->GetLocalDeviceInfo());
  change_processor()->Put(specifics->cache_guid(), CopyToEntityData(*specifics),
                          batch->GetMetadataChangeList());
  StoreSpecifics(std::move(specifics), batch.get());
  CommitAndNotify(std::move(batch), /*should_notify=*/true);

  pulse_timer_.Start(FROM_HERE, DeviceInfoUtil::GetPulseInterval(),
                     base::BindOnce(&DeviceInfoSyncBridge::SendLocalData,
                                    base::Unretained(this)));
}

void DeviceInfoSyncBridge::CommitAndNotify(std::unique_ptr<WriteBatch> batch,
                                           bool should_notify) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&DeviceInfoSyncBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
  if (should_notify) {
    NotifyObservers();
  }
}

int DeviceInfoSyncBridge::CountActiveDevices(const Time now) const {
  // The algorithm below leverages sync timestamps to give a tight lower bound
  // (modulo clock skew) on how many distinct devices are currently active
  // (where active means being used recently enough as specified by
  // DeviceInfoUtil::kActiveThreshold).
  //
  // Devices of the same type that have no overlap between their time-of-use are
  // likely to be the same device (just with a different cache GUID). Thus, the
  // algorithm counts, for each device type separately, the maximum number of
  // devices observed concurrently active. Then returns the maximum.

  // The series of relevant events over time, the value being +1 when a device
  // was seen for the first time, and -1 when a device was seen last.
  std::map<sync_pb::SyncEnums_DeviceType, std::multimap<base::Time, int>>
      relevant_events;

  for (const auto& pair : all_data_) {
    if (DeviceInfoUtil::IsActive(GetLastUpdateTime(*pair.second), now)) {
      base::Time begin = change_processor()->GetEntityCreationTime(pair.first);
      base::Time end =
          change_processor()->GetEntityModificationTime(pair.first);
      // Begin/end timestamps are received from other devices without local
      // sanitizing, so potentially the timestamps could be malformed, and the
      // modification time may predate the creation time.
      if (begin > end) {
        continue;
      }
      relevant_events[pair.second->device_type()].emplace(begin, 1);
      relevant_events[pair.second->device_type()].emplace(end, -1);
    }
  }

  int max_overlapping_sum = 0;
  for (const auto& type_and_events : relevant_events) {
    int max_overlapping = 0;
    int overlapping = 0;
    for (const auto& event : type_and_events.second) {
      overlapping += event.second;
      DCHECK_LE(0, overlapping);
      max_overlapping = std::max(max_overlapping, overlapping);
    }
    DCHECK_EQ(overlapping, 0);
    max_overlapping_sum += max_overlapping;
  }

  return max_overlapping_sum;
}

void DeviceInfoSyncBridge::ExpireOldEntries() {
  const base::Time expiration_threshold =
      base::Time::Now() - kExpirationThreshold;
  std::unordered_set<std::string> cache_guids_to_expire;
  // Just collecting cache guids to expire to avoid modifying |all_data_| via
  // DeleteSpecifics() while iterating over it.
  for (const auto& pair : all_data_) {
    const std::string& cache_guid = pair.first;
    if (cache_guid != local_cache_guid_ &&
        GetLastUpdateTime(*pair.second) < expiration_threshold) {
      cache_guids_to_expire.insert(cache_guid);
    }
  }

  if (cache_guids_to_expire.empty()) {
    return;
  }

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const std::string& cache_guid : cache_guids_to_expire) {
    DeleteSpecifics(cache_guid, batch.get());
    batch->GetMetadataChangeList()->ClearMetadata(cache_guid);
    change_processor()->UntrackEntityForStorageKey(cache_guid);
  }
  CommitAndNotify(std::move(batch), /*should_notify=*/true);
}

}  // namespace syncer
