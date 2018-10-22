// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_database.h"

#include <utility>

#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "components/feed/core/proto/journal_storage.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace feed {

namespace {
using StorageEntryVector =
    leveldb_proto::ProtoDatabase<JournalStorageProto>::KeyEntryVector;

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.FeedJournalDatabase. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kJournalDatabaseUMAClientName[] = "FeedJournalDatabase";

const char kJournalDatabaseFolder[] = "journal";

const size_t kDatabaseWriteBufferSizeBytes = 512 * 1024;
const size_t kDatabaseWriteBufferSizeBytesForLowEndDevice = 128 * 1024;

}  // namespace

FeedJournalDatabase::FeedJournalDatabase(const base::FilePath& database_folder)
    : FeedJournalDatabase(
          database_folder,
          std::make_unique<
              leveldb_proto::ProtoDatabaseImpl<JournalStorageProto>>(
              base::CreateSequencedTaskRunnerWithTraits(
                  {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                   base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}))) {}

FeedJournalDatabase::FeedJournalDatabase(
    const base::FilePath& database_folder,
    std::unique_ptr<leveldb_proto::ProtoDatabase<JournalStorageProto>>
        storage_database)
    : database_status_(UNINITIALIZED),
      storage_database_(std::move(storage_database)),
      weak_ptr_factory_(this) {
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  if (base::SysInfo::IsLowEndDevice()) {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytesForLowEndDevice;
  } else {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  }

  base::FilePath storage_folder =
      database_folder.AppendASCII(kJournalDatabaseFolder);
  storage_database_->Init(
      kJournalDatabaseUMAClientName, storage_folder, options,
      base::BindOnce(&FeedJournalDatabase::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

FeedJournalDatabase::~FeedJournalDatabase() = default;

bool FeedJournalDatabase::IsInitialized() const {
  return INITIALIZED == database_status_;
}

void FeedJournalDatabase::LoadJournal(const std::string& key,
                                      JournalLoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->GetEntry(
      key, base::BindOnce(&FeedJournalDatabase::OnGetEntryForLoadJournal,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedJournalDatabase::OnGetEntryForLoadJournal(
    JournalLoadCallback callback,
    bool success,
    std::unique_ptr<JournalStorageProto> journal) {
  std::vector<std::string> results;

  if (!success || !journal) {
    DVLOG_IF(1, !success) << "FeedJournalDatabase load journal failed.";
    std::move(callback).Run(std::move(results));
    return;
  }

  for (int i = 0; i < journal->journal_data_size(); ++i) {
    results.emplace_back(journal->journal_data(i));
  }

  std::move(callback).Run(std::move(results));
}

void FeedJournalDatabase::OnDatabaseInitialized(bool success) {
  DCHECK_EQ(database_status_, UNINITIALIZED);

  if (success) {
    database_status_ = INITIALIZED;
  } else {
    database_status_ = INIT_FAILURE;
    DVLOG(1) << "FeedJournalDatabase init failed.";
  }
}

}  // namespace feed
