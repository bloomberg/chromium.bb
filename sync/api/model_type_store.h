// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_MODEL_TYPE_STORE_H_
#define SYNC_API_MODEL_TYPE_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace syncer_v2 {

// ModelTypeStore is leveldb backed store for model type's data, metadata and
// global metadata.
//
// Store keeps records for entries identified by ids. For each entry store keeps
// data and metadata. Also store keeps one record for global metadata.
//
// To create store call one of Create*Store static factory functions. Model type
// controls store's lifetime with returned unique_ptr. Call to Create*Store
// function triggers asynchronous store backend initialization, callback will be
// called with results when initialization is done.
//
// Read operations are asynchronous, initiated with one of Read* functions,
// provided callback will be called with result code and output of read
// operation.
//
// Write operations are done in context of write batch. To get one call
// CreateWriteBatch(). After that pass write batch object to Write/Delete
// functions. WriteBatch only accumulates pending changes, doesn't actually do
// data modification. Calling CommitWriteBatch writes all accumulated changes to
// disk atomically. Callback passed to CommitWriteBatch will be called with
// result of write operation. If write batch object is destroyed without
// comitting accumulated write operations will not be persisted.
//
// Destroying store object doesn't necessarily cancel asynchronous operations
// issued previously. You should be prepared to handle callbacks from those
// operations.
class SYNC_EXPORT ModelTypeStore {
 public:
  // Result of store operations.
  enum class Result {
    SUCCESS,
    UNSPECIFIED_ERROR,
  };

  // Output of read operations is passed back as list of Record structures.
  struct Record {
    Record(const std::string& id, const std::string& value)
        : id(id), value(value) {}

    std::string id;
    std::string value;
  };

  // WriteBatch object is used in all modification operations.
  class SYNC_EXPORT WriteBatch {
   public:
    virtual ~WriteBatch();

   protected:
    friend class MockModelTypeStore;
    WriteBatch();
  };

  typedef std::vector<Record> RecordList;
  typedef std::vector<std::string> IdList;

  typedef base::Callback<void(Result result,
                              std::unique_ptr<ModelTypeStore> store)>
      InitCallback;
  typedef base::Callback<void(Result result)> CallbackWithResult;
  typedef base::Callback<void(Result result,
                              std::unique_ptr<RecordList> data_records,
                              std::unique_ptr<IdList> missing_id_list)>
      ReadDataCallback;
  typedef base::Callback<void(Result result,
                              std::unique_ptr<RecordList> data_records)>
      ReadAllDataCallback;
  typedef base::Callback<void(Result result,
                              std::unique_ptr<RecordList> metadata_records,
                              const std::string& global_metadata)>
      ReadMetadataCallback;

  // CreateStore takes |path| and |blocking_task_runner|. Here is how to get
  // task runner in production code:
  //
  // base::SequencedWorkerPool* worker_pool =
  //     content::BrowserThread::GetBlockingPool();
  // scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
  //     worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
  //         worker_pool->GetSequenceToken(),
  //         base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  //
  // In test get task runner from MessageLoop::task_runner().
  static void CreateStore(
      const syncer::ModelType type,
      const std::string& path,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const InitCallback& callback);
  // Creates store object backed by in-memory leveldb database. It is used in
  // tests.
  static void CreateInMemoryStoreForTest(const InitCallback& callback);

  virtual ~ModelTypeStore();

  // Read operations return records either for all entries or only for ones
  // identified in |id_list|. Result is SUCCESS if all records were read
  // successfully. If reading any of records fails result is UNSPECIFIED_ERROR
  // and RecordList contains some records that were read successfully. There is
  // no guarantee that RecordList will contain all successfully read records in
  // this case.
  // Callback for ReadData (ReadDataCallback) in addition receives list of ids
  // that were not found in store (missing_id_list).
  virtual void ReadData(const IdList& id_list,
                        const ReadDataCallback& callback) = 0;
  virtual void ReadAllData(const ReadAllDataCallback& callback) = 0;
  // ReadMetadataCallback will be invoked with three parameters: result of
  // operation, list of metadata records and global metadata.
  virtual void ReadAllMetadata(const ReadMetadataCallback& callback) = 0;

  // Creates write batch for write operations.
  virtual std::unique_ptr<WriteBatch> CreateWriteBatch() = 0;

  // Commits write operations accumulated in write batch. If write operation
  // fails result is UNSPECIFIED_ERROR and write operations will not be
  // reflected in the store.
  virtual void CommitWriteBatch(std::unique_ptr<WriteBatch> write_batch,
                                const CallbackWithResult& callback) = 0;

  // Write operations.
  virtual void WriteData(WriteBatch* write_batch,
                         const std::string& id,
                         const std::string& value) = 0;
  virtual void WriteMetadata(WriteBatch* write_batch,
                             const std::string& id,
                             const std::string& value) = 0;
  virtual void WriteGlobalMetadata(WriteBatch* write_batch,
                                   const std::string& value) = 0;
  virtual void DeleteData(WriteBatch* write_batch, const std::string& id) = 0;
  virtual void DeleteMetadata(WriteBatch* write_batch,
                              const std::string& id) = 0;
  virtual void DeleteGlobalMetadata(WriteBatch* write_batch) = 0;
  // TODO(pavely): Consider implementing DeleteAllMetadata with following
  // signature:
  // virtual void DeleteAllMetadata(const CallbackWithResult& callback) = 0.
  // It will delete all metadata records and global metadata record.
};

}  // namespace syncer_v2

#endif  // SYNC_API_MODEL_TYPE_STORE_H_
