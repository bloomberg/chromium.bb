// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/shared_proto_database.h"

#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"

namespace leveldb_proto {

SharedProtoDatabase::SharedProtoDatabase(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const std::string& client_name,
    const base::FilePath& db_dir)
    : task_runner_(task_runner),
      db_dir_(db_dir),
      db_wrapper_(std::make_unique<ProtoLevelDBWrapper>(task_runner_)),
      db_(std::make_unique<LevelDB>(client_name.c_str())),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(on_task_runner_);
}

// All init functionality runs on the same SequencedTaskRunner, so any caller of
// this after a database Init will receive the correct status of the database.
// PostTaskAndReply is used to ensure that we call the Init callback on its
// original calling thread.
void SharedProtoDatabase::GetDatabaseInitStateAsync(
    ProtoLevelDBWrapper::InitCallback callback) {
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&SharedProtoDatabase::RunInitCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SharedProtoDatabase::RunInitCallback(
    ProtoLevelDBWrapper::InitCallback callback) {
  std::move(callback).Run(init_state_ == InitState::kSuccess);
}

// Setting |create_if_missing| to false allows us to test whether or not the
// shared database already exists, useful for migrating data from the shared
// database to a unique database if it exists.
// All clients planning to use the shared database should be setting
// |create_if_missing| to true. Setting this to false can result in unexpected
// behaviour since the ordering of Init calls may matter if some calls are made
// with this set to true, and others false.
void SharedProtoDatabase::Init(
    bool create_if_missing,
    ProtoLevelDBWrapper::InitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);

  // If we succeeded previously, just let the callback know. Otherwise, we'll
  // continue to try initialization for every new request.
  if (init_state_ == InitState::kSuccess) {
    callback_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true /* success */));
    return;
  }

  init_state_ = InitState::kInProgress;
  auto options = CreateSimpleOptions();
  options.create_if_missing = create_if_missing;
  // |db_wrapper_| uses the same SequencedTaskRunner that Init is called on,
  // so OnDatabaseInit will be called on the same sequence after Init.
  // This means any callers to Init using the same TaskRunner can guarantee that
  // the InitState will be final after Init is called.
  db_wrapper_->InitWithDatabase(
      db_.get(), db_dir_, options, false /* destroy_on_corruption */,
      base::BindOnce(&SharedProtoDatabase::OnDatabaseInit,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     callback_task_runner));
}

void SharedProtoDatabase::OnDatabaseInit(
    ProtoLevelDBWrapper::InitCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_task_runner_);
  init_state_ = success ? InitState::kSuccess : InitState::kFailure;

  // TODO(thildebr): Check the db_wrapper_->IsCorrupt() and store corruption
  // information to inform clients they may have lost data.

  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), success));
}

SharedProtoDatabase::~SharedProtoDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(on_creation_sequence_);
}

LevelDB* SharedProtoDatabase::GetLevelDBForTesting() const {
  return db_.get();
}

}  // namespace leveldb_proto