// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/on_disk_directory_backing_store.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/metrics/histogram.h"
#include "sync/syncable/syncable-inl.h"

namespace syncer {
namespace syncable {

namespace {

enum HistogramResultEnum {
  FIRST_TRY_SUCCESS,
  SECOND_TRY_SUCCESS,
  SECOND_TRY_FAILURE,
  RESULT_COUNT
};

}  // namespace

OnDiskDirectoryBackingStore::OnDiskDirectoryBackingStore(
    const std::string& dir_name, const FilePath& backing_filepath)
    : DirectoryBackingStore(dir_name),
      allow_failure_for_test_(false),
      backing_filepath_(backing_filepath) {
  db_->set_exclusive_locking();
  db_->set_page_size(4096);
}

OnDiskDirectoryBackingStore::~OnDiskDirectoryBackingStore() { }

DirOpenResult OnDiskDirectoryBackingStore::TryLoad(
    MetahandlesIndex* entry_bucket,
    JournalIndex* delete_journals,
    Directory::KernelLoadInfo* kernel_load_info) {
  DCHECK(CalledOnValidThread());
  if (!db_->is_open()) {
    if (!db_->Open(backing_filepath_))
      return FAILED_OPEN_DATABASE;
  }

  if (!InitializeTables())
    return FAILED_OPEN_DATABASE;

  if (!DropDeletedEntries())
    return FAILED_DATABASE_CORRUPT;
  if (!LoadEntries(entry_bucket))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadDeleteJournals(delete_journals))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadInfo(kernel_load_info))
    return FAILED_DATABASE_CORRUPT;
  if (!VerifyReferenceIntegrity(*entry_bucket))
    return FAILED_DATABASE_CORRUPT;

  return OPENED;

}

DirOpenResult OnDiskDirectoryBackingStore::Load(
    MetahandlesIndex* entry_bucket,
    JournalIndex* delete_journals,
    Directory::KernelLoadInfo* kernel_load_info) {
  DirOpenResult result = TryLoad(entry_bucket, delete_journals,
                                 kernel_load_info);
  if (result == OPENED) {
    UMA_HISTOGRAM_ENUMERATION(
        "Sync.DirectoryOpenResult", FIRST_TRY_SUCCESS, RESULT_COUNT);
    return OPENED;
  }

  ReportFirstTryOpenFailure();

  // The fallback: delete the current database and return a fresh one.  We can
  // fetch the user's data from the cloud.
  STLDeleteElements(entry_bucket);
  STLDeleteElements(delete_journals);
  db_.reset(new sql::Connection);
  file_util::Delete(backing_filepath_, false);

  result = TryLoad(entry_bucket, delete_journals, kernel_load_info);
  if (result == OPENED) {
    UMA_HISTOGRAM_ENUMERATION(
        "Sync.DirectoryOpenResult", SECOND_TRY_SUCCESS, RESULT_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Sync.DirectoryOpenResult", SECOND_TRY_FAILURE, RESULT_COUNT);
  }

  return result;
}

void OnDiskDirectoryBackingStore::ReportFirstTryOpenFailure() {
  // In debug builds, the last thing we want is to silently clear the database.
  // It's full of evidence that might help us determine what went wrong.  It
  // might be sqlite's fault, but it could also be a bug in sync.  We crash
  // immediately so a developer can investigate.
  //
  // Developers: If you're not interested in debugging this right now, just move
  // aside the 'Sync Data' directory in your profile.  This is similar to what
  // the code would do if this DCHECK were disabled.
  NOTREACHED() << "Crashing to preserve corrupt sync database";
}

}  // namespace syncable
}  // namespace syncer
