// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_HINT_UPDATE_DATA_H_
#define COMPONENTS_PREVIEWS_CONTENT_HINT_UPDATE_DATA_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace optimization_guide {
namespace proto {
class Hint;
}  // namespace proto
}  // namespace optimization_guide

namespace previews {
namespace proto {
class StoreEntry;
}  // namespace proto

using EntryVector =
    leveldb_proto::ProtoDatabase<previews::proto::StoreEntry>::KeyEntryVector;

// Holds hint data for updating the HintCacheStore.
class HintUpdateData {
 public:
  ~HintUpdateData();

  // Creates an update data object for a component hint update.
  static std::unique_ptr<HintUpdateData> CreateComponentHintUpdateData(
      const base::Version& component_version);

  // Creates an update data object for a fetched hint update.
  static std::unique_ptr<HintUpdateData> CreateFetchedHintUpdateData(
      base::Time fetch_update_time);

  // Returns the component version of a component hint update.
  const base::Optional<base::Version> component_version() const {
    return component_version_;
  }

  // Returns the next update time for a fetched hint update.
  const base::Optional<base::Time> fetch_update_time() const {
    return fetch_update_time_;
  }

  // Moves |hint| into this update data. After MoveHintIntoUpdateData() is
  // called, |hint| is no longer valid.
  void MoveHintIntoUpdateData(optimization_guide::proto::Hint&& hint);

  // Returns the store entry updates along with ownership to them.
  std::unique_ptr<EntryVector> TakeUpdateEntries();

 private:
  HintUpdateData(base::Optional<base::Version> component_version,
                 base::Optional<base::Time> fetch_update_time);

  // The component version of the update data for a component update.
  base::Optional<base::Version> component_version_;

  // The time when hints in this update need to be updated for a fetch update.
  base::Optional<base::Time> fetch_update_time_;

  // The prefix to add to the key of every hint entry. It is set
  // during construction for appropriate type of update.
  std::string hint_entry_key_prefix_;

  // The vector of entries to save.
  std::unique_ptr<EntryVector> entries_to_save_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HintUpdateData);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_HINT_UPDATE_DATA_H_
