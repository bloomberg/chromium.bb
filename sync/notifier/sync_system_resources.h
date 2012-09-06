// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple system resources class that uses the current message loop
// for scheduling.  Assumes the current message loop is already
// running.

#ifndef SYNC_NOTIFIER_SYNC_SYSTEM_RESOURCES_H_
#define SYNC_NOTIFIER_SYNC_SYSTEM_RESOURCES_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "google/cacheinvalidation/include/system-resources.h"
#include "sync/notifier/push_client_channel.h"
#include "sync/notifier/state_writer.h"

namespace notifier {
class PushClient;
}  // namespace notifier

namespace syncer {

class SyncLogger : public invalidation::Logger {
 public:
  SyncLogger();

  virtual ~SyncLogger();

  // invalidation::Logger implementation.
  virtual void Log(LogLevel level, const char* file, int line,
                   const char* format, ...) OVERRIDE;

  virtual void SetSystemResources(
      invalidation::SystemResources* resources) OVERRIDE;
};

class SyncInvalidationScheduler : public invalidation::Scheduler {
 public:
  SyncInvalidationScheduler();

  virtual ~SyncInvalidationScheduler();

  // Start and stop the scheduler.
  void Start();
  void Stop();

  // invalidation::Scheduler implementation.
  virtual void Schedule(invalidation::TimeDelta delay,
                        invalidation::Closure* task) OVERRIDE;

  virtual bool IsRunningOnThread() const OVERRIDE;

  virtual invalidation::Time GetCurrentTime() const OVERRIDE;

  virtual void SetSystemResources(
      invalidation::SystemResources* resources) OVERRIDE;

 private:
  base::WeakPtrFactory<SyncInvalidationScheduler> weak_factory_;
  // Holds all posted tasks that have not yet been run.
  std::set<invalidation::Closure*> posted_tasks_;

  const MessageLoop* created_on_loop_;
  bool is_started_;
  bool is_stopped_;

  // Runs the task, deletes it, and removes it from |posted_tasks_|.
  void RunPostedTask(invalidation::Closure* task);
};

class SyncStorage : public invalidation::Storage {
 public:
  SyncStorage(StateWriter* state_writer, invalidation::Scheduler* scheduler);

  virtual ~SyncStorage();

  void SetInitialState(const std::string& value) {
    cached_state_ = value;
  }

  // invalidation::Storage implementation.
  virtual void WriteKey(const std::string& key, const std::string& value,
                        invalidation::WriteKeyCallback* done) OVERRIDE;

  virtual void ReadKey(const std::string& key,
                       invalidation::ReadKeyCallback* done) OVERRIDE;

  virtual void DeleteKey(const std::string& key,
                         invalidation::DeleteKeyCallback* done) OVERRIDE;

  virtual void ReadAllKeys(
      invalidation::ReadAllKeysCallback* key_callback) OVERRIDE;

  virtual void SetSystemResources(
      invalidation::SystemResources* resources) OVERRIDE;

 private:
  // Runs the given storage callback with SUCCESS status and deletes it.
  void RunAndDeleteWriteKeyCallback(
      invalidation::WriteKeyCallback* callback);

  // Runs the given callback with the given value and deletes it.
  void RunAndDeleteReadKeyCallback(
      invalidation::ReadKeyCallback* callback, const std::string& value);

  StateWriter* state_writer_;
  invalidation::Scheduler* scheduler_;
  std::string cached_state_;
};

class SyncSystemResources : public invalidation::SystemResources {
 public:
  SyncSystemResources(scoped_ptr<notifier::PushClient> push_client,
                      StateWriter* state_writer);

  virtual ~SyncSystemResources();

  // invalidation::SystemResources implementation.
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual void set_platform(const std::string& platform);
  virtual std::string platform() const OVERRIDE;
  virtual SyncLogger* logger() OVERRIDE;
  virtual SyncStorage* storage() OVERRIDE;
  virtual PushClientChannel* network() OVERRIDE;
  virtual SyncInvalidationScheduler* internal_scheduler() OVERRIDE;
  virtual SyncInvalidationScheduler* listener_scheduler() OVERRIDE;

 private:
  bool is_started_;
  std::string platform_;
  scoped_ptr<SyncLogger> logger_;
  scoped_ptr<SyncInvalidationScheduler> internal_scheduler_;
  scoped_ptr<SyncInvalidationScheduler> listener_scheduler_;
  scoped_ptr<SyncStorage> storage_;
  PushClientChannel push_client_channel_;
};

}  // namespace syncer

#endif  // SYNC_NOTIFIER_SYNC_SYSTEM_RESOURCES_H_
