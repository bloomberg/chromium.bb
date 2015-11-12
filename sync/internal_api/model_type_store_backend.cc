// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/model_type_store_backend.h"

#include "base/files/file_path.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace syncer_v2 {

ModelTypeStoreBackend::ModelTypeStoreBackend() {
  // ModelTypeStoreBackend is used on different thread from the one where it is
  // created.
  DetachFromThread();
}

ModelTypeStoreBackend::~ModelTypeStoreBackend() {
  DCHECK(CalledOnValidThread());
}

scoped_ptr<leveldb::Env> ModelTypeStoreBackend::CreateInMemoryEnv() {
  return make_scoped_ptr(leveldb::NewMemEnv(leveldb::Env::Default()));
}

void ModelTypeStoreBackend::TakeEnvOwnership(scoped_ptr<leveldb::Env> env) {
  env_ = env.Pass();
}

ModelTypeStore::Result ModelTypeStoreBackend::Init(const std::string& path,
                                                   leveldb::Env* env) {
  DCHECK(CalledOnValidThread());
  leveldb::DB* db_raw = nullptr;

  leveldb::Options options;
  options.create_if_missing = true;
  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;
  options.paranoid_checks = true;
  if (env)
    options.env = env;
  leveldb::Status status = leveldb::DB::Open(options, path, &db_raw);
  if (!status.ok()) {
    DCHECK(db_raw == nullptr);
    return ModelTypeStore::Result::UNSPECIFIED_ERROR;
  }
  db_.reset(db_raw);
  return ModelTypeStore::Result::SUCCESS;
}

ModelTypeStore::Result ModelTypeStoreBackend::ReadRecordsWithPrefix(
    const std::string& prefix,
    const ModelTypeStore::IdList& id_list,
    ModelTypeStore::RecordList* record_list,
    ModelTypeStore::IdList* missing_id_list) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_);
  record_list->reserve(id_list.size());
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  std::string key;
  std::string value;
  for (const std::string& id : id_list) {
    key = prefix + id;
    leveldb::Status status = db_->Get(read_options, key, &value);
    if (status.ok()) {
      // TODO(pavely): Use emplace_back instead of push_back once it is allowed.
      record_list->push_back(ModelTypeStore::Record(id, value));
    } else if (status.IsNotFound()) {
      missing_id_list->push_back(id);
    } else {
      return ModelTypeStore::Result::UNSPECIFIED_ERROR;
    }
  }
  return ModelTypeStore::Result::SUCCESS;
}

ModelTypeStore::Result ModelTypeStoreBackend::ReadAllRecordsWithPrefix(
    const std::string& prefix,
    ModelTypeStore::RecordList* record_list) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_);
  leveldb::ReadOptions read_options;
  read_options.verify_checksums = true;
  scoped_ptr<leveldb::Iterator> iter(db_->NewIterator(read_options));
  const leveldb::Slice prefix_slice(prefix);
  for (iter->Seek(prefix_slice); iter->Valid(); iter->Next()) {
    leveldb::Slice key = iter->key();
    if (!key.starts_with(prefix_slice))
      break;
    key.remove_prefix(prefix_slice.size());
    // TODO(pavely): Use emplace_back instead of push_back once it is allowed.
    record_list->push_back(
        ModelTypeStore::Record(key.ToString(), iter->value().ToString()));
  }
  return iter->status().ok() ? ModelTypeStore::Result::SUCCESS
                             : ModelTypeStore::Result::UNSPECIFIED_ERROR;
}

ModelTypeStore::Result ModelTypeStoreBackend::WriteModifications(
    scoped_ptr<leveldb::WriteBatch> write_batch) {
  DCHECK(CalledOnValidThread());
  DCHECK(db_);
  leveldb::Status status =
      db_->Write(leveldb::WriteOptions(), write_batch.get());
  return status.ok() ? ModelTypeStore::Result::SUCCESS
                     : ModelTypeStore::Result::UNSPECIFIED_ERROR;
}

}  // namespace syncer_v2
