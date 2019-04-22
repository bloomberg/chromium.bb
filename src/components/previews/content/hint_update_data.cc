// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/hint_update_data.h"

#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/hint_cache_store.h"
#include "components/previews/content/proto/hint_cache.pb.h"

namespace previews {

// static
std::unique_ptr<HintUpdateData> HintUpdateData::CreateComponentHintUpdateData(
    const base::Version& component_version) {
  std::unique_ptr<HintUpdateData> update_data(
      new HintUpdateData(base::Optional<base::Version>(component_version),
                         base::Optional<base::Time>()));
  return update_data;
}

// static
std::unique_ptr<HintUpdateData> HintUpdateData::CreateFetchedHintUpdateData(
    base::Time fetch_update_time) {
  std::unique_ptr<HintUpdateData> update_data(
      new HintUpdateData(base::Optional<base::Version>(),
                         base::Optional<base::Time>(fetch_update_time)));
  return update_data;
}

HintUpdateData::HintUpdateData(base::Optional<base::Version> component_version,
                               base::Optional<base::Time> fetch_update_time)
    : component_version_(component_version),
      fetch_update_time_(fetch_update_time),
      entries_to_save_(std::make_unique<EntryVector>()) {
  DCHECK_NE(!component_version_, !fetch_update_time_);

  if (component_version_.has_value()) {
    hint_entry_key_prefix_ =
        HintCacheStore::GetComponentHintEntryKeyPrefix(*component_version_);

    // Add a component metadata entry for the component's version.
    previews::proto::StoreEntry metadata_component_entry;
    metadata_component_entry.set_version(component_version_->GetString());
    entries_to_save_->emplace_back(
        HintCacheStore::GetMetadataTypeEntryKey(
            HintCacheStore::MetadataType::kComponent),
        std::move(metadata_component_entry));
  } else if (fetch_update_time_.has_value()) {
    hint_entry_key_prefix_ =
        // TODO(dougarnett): Merge in new call once landed:
        // HintCacheStore::GetFetchedHintEntryKeyPrefix();
        "3_";

    // TODO(dougarnett): add metadata entry for Fetch update?
  } else {
    NOTREACHED();
  }
}

HintUpdateData::~HintUpdateData() {}

void HintUpdateData::MoveHintIntoUpdateData(
    optimization_guide::proto::Hint&& hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!hint_entry_key_prefix_.empty());

  // To avoid any unnecessary copying, the hint is moved into proto::StoreEntry.
  HintCacheStore::EntryKey hint_entry_key = hint_entry_key_prefix_ + hint.key();
  previews::proto::StoreEntry entry_proto;
  entry_proto.set_allocated_hint(
      new optimization_guide::proto::Hint(std::move(hint)));
  entries_to_save_->emplace_back(std::move(hint_entry_key),
                                 std::move(entry_proto));
}

std::unique_ptr<EntryVector> HintUpdateData::TakeUpdateEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(entries_to_save_);

  return std::move(entries_to_save_);
}

}  // namespace previews
