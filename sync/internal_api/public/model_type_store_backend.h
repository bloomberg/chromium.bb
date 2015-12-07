// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_STORE_BACKEND_H_
#define SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_STORE_BACKEND_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "sync/api/model_type_store.h"
#include "sync/base/sync_export.h"

namespace leveldb {
class DB;
class Env;
class WriteBatch;
}  // namespace leveldb

namespace syncer_v2 {

// ModelTypeStoreBackend handles operations with leveldb. It is oblivious of the
// fact that it is called from separate thread (with the exception of ctor),
// meaning it shouldn't deal with callbacks and task_runners.
class SYNC_EXPORT_PRIVATE ModelTypeStoreBackend : public base::NonThreadSafe {
 public:
  ModelTypeStoreBackend();
  ~ModelTypeStoreBackend();

  // Helper function to create in memory environment for leveldb.
  static scoped_ptr<leveldb::Env> CreateInMemoryEnv();

  // Take ownership of env from consumer of ModelTypeStoreBackend object. env
  // will be deleted right after db_ is deleted. This function allows tests to
  // create in-memory store without requiring them to manage env ownership.
  void TakeEnvOwnership(scoped_ptr<leveldb::Env> env);

  // Init opens database at |path|. If database doesn't exist it creates one.
  // Normally |env| should be nullptr, this causes leveldb to use default disk
  // based environment from leveldb::Env::Default().
  // Providing |env| allows to override environment used by leveldb for tests
  // with in-memory or faulty environment.
  ModelTypeStore::Result Init(const std::string& path, leveldb::Env* env);

  // Reads records with keys formed by prepending ids from |id_list| with
  // |prefix|. If the record is found its id (without prefix) and value is
  // appended to record_list. If record is not found its id is appended to
  // |missing_id_list|. It is not an error that records for ids are not found so
  // function will still return success in this case.
  ModelTypeStore::Result ReadRecordsWithPrefix(
      const std::string& prefix,
      const ModelTypeStore::IdList& id_list,
      ModelTypeStore::RecordList* record_list,
      ModelTypeStore::IdList* missing_id_list);

  // Reads all records with keys starting with |prefix|. Prefix is removed from
  // key before it is added to |record_list|.
  ModelTypeStore::Result ReadAllRecordsWithPrefix(
      const std::string& prefix,
      ModelTypeStore::RecordList* record_list);

  // Writes modifications accumulated in |write_batch| to database.
  ModelTypeStore::Result WriteModifications(
      scoped_ptr<leveldb::WriteBatch> write_batch);

 private:
  // In some scenarios ModelTypeStoreBackend holds ownership of env. Typical
  // example is when test creates in memory environment with CreateInMemoryEnv
  // and wants it to be destroyed along with backend. This is achieved by
  // passing ownership of env to TakeEnvOwnership function.
  //
  // env_ declaration should appear before declaration of db_ because
  // environment object should still be valid when db_'s destructor is called.
  scoped_ptr<leveldb::Env> env_;

  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(ModelTypeStoreBackend);
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_STORE_BACKEND_H_
