// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_database_impl.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/proto/signal.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
namespace {

// TODO(shaktisahu): May be make this a class member for ease of testing.
bool FilterKeyBasedOnRange(proto::SignalType signal_type,
                           uint64_t name_hash,
                           base::Time end_time,
                           base::Time start_time,
                           const std::string& signal_key) {
  DCHECK(start_time <= end_time);
  SignalKey key;
  SignalKey::FromBinary(signal_key, &key);
  DCHECK(key.IsValid());
  if (key.kind() != metadata_utils::SignalTypeToSignalKind(signal_type) ||
      key.name_hash() != name_hash) {
    return false;
  }

  // Check if the key range is contained within the given range.
  return key.range_end() <= end_time && start_time <= key.range_start();
}

}  // namespace

SignalDatabaseImpl::SignalDatabaseImpl(std::unique_ptr<SignalProtoDb> database)
    : database_(std::move(database)) {}

SignalDatabaseImpl::~SignalDatabaseImpl() = default;

void SignalDatabaseImpl::Initialize(SuccessCallback callback) {
  database_->Init(
      leveldb_proto::CreateSimpleOptions(),
      base::BindOnce(&SignalDatabaseImpl::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignalDatabaseImpl::WriteSample(proto::SignalType signal_type,
                                     uint64_t name_hash,
                                     absl::optional<int32_t> value,
                                     base::Time timestamp,
                                     SuccessCallback callback) {
  DCHECK(initialized_);
  SignalKey key(metadata_utils::SignalTypeToSignalKind(signal_type), name_hash,
                timestamp, timestamp);

  proto::SignalData signal_data;
  proto::Sample* sample = signal_data.add_samples();
  if (value.has_value())
    sample->set_value(value.value());

  // Convert to delta from UTC midnight. This results in smaller values thereby
  // requiring less storage space in the DB.
  base::TimeDelta midnight_delta = timestamp - timestamp.UTCMidnight();
  sample->set_time_sec_delta(midnight_delta.InSeconds());

  // Write as a new db entry.
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SignalData>>>();
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();
  entries_to_save->emplace_back(
      std::make_pair(key.ToBinary(), std::move(signal_data)));
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_delete), std::move(callback));
}

void SignalDatabaseImpl::GetSamples(proto::SignalType signal_type,
                                    uint64_t name_hash,
                                    base::Time start_time,
                                    base::Time end_time,
                                    SamplesCallback callback) {
  DCHECK(initialized_);
  SignalKey dummy_key(metadata_utils::SignalTypeToSignalKind(signal_type),
                      name_hash, base::Time(), base::Time());
  std::string key_prefix = dummy_key.GetPrefixInBinary();
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&FilterKeyBasedOnRange, signal_type, name_hash,
                          end_time, start_time),
      leveldb::ReadOptions(), key_prefix,
      base::BindOnce(&SignalDatabaseImpl::OnGetSamples,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     start_time, end_time));
}

void SignalDatabaseImpl::OnGetSamples(
    SamplesCallback callback,
    base::Time start_time,
    base::Time end_time,
    bool success,
    std::unique_ptr<std::map<std::string, proto::SignalData>> entries) {
  std::vector<Sample> out;
  if (!success || !entries) {
    std::move(callback).Run(out);
    return;
  }

  for (const auto& pair : *entries.get()) {
    SignalKey key;
    SignalKey::FromBinary(pair.first, &key);
    DCHECK(key.IsValid());
    // TODO(shaktisahu): Remove DCHECK and collect UMA.
    const auto& signal_data = pair.second;
    base::Time midnight = key.range_start().UTCMidnight();
    for (int i = 0; i < signal_data.samples_size(); ++i) {
      const auto& sample = signal_data.samples(i);
      base::Time timestamp =
          midnight + base::TimeDelta::FromSeconds(sample.time_sec_delta());
      if (timestamp < start_time || timestamp > end_time)
        continue;

      out.emplace_back(std::make_pair(
          timestamp, sample.has_value() ? absl::make_optional(sample.value())
                                        : absl::nullopt));
    }
  }

  std::move(callback).Run(out);
}

void SignalDatabaseImpl::DeleteSamples(proto::SignalType signal_type,
                                       uint64_t name_hash,
                                       base::Time end_time,
                                       SuccessCallback callback) {
  DCHECK(initialized_);
  SignalKey dummy_key(metadata_utils::SignalTypeToSignalKind(signal_type),
                      name_hash, base::Time(), base::Time());
  std::string key_prefix = dummy_key.GetPrefixInBinary();
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&FilterKeyBasedOnRange, signal_type, name_hash,
                          end_time, base::Time()),
      leveldb::ReadOptions(), key_prefix,
      base::BindOnce(&SignalDatabaseImpl::OnGetSamplesForDeletion,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignalDatabaseImpl::OnGetSamplesForDeletion(
    SuccessCallback callback,
    bool success,
    std::unique_ptr<std::map<std::string, proto::SignalData>> entries) {
  if (!success || !entries) {
    std::move(callback).Run(success);
    return;
  }

  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SignalData>>>();
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();

  // Collect the keys to be deleted.
  for (const auto& pair : *entries.get()) {
    keys_to_delete->emplace_back(pair.first);
  }

  // Write to DB.
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_delete), std::move(callback));
}

void SignalDatabaseImpl::CompactSamplesForDay(proto::SignalType signal_type,
                                              uint64_t name_hash,
                                              base::Time day_start_time,
                                              SuccessCallback callback) {
  DCHECK(initialized_);
  // Compact the signals between 00:00:00AM to 23:59:59PM.
  day_start_time = day_start_time.UTCMidnight();
  base::Time day_end_time = day_start_time + base::TimeDelta::FromDays(1) -
                            base::TimeDelta::FromSeconds(1);
  SignalKey compact_key(metadata_utils::SignalTypeToSignalKind(signal_type),
                        name_hash, day_end_time, day_start_time);
  database_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&FilterKeyBasedOnRange, signal_type, name_hash,
                          day_end_time, day_start_time),
      base::BindOnce(&SignalDatabaseImpl::OnGetSamplesForCompaction,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     compact_key.ToBinary()));
}

void SignalDatabaseImpl::OnGetSamplesForCompaction(
    SuccessCallback callback,
    std::string compact_key,
    bool success,
    std::unique_ptr<std::map<std::string, proto::SignalData>> entries) {
  if (!success || !entries || entries->empty()) {
    std::move(callback).Run(success);
    return;
  }

  // We found one or more entries for the day. Let's compact them.
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();

  // Aggregate samples under a new proto. Delete the old entries.
  proto::SignalData compact;
  for (const auto& pair : *entries.get()) {
    const auto& signal_data = pair.second;
    for (int i = 0; i < signal_data.samples_size(); i++) {
      auto* new_sample = compact.add_samples();
      new_sample->CopyFrom(signal_data.samples(i));
    }

    keys_to_delete->emplace_back(pair.first);
  }

  // Write to DB.
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SignalData>>>();
  entries_to_save->emplace_back(
      std::make_pair(compact_key, std::move(compact)));
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_delete), std::move(callback));
}

void SignalDatabaseImpl::OnDatabaseInitialized(
    SuccessCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  initialized_ = status == leveldb_proto::Enums::InitStatus::kOK;
  std::move(callback).Run(status == leveldb_proto::Enums::InitStatus::kOK);
}

}  // namespace segmentation_platform
