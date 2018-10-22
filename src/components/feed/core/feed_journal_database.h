// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_JOURNAL_DATABASE_H_
#define COMPONENTS_FEED_CORE_FEED_JOURNAL_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/proto_database.h"

namespace feed {

class JournalStorageProto;

// FeedJournalDatabase is leveldb backend store for Feed's journal storage data.
// Feed's journal data are key-value pairs.
class FeedJournalDatabase {
 public:
  enum State {
    UNINITIALIZED,
    INITIALIZED,
    INIT_FAILURE,
  };

  // Returns the journal data as a vector of strings when calling loading data
  // or keys.
  using JournalLoadCallback =
      base::OnceCallback<void(std::vector<std::string>)>;

  // Initializes the database with |database_folder|.
  explicit FeedJournalDatabase(const base::FilePath& database_folder);

  // Initializes the database with |database_folder|. Creates storage using the
  // given |storage_database| for local storage. Useful for testing.
  FeedJournalDatabase(
      const base::FilePath& database_folder,
      std::unique_ptr<leveldb_proto::ProtoDatabase<JournalStorageProto>>
          storage_database);

  ~FeedJournalDatabase();

  // Returns true if initialization has finished successfully, else false.
  // While this is false, initialization may already started, or initialization
  // failed.
  bool IsInitialized() const;

  // Loads the journal data for the |key| and passes it to |callback|.
  void LoadJournal(const std::string& key, JournalLoadCallback callback);

 private:
  void OnGetEntryForLoadJournal(JournalLoadCallback callback,
                                bool success,
                                std::unique_ptr<JournalStorageProto> journal);

  // Callback methods given to |storage_database_| for async responses.
  void OnDatabaseInitialized(bool success);

  // Status of the database initialization.
  State database_status_;

  // The database for storing journal storage information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<JournalStorageProto>>
      storage_database_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FeedJournalDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedJournalDatabase);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_JOURNAL_DATABASE_H_
