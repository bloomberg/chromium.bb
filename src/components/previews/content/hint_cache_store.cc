// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/hint_cache_store.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/previews/content/proto/hint_cache.pb.h"

namespace previews {

namespace {

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.PreviewsHintCacheStore. Changing this needs
// synchronization with histograms.xml.
constexpr char kHintCacheStoreUMAClientName[] = "PreviewsHintCacheStore";

// The folder where the data will be stored on disk.
constexpr char kHintCacheStoreFolder[] = "previews_hint_cache_store";

// The amount of data to build up in memory before converting to a sorted on-
// disk file.
constexpr size_t kDatabaseWriteBufferSizeBytes = 128 * 1024;

// Delimiter that appears between the sections of a store entry key.
//  Examples:
//    "[EntryType::kMetadata]_[MetadataType]"
//    "[EntryType::kComponentHint]_[component_version]_[host]"
constexpr char kKeySectionDelimiter = '_';

// Enumerates the possible outcomes of loading metadata. Used in UMA histograms,
// so the order of enumerators should not be changed.
//
// Keep in sync with PreviewsHintCacheLevelDBStoreProcessMetadataResult in
// tools/metrics/histograms/enums.xml.
enum class PreviewsHintCacheLevelDBStoreLoadMetadataResult {
  kSuccess = 0,
  kLoadMetadataFailed = 1,
  kSchemaMetadataMissing = 2,
  kSchemaMetadataWrongVersion = 3,
  kComponentMetadataMissing = 4,
  kFetchedMetadataMissing = 5,
  kComponentAndFetchedMetadataMissing = 6,
  kMaxValue = kComponentAndFetchedMetadataMissing,
};

// Util class for recording the result of loading the metadata. The result is
// recorded when it goes out of scope and its destructor is called.
class ScopedLoadMetadataResultRecorder {
 public:
  ScopedLoadMetadataResultRecorder()
      : result_(PreviewsHintCacheLevelDBStoreLoadMetadataResult::kSuccess) {}
  ~ScopedLoadMetadataResultRecorder() {
    UMA_HISTOGRAM_ENUMERATION(
        "Previews.HintCacheLevelDBStore.LoadMetadataResult", result_);
  }

  void set_result(PreviewsHintCacheLevelDBStoreLoadMetadataResult result) {
    result_ = result;
  }

 private:
  PreviewsHintCacheLevelDBStoreLoadMetadataResult result_;
};

void RecordStatusChange(HintCacheStore::Status status) {
  UMA_HISTOGRAM_ENUMERATION("Previews.HintCacheLevelDBStore.Status", status);
}

// Returns true if |key_prefix| is a prefix of |key|.
bool DatabasePrefixFilter(const std::string& key_prefix,
                          const std::string& key) {
  return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
}

}  // namespace

HintCacheStore::HintCacheStore(
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> store_task_runner)
    : HintCacheStore(database_dir,
                     leveldb_proto::ProtoDatabaseProvider::CreateUniqueDB<
                         previews::proto::StoreEntry>(store_task_runner)) {}

HintCacheStore::HintCacheStore(
    const base::FilePath& database_dir,
    std::unique_ptr<leveldb_proto::ProtoDatabase<previews::proto::StoreEntry>>
        database)
    : database_dir_(database_dir),
      database_(std::move(database)),
      status_(Status::kUninitialized),
      component_data_update_in_flight_(false),
      weak_ptr_factory_(this) {
  RecordStatusChange(status_);
}

HintCacheStore::~HintCacheStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HintCacheStore::Initialize(bool purge_existing_data,
                                base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateStatus(Status::kInitializing);

  // Asynchronously initialize the store and run the callback once
  // initialization completes. Initialization consists of the following steps:
  //   1. Initialize the database.
  //   2. If |purge_existing_data| is set to true, unconditionally purge
  //      database and skip to step 6.
  //   3. Otherwise, retrieve the metadata entries (e.g. Schema and Component).
  //   4. If schema is the wrong version, purge database and skip to step 6.
  //   5. Otherwise, load all hint entry keys.
  //   6. Run callback after purging database or retrieving hint entry keys.

  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  base::FilePath hint_store_dir =
      database_dir_.AppendASCII(kHintCacheStoreFolder);
  database_->Init(kHintCacheStoreUMAClientName, hint_store_dir, options,
                  base::BindOnce(&HintCacheStore::OnDatabaseInitialized,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 purge_existing_data, std::move(callback)));
}

std::unique_ptr<HintCacheStore::ComponentUpdateData>
HintCacheStore::MaybeCreateComponentUpdateData(
    const base::Version& version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(version.IsValid());

  if (!IsAvailable()) {
    return nullptr;
  }

  // Component updates are only permitted when the update version is newer than
  // the store's current one.
  if (component_version_.has_value() && version <= component_version_) {
    return nullptr;
  }

  // Create and return a LevelDBComponentUpdateData object. This object has
  // hints from the component moved into it and organizes them in a format
  // usable by the store; the object will returned to the store in
  // UpdateComponentData().
  return std::make_unique<LevelDBComponentUpdateData>(version);
}

std::unique_ptr<HintCacheStore::ComponentUpdateData>
HintCacheStore::CreateUpdateDataForFetchedHints(base::Time update_time) const {
  // TODO(mcrouse): Currently returns a LevelDBComponentUpdateData, future
  // refactor will enable the construction of UpdateData with the settings
  // necessary for the type of hint being updated, fetched or component.
  return std::make_unique<LevelDBComponentUpdateData>(update_time);
}

void HintCacheStore::UpdateComponentData(
    std::unique_ptr<HintCacheStore::ComponentUpdateData> component_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(component_data);
  DCHECK(!component_data_update_in_flight_);

  if (!IsAvailable()) {
    std::move(callback).Run();
    return;
  }

  // If this isn't a newer component version than the store's current one, then
  // the simply return. There's nothing to update.
  if (component_version_.has_value() &&
      component_data->version() <= component_version_) {
    std::move(callback).Run();
    return;
  }

  // Mark that there's now a component data update in-flight. While this is
  // true, keys and hints will not be returned by the store.
  component_data_update_in_flight_ = true;

  // Set the component version prior to requesting the update. This ensures that
  // a second update request for the same component version won't be allowed. In
  // the case where the update fails, the store will become unavailable, so it's
  // safe to treat component version in the update as the new one.
  SetComponentVersion(component_data->version());

  // The current keys are about to be removed, so clear out the keys available
  // within the store. The keys will be populated after the component data
  // update completes.
  hint_entry_keys_.reset();

  LevelDBComponentUpdateData* leveldb_component_data =
      static_cast<LevelDBComponentUpdateData*>(component_data.get());

  // Purge any component hints that are missing the new component version
  // prefix.
  EntryKeyPrefix retain_prefix =
      GetComponentHintEntryKeyPrefix(component_version_.value());
  EntryKeyPrefix filter_prefix = GetComponentHintEntryKeyPrefixWithoutVersion();

  // Add the new component data and purge any old component hints from the db.
  // After processing finishes, OnUpdateComponentData() is called, which loads
  // the updated hint entry keys from the database.
  database_->UpdateEntriesWithRemoveFilter(
      std::move(leveldb_component_data->entries_to_save_),
      base::BindRepeating(
          [](const EntryKeyPrefix& retain_prefix,
             const EntryKeyPrefix& filter_prefix, const std::string& key) {
            return key.compare(0, retain_prefix.length(), retain_prefix) != 0 &&
                   key.compare(0, filter_prefix.length(), filter_prefix) == 0;
          },
          retain_prefix, filter_prefix),
      base::BindOnce(&HintCacheStore::OnUpdateComponentData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HintCacheStore::UpdateFetchedHintsData(
    std::unique_ptr<ComponentUpdateData> fetched_hints_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(fetched_hints_data);
  DCHECK(!component_data_update_in_flight_);

  if (!IsAvailable()) {
    std::move(callback).Run();
    return;
  }

  fetched_update_time_ = fetched_hints_data->update_time();

  component_data_update_in_flight_ = true;

  hint_entry_keys_.reset();

  LevelDBComponentUpdateData* leveldb_fetched_hints_data =
      static_cast<LevelDBComponentUpdateData*>(fetched_hints_data.get());

  // This will remove the fetched metadata entry and insert all the entries
  // currently in |leveldb_fetched_hints_data|.
  database_->UpdateEntriesWithRemoveFilter(
      std::move(leveldb_fetched_hints_data->entries_to_save_),
      base::BindRepeating(&DatabasePrefixFilter,
                          GetMetadataTypeEntryKey(MetadataType::kFetched)),
      base::BindOnce(&HintCacheStore::OnUpdateComponentData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool HintCacheStore::FindHintEntryKey(const std::string& host_suffix,
                                      EntryKey* out_hint_entry_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!component_version_.has_value() ||
         component_hint_entry_key_prefix_ ==
             GetComponentHintEntryKeyPrefix(component_version_.value()));

  // Search for a component hint entry with the host suffix.
  *out_hint_entry_key = component_hint_entry_key_prefix_ + host_suffix;
  if (hint_entry_keys_ &&
      hint_entry_keys_->find(*out_hint_entry_key) != hint_entry_keys_->end()) {
    return true;
  }
  return false;
}

void HintCacheStore::LoadHint(const EntryKey& hint_entry_key,
                              HintLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsAvailable()) {
    std::move(callback).Run(hint_entry_key, nullptr);
    return;
  }

  database_->GetEntry(hint_entry_key,
                      base::BindOnce(&HintCacheStore::OnLoadHint,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     hint_entry_key, std::move(callback)));
}

base::Time HintCacheStore::FetchedHintsUpdateTime() const {
  // If the store is not available, the metadata entries have not been loaded
  // so there are no fetched hints.
  if (!IsAvailable())
    return base::Time();
  return fetched_update_time_;
}

HintCacheStore::LevelDBComponentUpdateData::LevelDBComponentUpdateData(
    const base::Version& version)
    : ComponentUpdateData(version),
      component_hint_entry_key_prefix_(GetComponentHintEntryKeyPrefix(version)),
      entries_to_save_(std::make_unique<EntryVector>()) {
  // Add a component metadata entry to the update data with the component's
  // version.
  previews::proto::StoreEntry metadata_component_entry;
  metadata_component_entry.set_version(version.GetString());
  entries_to_save_->emplace_back(
      GetMetadataTypeEntryKey(MetadataType::kComponent),
      std::move(metadata_component_entry));
}

HintCacheStore::LevelDBComponentUpdateData::LevelDBComponentUpdateData(
    base::Time update_time)
    : ComponentUpdateData(update_time),
      component_hint_entry_key_prefix_(GetFetchedHintEntryKeyPrefix()),
      entries_to_save_(std::make_unique<EntryVector>()) {
  // Add fetched metadata entry
  previews::proto::StoreEntry metadata_fetched_entry;
  metadata_fetched_entry.set_update_time_secs(
      update_time.ToDeltaSinceWindowsEpoch().InSeconds());

  entries_to_save_->emplace_back(
      GetMetadataTypeEntryKey(MetadataType::kFetched),
      std::move(metadata_fetched_entry));
}

HintCacheStore::LevelDBComponentUpdateData::~LevelDBComponentUpdateData() =
    default;

void HintCacheStore::LevelDBComponentUpdateData::MoveHintIntoUpdateData(
    optimization_guide::proto::Hint&& hint) {
  // Add a component hint entry to the update data. To avoid any unnecessary
  // copying, the hint is moved into proto::StoreEntry.
  EntryKey hint_entry_key = component_hint_entry_key_prefix_ + hint.key();
  previews::proto::StoreEntry entry_proto;
  entry_proto.set_allocated_hint(
      new optimization_guide::proto::Hint(std::move(hint)));
  entries_to_save_->emplace_back(std::move(hint_entry_key),
                                 std::move(entry_proto));
}

// static
const char HintCacheStore::kStoreSchemaVersion[] = "1";

// static
HintCacheStore::EntryKeyPrefix HintCacheStore::GetMetadataEntryKeyPrefix() {
  return std::to_string(static_cast<int>(EntryType::kMetadata)) +
         kKeySectionDelimiter;
}

// static
HintCacheStore::EntryKey HintCacheStore::GetMetadataTypeEntryKey(
    MetadataType metadata_type) {
  return GetMetadataEntryKeyPrefix() +
         std::to_string(static_cast<int>(metadata_type));
}

// static
HintCacheStore::EntryKeyPrefix
HintCacheStore::GetComponentHintEntryKeyPrefixWithoutVersion() {
  return std::to_string(static_cast<int>(EntryType::kComponentHint)) +
         kKeySectionDelimiter;
}

// static
HintCacheStore::EntryKeyPrefix HintCacheStore::GetComponentHintEntryKeyPrefix(
    const base::Version& component_version) {
  return GetComponentHintEntryKeyPrefixWithoutVersion() +
         component_version.GetString() + kKeySectionDelimiter;
}

// static
HintCacheStore::EntryKeyPrefix HintCacheStore::GetFetchedHintEntryKeyPrefix() {
  return base::NumberToString(static_cast<int>(EntryType::kFetchedHint)) +
         kKeySectionDelimiter;
}

void HintCacheStore::UpdateStatus(Status new_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Status::kUninitialized can only transition to Status::kInitializing.
  DCHECK(status_ != Status::kUninitialized ||
         new_status == Status::kInitializing);
  // Status::kInitializing can only transition to Status::kAvailable or
  // Status::kFailed.
  DCHECK(status_ != Status::kInitializing || new_status == Status::kAvailable ||
         new_status == Status::kFailed);
  // Status::kAvailable can only transition to kStatus::Failed.
  DCHECK(status_ != Status::kAvailable || new_status == Status::kFailed);
  // The status can never transition from Status::kFailed.
  DCHECK(status_ != Status::kFailed || new_status == Status::kFailed);

  // If the status is not changing, simply return; the remaining logic handles
  // status changes.
  if (status_ == new_status) {
    return;
  }

  status_ = new_status;
  RecordStatusChange(status_);

  if (status_ == Status::kFailed) {
    database_->Destroy(base::BindOnce(&HintCacheStore::OnDatabaseDestroyed,
                                      weak_ptr_factory_.GetWeakPtr()));
    ClearComponentVersion();
    hint_entry_keys_.reset();
  }
}

bool HintCacheStore::IsAvailable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return status_ == Status::kAvailable;
}

void HintCacheStore::PurgeDatabase(base::OnceClosure callback) {
  // When purging the database, update the schema version to the current one.
  EntryKey schema_entry_key = GetMetadataTypeEntryKey(MetadataType::kSchema);
  previews::proto::StoreEntry schema_entry;
  schema_entry.set_version(kStoreSchemaVersion);

  auto entries_to_save = std::make_unique<EntryVector>();
  entries_to_save->emplace_back(schema_entry_key, schema_entry);

  database_->UpdateEntriesWithRemoveFilter(
      std::move(entries_to_save),
      base::BindRepeating(
          [](const std::string& schema_entry_key, const std::string& key) {
            return key.compare(0, schema_entry_key.length(),
                               schema_entry_key) != 0;
          },
          schema_entry_key),
      base::BindOnce(&HintCacheStore::OnPurgeDatabase,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HintCacheStore::SetComponentVersion(
    const base::Version& component_version) {
  DCHECK(component_version.IsValid());
  component_version_ = component_version;
  component_hint_entry_key_prefix_ =
      GetComponentHintEntryKeyPrefix(component_version_.value());
}

void HintCacheStore::ClearComponentVersion() {
  component_version_.reset();
  component_hint_entry_key_prefix_.clear();
}

void HintCacheStore::MaybeLoadHintEntryKeys(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the database is unavailable or if there's an in-flight component data
  // update, then don't load the hint keys. Simply run the callback.
  if (!IsAvailable() || component_data_update_in_flight_) {
    std::move(callback).Run();
    return;
  }

  // Create a new KeySet object. This is populated by the store's keys as the
  // filter is run with each key on the DB's background thread. The filter
  // itself always returns false, ensuring that no entries are ever actually
  // loaded by the DB. Ownership of the KeySet is passed into the
  // LoadKeysAndEntriesCallback callback, guaranteeing that the KeySet has a
  // lifespan longer than the filter calls.
  std::unique_ptr<EntryKeySet> hint_entry_keys(std::make_unique<EntryKeySet>());
  EntryKeySet* raw_hint_entry_keys_pointer = hint_entry_keys.get();
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(
          [](EntryKeySet* hint_entry_keys, const std::string& filter_prefix,
             const std::string& entry_key) {
            if (entry_key.compare(0, filter_prefix.length(), filter_prefix) !=
                0) {
              hint_entry_keys->insert(entry_key);
            }
            return false;
          },
          raw_hint_entry_keys_pointer, GetMetadataEntryKeyPrefix()),
      base::BindOnce(&HintCacheStore::OnLoadHintEntryKeys,
                     weak_ptr_factory_.GetWeakPtr(), std::move(hint_entry_keys),
                     std::move(callback)));
}

size_t HintCacheStore::GetHintEntryKeyCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return hint_entry_keys_ ? hint_entry_keys_->size() : 0;
}

void HintCacheStore::OnDatabaseInitialized(bool purge_existing_data,
                                           base::OnceClosure callback,
                                           bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }

  // If initialization is set to purge all existing data, then simply trigger
  // the purge and return. There's no need to load metadata entries that'll
  // immediately be purged.
  if (purge_existing_data) {
    PurgeDatabase(std::move(callback));
    return;
  }

  // Load all entries from the DB with the metadata key prefix.
  database_->LoadKeysAndEntriesWithFilter(
      leveldb_proto::KeyFilter(), leveldb::ReadOptions(),
      GetMetadataEntryKeyPrefix(),
      base::BindOnce(&HintCacheStore::OnLoadMetadata,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HintCacheStore::OnDatabaseDestroyed(bool /*success*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HintCacheStore::OnLoadMetadata(
    base::OnceClosure callback,
    bool success,
    std::unique_ptr<EntryMap> metadata_entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_entries);

  // Create a scoped load metadata result recorder. It records the result when
  // its destructor is called.
  ScopedLoadMetadataResultRecorder result_recorder;

  if (!success) {
    result_recorder.set_result(
        PreviewsHintCacheLevelDBStoreLoadMetadataResult::kLoadMetadataFailed);

    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }

  // If the schema version in the DB is not the current version, then purge the
  // database.
  auto schema_entry =
      metadata_entries->find(GetMetadataTypeEntryKey(MetadataType::kSchema));
  if (schema_entry == metadata_entries->end() ||
      !schema_entry->second.has_version() ||
      schema_entry->second.version() != kStoreSchemaVersion) {
    if (schema_entry == metadata_entries->end()) {
      result_recorder.set_result(
          PreviewsHintCacheLevelDBStoreLoadMetadataResult::
              kSchemaMetadataMissing);
    } else {
      result_recorder.set_result(
          PreviewsHintCacheLevelDBStoreLoadMetadataResult::
              kSchemaMetadataWrongVersion);
    }

    PurgeDatabase(std::move(callback));
    return;
  }

  // If the component metadata entry exists, then use it to set the component
  // version.
  bool component_metadata_missing = false;
  auto component_entry =
      metadata_entries->find(GetMetadataTypeEntryKey(MetadataType::kComponent));
  if (component_entry != metadata_entries->end()) {
    DCHECK(component_entry->second.has_version());
    SetComponentVersion(base::Version(component_entry->second.version()));
  } else {
    result_recorder.set_result(PreviewsHintCacheLevelDBStoreLoadMetadataResult::
                                   kComponentMetadataMissing);
    component_metadata_missing = true;
  }

  auto fetched_entry =
      metadata_entries->find(GetMetadataTypeEntryKey(MetadataType::kFetched));
  if (fetched_entry != metadata_entries->end()) {
    DCHECK(fetched_entry->second.has_update_time_secs());
    fetched_update_time_ = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(fetched_entry->second.update_time_secs()));
  } else {
    if (component_metadata_missing) {
      result_recorder.set_result(
          PreviewsHintCacheLevelDBStoreLoadMetadataResult::
              kComponentAndFetchedMetadataMissing);
    } else {
      result_recorder.set_result(
          PreviewsHintCacheLevelDBStoreLoadMetadataResult::
              kFetchedMetadataMissing);
    }
    fetched_update_time_ = base::Time();
  }

  UpdateStatus(Status::kAvailable);
  MaybeLoadHintEntryKeys(std::move(callback));
}

void HintCacheStore::OnPurgeDatabase(base::OnceClosure callback, bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The database can only be purged during initialization.
  DCHECK_EQ(status_, Status::kInitializing);

  UpdateStatus(success ? Status::kAvailable : Status::kFailed);
  std::move(callback).Run();
}

void HintCacheStore::OnUpdateComponentData(base::OnceClosure callback,
                                           bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(component_data_update_in_flight_);

  component_data_update_in_flight_ = false;
  if (!success) {
    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }
  MaybeLoadHintEntryKeys(std::move(callback));
}

void HintCacheStore::OnLoadHintEntryKeys(
    std::unique_ptr<EntryKeySet> hint_entry_keys,
    base::OnceClosure callback,
    bool success,
    std::unique_ptr<EntryMap> /*unused*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!hint_entry_keys_);

  if (!success) {
    UpdateStatus(Status::kFailed);
    std::move(callback).Run();
    return;
  }

  // If the store was set to unavailable after the request was started, or if
  // there's an in-flight component data update, which means the keys are about
  // to be invalidated, then the loaded keys should not be considered valid.
  // Reset the keys so that they are cleared.
  if (!IsAvailable() || component_data_update_in_flight_) {
    hint_entry_keys.reset();
  }

  hint_entry_keys_ = std::move(hint_entry_keys);
  std::move(callback).Run();
}

void HintCacheStore::OnLoadHint(
    const std::string& entry_key,
    HintLoadedCallback callback,
    bool success,
    std::unique_ptr<previews::proto::StoreEntry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If either the request failed, the store was set to unavailable after the
  // request was started, or there's an in-flight component data update, which
  // means the entry is about to be invalidated, then the loaded hint should not
  // be considered valid. Reset the entry so that no hint is returned to the
  // requester.
  if (!success || !IsAvailable() || component_data_update_in_flight_) {
    entry.reset();
  }

  // If the hint exists, release it into the Hint unique_ptr contained in the
  // callback. This eliminates the need for any copies of the entry's hint.
  std::unique_ptr<optimization_guide::proto::Hint> loaded_hint(
      entry && entry->has_hint() ? entry->release_hint() : nullptr);
  std::move(callback).Run(entry_key, std::move(loaded_hint));
}

}  // namespace previews
