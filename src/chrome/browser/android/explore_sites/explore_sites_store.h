// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_STORE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_STORE_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"

namespace sql {
class Database;
}

namespace explore_sites {

enum class InitializationStatus {
  NOT_INITIALIZED,
  INITIALIZING,
  SUCCESS,
  FAILURE,
};

// ExploreSitesStore is a front end to SQLite store hosting the explore sites
// web catalog.
//
// The store controls the pointer to the SQLite database and only makes it
// available to the |RunCallback| of the |Execute| method on the blocking
// thread.
class ExploreSitesStore {
 public:
  // Definition of the callback that is going to run the core of the command in
  // the |Execute| method.
  template <typename T>
  using RunCallback = base::OnceCallback<T(sql::Database*)>;

  // Definition of the callback used to pass the result back to the caller of
  // |Execute| method.
  template <typename T>
  using ResultCallback = base::OnceCallback<void(T)>;

  // Defines inactivity time of DB after which it is going to be closed.
  // TODO(dewittj): Derive appropriate value in a scientific way.
  static constexpr base::TimeDelta kClosingDelay =
      base::TimeDelta::FromSeconds(20);

  // Creates an instance of |ExploreSitesStore| with an in-memory SQLite
  // database.
  explicit ExploreSitesStore(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  // Creates an instance of |ExploreSitesStore| with a SQLite database stored in
  // |database_dir|.
  ExploreSitesStore(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const base::FilePath& database_dir);

  ~ExploreSitesStore();

  // Executes a |run_callback| on SQL store on the blocking thread, and posts
  // its result back to calling thread through |result_callback|.
  // Calling |Execute| when store is NOT_INITIALIZED will cause the store
  // initialization to start.
  // Store initialization status needs to be SUCCESS for run_callback to run.
  // If initialization fails, |result_callback| is invoked with |default_value|.
  template <typename T>
  void Execute(RunCallback<T> run_callback,
               ResultCallback<T> result_callback,
               T default_value) {
    CHECK_NE(initialization_status_, InitializationStatus::INITIALIZING);

    if (initialization_status_ == InitializationStatus::NOT_INITIALIZED) {
      Initialize(base::BindOnce(
          &ExploreSitesStore::Execute<T>, weak_ptr_factory_.GetWeakPtr(),
          std::move(run_callback), std::move(result_callback),
          std::move(default_value)));
      return;
    }

    TRACE_EVENT_ASYNC_BEGIN1(
        "explore_sites", "ExploreSites Store: task execution", this,
        "is store loaded",
        initialization_status_ == InitializationStatus::SUCCESS);
    // Ensure that any scheduled close operations are canceled.
    closing_weak_ptr_factory_.InvalidateWeakPtrs();

    sql::Database* db = initialization_status_ == InitializationStatus::SUCCESS
                            ? db_.get()
                            : nullptr;
    if (!db) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(result_callback), std::move(default_value)));
    } else {
      base::PostTaskAndReplyWithResult(
          blocking_task_runner_.get(), FROM_HERE,
          base::BindOnce(std::move(run_callback), db),
          base::BindOnce(&ExploreSitesStore::RescheduleClosing<T>,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(result_callback)));
    }
  }

  // Gets the initialization status of the store.
  InitializationStatus initialization_status() const {
    return initialization_status_;
  }

  void SetInitializationStatusForTest(InitializationStatus status);

 private:
  using DatabaseUniquePtr =
      std::unique_ptr<sql::Database, base::OnTaskRunnerDeleter>;

  // Used internally to initialize connection.
  void Initialize(base::OnceClosure pending_command);

  // Used to conclude opening/resetting DB connection.
  void OnInitializeDone(base::OnceClosure pending_command, bool success);

  // Reschedules the closing with a delay. Ensures that |result_callback| is
  // called.
  template <typename T>
  void RescheduleClosing(ResultCallback<T> result_callback, T result) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ExploreSitesStore::CloseInternal,
                       closing_weak_ptr_factory_.GetWeakPtr()),
        kClosingDelay);

    // Note: the time recorded for this trace step will include thread hop wait
    // times to the background thread and back.
    TRACE_EVENT_ASYNC_STEP_PAST0(
        "explore_sites", "ExploreSites Store: task execution", this, "Task");
    std::move(result_callback).Run(std::move(result));
    TRACE_EVENT_ASYNC_STEP_PAST0("explore_sites",
                                 "ExploreSites Store: task execution", this,
                                 "Callback");
    TRACE_EVENT_ASYNC_END0("explore_sites",
                           "ExploreSites Store: task execution", this);
  }

  // Internal function initiating the closing.
  void CloseInternal();

  // Completes the closing. Main purpose is to destroy the db pointer.
  void CloseInternalDone(DatabaseUniquePtr db);

  // Background thread where all SQL access should be run.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Path to the database on disk.
  base::FilePath db_file_path_;

  // Only open the store in memory. Used for testing.
  bool in_memory_;

  // Database connection.
  std::unique_ptr<sql::Database, base::OnTaskRunnerDeleter> db_;

  // Initialization status of the store.
  InitializationStatus initialization_status_;

  // Time of the last time the store was closed. Kept for metrics reporting.
  base::Time last_closing_time_;

  // Weak pointer to control the callback.
  base::WeakPtrFactory<ExploreSitesStore> weak_ptr_factory_;
  // Weak pointer to cancel closing of the store.
  base::WeakPtrFactory<ExploreSitesStore> closing_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesStore);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_STORE_H_
