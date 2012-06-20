// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/chrome_system_resources.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "google/cacheinvalidation/deps/callback.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/push_client.h"
#include "sync/notifier/invalidation_util.h"

namespace csync {

ChromeLogger::ChromeLogger() {}
ChromeLogger::~ChromeLogger() {}

void ChromeLogger::Log(LogLevel level, const char* file, int line,
                       const char* format, ...) {
  logging::LogSeverity log_severity = -2;  // VLOG(2)
  bool emit_log = false;
  switch (level) {
    case FINE_LEVEL:
      log_severity = -2;  // VLOG(2)
      emit_log = VLOG_IS_ON(2);
      break;
    case INFO_LEVEL:
      log_severity = -1;  // VLOG(1)
      emit_log = VLOG_IS_ON(1);
      break;
    case WARNING_LEVEL:
      log_severity = logging::LOG_WARNING;
      emit_log = LOG_IS_ON(WARNING);
      break;
    case SEVERE_LEVEL:
      log_severity = logging::LOG_ERROR;
      emit_log = LOG_IS_ON(ERROR);
      break;
  }
  if (emit_log) {
    va_list ap;
    va_start(ap, format);
    std::string result;
    base::StringAppendV(&result, format, ap);
    logging::LogMessage(file, line, log_severity).stream() << result;
    va_end(ap);
  }
}

void ChromeLogger::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

ChromeScheduler::ChromeScheduler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      created_on_loop_(MessageLoop::current()),
      is_started_(false),
      is_stopped_(false) {
  CHECK(created_on_loop_);
}

ChromeScheduler::~ChromeScheduler() {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  CHECK(is_stopped_);
}

void ChromeScheduler::Start() {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  CHECK(!is_started_);
  is_started_ = true;
  is_stopped_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

void ChromeScheduler::Stop() {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  is_stopped_ = true;
  is_started_ = false;
  weak_factory_.InvalidateWeakPtrs();
  STLDeleteElements(&posted_tasks_);
  posted_tasks_.clear();
}

void ChromeScheduler::Schedule(invalidation::TimeDelta delay,
                               invalidation::Closure* task) {
  DCHECK(invalidation::IsCallbackRepeatable(task));
  CHECK_EQ(created_on_loop_, MessageLoop::current());

  if (!is_started_) {
    delete task;
    return;
  }

  posted_tasks_.insert(task);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(&ChromeScheduler::RunPostedTask,
                            weak_factory_.GetWeakPtr(), task),
      delay);
}

bool ChromeScheduler::IsRunningOnThread() const {
  return created_on_loop_ == MessageLoop::current();
}

invalidation::Time ChromeScheduler::GetCurrentTime() const {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  return base::Time::Now();
}

void ChromeScheduler::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

void ChromeScheduler::RunPostedTask(invalidation::Closure* task) {
  CHECK_EQ(created_on_loop_, MessageLoop::current());
  task->Run();
  posted_tasks_.erase(task);
  delete task;
}

ChromeStorage::ChromeStorage(StateWriter* state_writer,
                             invalidation::Scheduler* scheduler)
    : state_writer_(state_writer),
      scheduler_(scheduler) {
  DCHECK(state_writer_);
  DCHECK(scheduler_);
}

ChromeStorage::~ChromeStorage() {}

void ChromeStorage::WriteKey(const std::string& key, const std::string& value,
                             invalidation::WriteKeyCallback* done) {
  CHECK(state_writer_);
  // TODO(ghc): actually write key,value associations, and don't invoke the
  // callback until the operation completes.
  state_writer_->WriteState(value);
  cached_state_ = value;
  // According to the cache invalidation API folks, we can do this as
  // long as we make sure to clear the persistent state that we start
  // up the cache invalidation client with.  However, we musn't do it
  // right away, as we may be called under a lock that the callback
  // uses.
  scheduler_->Schedule(
      invalidation::Scheduler::NoDelay(),
      invalidation::NewPermanentCallback(
          this, &ChromeStorage::RunAndDeleteWriteKeyCallback,
          done));
}

void ChromeStorage::ReadKey(const std::string& key,
                            invalidation::ReadKeyCallback* done) {
  DCHECK(scheduler_->IsRunningOnThread()) << "not running on scheduler thread";
  RunAndDeleteReadKeyCallback(done, cached_state_);
}

void ChromeStorage::DeleteKey(const std::string& key,
                              invalidation::DeleteKeyCallback* done) {
  // TODO(ghc): Implement.
  LOG(WARNING) << "ignoring call to DeleteKey(" << key << ", callback)";
}

void ChromeStorage::ReadAllKeys(invalidation::ReadAllKeysCallback* done) {
  // TODO(ghc): Implement.
  LOG(WARNING) << "ignoring call to ReadAllKeys(callback)";
}

void ChromeStorage::SetSystemResources(
    invalidation::SystemResources* resources) {
  // Do nothing.
}

void ChromeStorage::RunAndDeleteWriteKeyCallback(
    invalidation::WriteKeyCallback* callback) {
  callback->Run(invalidation::Status(invalidation::Status::SUCCESS, ""));
  delete callback;
}

void ChromeStorage::RunAndDeleteReadKeyCallback(
    invalidation::ReadKeyCallback* callback, const std::string& value) {
  callback->Run(std::make_pair(
      invalidation::Status(invalidation::Status::SUCCESS, ""),
      value));
  delete callback;
}

ChromeSystemResources::ChromeSystemResources(
    scoped_ptr<notifier::PushClient> push_client,
    StateWriter* state_writer)
    : is_started_(false),
      logger_(new ChromeLogger()),
      internal_scheduler_(new ChromeScheduler()),
      listener_scheduler_(new ChromeScheduler()),
      storage_(new ChromeStorage(state_writer, internal_scheduler_.get())),
      push_client_channel_(push_client.Pass()) {
}

ChromeSystemResources::~ChromeSystemResources() {
  Stop();
}

void ChromeSystemResources::Start() {
  internal_scheduler_->Start();
  listener_scheduler_->Start();
  is_started_ = true;
}

void ChromeSystemResources::Stop() {
  internal_scheduler_->Stop();
  listener_scheduler_->Stop();
}

bool ChromeSystemResources::IsStarted() const {
  return is_started_;
}

void ChromeSystemResources::set_platform(const std::string& platform) {
  platform_ = platform;
}

std::string ChromeSystemResources::platform() const {
  return platform_;
}

ChromeLogger* ChromeSystemResources::logger() {
  return logger_.get();
}

ChromeStorage* ChromeSystemResources::storage() {
  return storage_.get();
}

PushClientChannel* ChromeSystemResources::network() {
  return &push_client_channel_;
}

ChromeScheduler* ChromeSystemResources::internal_scheduler() {
  return internal_scheduler_.get();
}

ChromeScheduler* ChromeSystemResources::listener_scheduler() {
  return listener_scheduler_.get();
}

}  // namespace csync
