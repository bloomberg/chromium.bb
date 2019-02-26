// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_store.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/explore_sites/explore_sites_schema.h"
#include "sql/database.h"

namespace explore_sites {
namespace {

const char kExploreSitesStoreFileName[] = "ExploreSitesStore.db";

bool PrepareDirectory(const base::FilePath& path) {
  base::File::Error error = base::File::FILE_OK;
  if (!base::DirectoryExists(path.DirName())) {
    if (!base::CreateDirectoryAndGetError(path.DirName(), &error)) {
      LOG(ERROR) << "Failed to create explore sites db directory: "
                 << base::File::ErrorToString(error);
      return false;
    }
  }
  return true;
}

bool InitializeSync(sql::Database* db,
                    const base::FilePath& path,
                    bool in_memory) {
  // These values are default.
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag("ExploreSitesStore");
  db->set_exclusive_locking();

  if (!in_memory && !PrepareDirectory(path))
    return false;

  bool open_db_result = in_memory ? db->OpenInMemory() : db->Open(path);

  if (!open_db_result) {
    LOG(ERROR) << "Failed to open database, in memory: " << in_memory;
    return false;
  }
  db->Preload();

  return ExploreSitesSchema::CreateOrUpgradeIfNeeded(db);
}

void CloseDatabaseSync(
    sql::Database* db,
    scoped_refptr<base::SingleThreadTaskRunner> callback_runner,
    base::OnceClosure callback) {
  if (db)
    db->Close();
  callback_runner->PostTask(FROM_HERE, std::move(callback));
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ExploreSitesStoreEvent {
  kReopened = 0,
  kOpenedFirstTime = 1,
  kCloseSkipped = 2,
  kClosed = 3,
  kMaxValue = kClosed,
};

void ReportStoreEvent(ExploreSitesStoreEvent event) {
  UMA_HISTOGRAM_ENUMERATION("ExploreSites.ExploreSitesStore.StoreEvent", event);
}

}  // namespace

// static
constexpr base::TimeDelta ExploreSitesStore::kClosingDelay;

ExploreSitesStore::ExploreSitesStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : blocking_task_runner_(std::move(blocking_task_runner)),
      in_memory_(true),
      db_(nullptr, base::OnTaskRunnerDeleter(blocking_task_runner_)),
      initialization_status_(InitializationStatus::NOT_INITIALIZED),
      weak_ptr_factory_(this),
      closing_weak_ptr_factory_(this) {}

ExploreSitesStore::ExploreSitesStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& path)
    : blocking_task_runner_(std::move(blocking_task_runner)),
      db_file_path_(path.AppendASCII(kExploreSitesStoreFileName)),
      in_memory_(false),
      db_(nullptr, base::OnTaskRunnerDeleter(blocking_task_runner_)),
      initialization_status_(InitializationStatus::NOT_INITIALIZED),
      weak_ptr_factory_(this),
      closing_weak_ptr_factory_(this) {}

ExploreSitesStore::~ExploreSitesStore() {}

void ExploreSitesStore::SetInitializationStatusForTest(
    InitializationStatus status) {
  initialization_status_ = status;
}

void ExploreSitesStore::Initialize(base::OnceClosure pending_command) {
  TRACE_EVENT_ASYNC_BEGIN1("explore_sites", "ExploreSitesStore", this,
                           "is reopen", !last_closing_time_.is_null());
  DCHECK_EQ(initialization_status_, InitializationStatus::NOT_INITIALIZED);
  initialization_status_ = InitializationStatus::INITIALIZING;

  if (!last_closing_time_.is_null()) {
    ReportStoreEvent(ExploreSitesStoreEvent::kReopened);
  } else {
    ReportStoreEvent(ExploreSitesStoreEvent::kOpenedFirstTime);
  }

  // This is how we reset a pointer and provide deleter. This is necessary to
  // ensure that we can close the store more than once.
  db_ = DatabaseUniquePtr(new sql::Database,
                          base::OnTaskRunnerDeleter(blocking_task_runner_));

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&InitializeSync, db_.get(), db_file_path_, in_memory_),
      base::BindOnce(&ExploreSitesStore::OnInitializeDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(pending_command)));
}

void ExploreSitesStore::OnInitializeDone(base::OnceClosure pending_command,
                                         bool success) {
  // TODO(carlosk): Add initializing error reporting here.
  TRACE_EVENT_ASYNC_STEP_PAST1("explore_sites", "ExploreSitesStore", this,
                               "Initializing", "succeeded", success);
  DCHECK_EQ(initialization_status_, InitializationStatus::INITIALIZING);
  if (success) {
    initialization_status_ = InitializationStatus::SUCCESS;
  } else {
    initialization_status_ = InitializationStatus::FAILURE;
    db_.reset();
    TRACE_EVENT_ASYNC_END0("explore_sites", "ExploreSitesStore", this);
  }

  CHECK(!pending_command.is_null());
  std::move(pending_command).Run();

  // Once pending commands are empty, we get back to NOT_INITIALIZED state, to
  // make it possible to retry initialization next time a DB operation is
  // attempted.
  if (initialization_status_ == InitializationStatus::FAILURE)
    initialization_status_ = InitializationStatus::NOT_INITIALIZED;
}

void ExploreSitesStore::CloseInternal() {
  if (initialization_status_ != InitializationStatus::SUCCESS) {
    ReportStoreEvent(ExploreSitesStoreEvent::kCloseSkipped);
    return;
  }
  TRACE_EVENT_ASYNC_STEP_PAST0("explore_sites", "ExploreSitesStore", this,
                               "Open");

  last_closing_time_ = base::Time::Now();
  ReportStoreEvent(ExploreSitesStoreEvent::kClosed);

  initialization_status_ = InitializationStatus::NOT_INITIALIZED;
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CloseDatabaseSync, db_.get(), base::ThreadTaskRunnerHandle::Get(),
          base::BindOnce(&ExploreSitesStore::CloseInternalDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(db_))));
}

void ExploreSitesStore::CloseInternalDone(DatabaseUniquePtr db) {
  db.reset();
  TRACE_EVENT_ASYNC_STEP_PAST0("explore_sites", "ExploreSitesStore", this,
                               "Closing");
  TRACE_EVENT_ASYNC_END0("explore_sites", "ExploreSitesStore", this);
}

}  // namespace explore_sites
