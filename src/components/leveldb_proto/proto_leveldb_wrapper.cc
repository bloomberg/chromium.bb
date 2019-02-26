// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/proto_leveldb_wrapper.h"

#include "components/leveldb_proto/proto_leveldb_wrapper_metrics.h"

namespace leveldb_proto {

namespace {

inline void InitFromTaskRunner(LevelDB* database,
                               const base::FilePath& database_dir,
                               const leveldb_env::Options& options,
                               bool destroy_on_corruption,
                               leveldb::Status* status,
                               const std::string& client_id) {
  DCHECK(status);

  // TODO(cjhopman): Histogram for database size.
  *status = database->Init(database_dir, options, destroy_on_corruption);
  ProtoLevelDBWrapperMetrics::RecordInit(client_id, *status);
}

void RunDestroyCallback(typename ProtoLevelDBWrapper::DestroyCallback callback,
                        const bool* success) {
  std::move(callback).Run(*success);
}

inline void DestroyFromTaskRunner(LevelDB* database,
                                  bool* success,
                                  const std::string& client_id) {
  CHECK(success);

  auto status = database->Destroy();
  *success = status.ok();
  ProtoLevelDBWrapperMetrics::RecordDestroy(client_id, *success);
}

void RunLoadKeysCallback(
    typename ProtoLevelDBWrapper::LoadKeysCallback callback,
    std::unique_ptr<bool> success,
    std::unique_ptr<std::vector<std::string>> keys) {
  std::move(callback).Run(*success, std::move(keys));
}

inline void LoadKeysFromTaskRunner(LevelDB* database,
                                   const std::string& target_prefix,
                                   std::vector<std::string>* keys,
                                   bool* success,
                                   const std::string& client_id) {
  DCHECK(success);
  DCHECK(keys);
  keys->clear();
  *success = database->LoadKeys(target_prefix, keys);
  ProtoLevelDBWrapperMetrics::RecordLoadKeys(client_id, *success);
}

}  // namespace

ProtoLevelDBWrapper::ProtoLevelDBWrapper(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProtoLevelDBWrapper::ProtoLevelDBWrapper(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    LevelDB* db)
    : task_runner_(task_runner), db_(db), weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProtoLevelDBWrapper::~ProtoLevelDBWrapper() = default;

void ProtoLevelDBWrapper::RunInitCallback(
    typename ProtoLevelDBWrapper::InitCallback callback,
    const leveldb::Status* status) {
  is_corrupt_ = status->IsCorruption();
  std::move(callback).Run(status->ok());
}

void ProtoLevelDBWrapper::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    bool destroy_on_corruption,
    typename ProtoLevelDBWrapper::InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_);
  DCHECK(database);
  db_ = database;
  leveldb::Status* status = new leveldb::Status();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(InitFromTaskRunner, base::Unretained(db_), database_dir,
                     options, destroy_on_corruption, status, metrics_id_),
      base::BindOnce(&ProtoLevelDBWrapper::RunInitCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::Owned(status)));
}

void ProtoLevelDBWrapper::Destroy(
    typename ProtoLevelDBWrapper::DestroyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(DestroyFromTaskRunner, base::Unretained(db_), success,
                     metrics_id_),
      base::BindOnce(RunDestroyCallback, std::move(callback),
                     base::Owned(success)));
}

void ProtoLevelDBWrapper::LoadKeys(
    typename ProtoLevelDBWrapper::LoadKeysCallback callback) {
  LoadKeys(std::string(), std::move(callback));
}

void ProtoLevelDBWrapper::LoadKeys(
    const std::string& target_prefix,
    typename ProtoLevelDBWrapper::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto success = std::make_unique<bool>(false);
  auto keys = std::make_unique<std::vector<std::string>>();
  bool* success_ptr = success.get();
  std::vector<std::string>* keys_ptr = keys.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(LoadKeysFromTaskRunner, base::Unretained(db_),
                     target_prefix, base::Unretained(keys_ptr),
                     base::Unretained(success_ptr), metrics_id_),
      base::BindOnce(RunLoadKeysCallback, std::move(callback),
                     std::move(success), std::move(keys)));
}

void ProtoLevelDBWrapper::SetMetricsId(const std::string& id) {
  metrics_id_ = id;
}

bool ProtoLevelDBWrapper::GetApproximateMemoryUse(uint64_t* approx_mem_use) {
  if (db_ == nullptr)
    return 0;

  return db_->GetApproximateMemoryUse(approx_mem_use);
}

bool ProtoLevelDBWrapper::IsCorrupt() {
  return is_corrupt_;
}

const scoped_refptr<base::SequencedTaskRunner>&
ProtoLevelDBWrapper::task_runner() {
  return task_runner_;
}

}  // namespace leveldb_proto