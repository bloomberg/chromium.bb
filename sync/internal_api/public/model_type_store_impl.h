// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_STORE_IMPL_H_
#define SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_STORE_IMPL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/non_thread_safe.h"
#include "sync/api/model_type_store.h"

namespace syncer_v2 {

class ModelTypeStoreBackend;

// ModelTypeStoreImpl handles details of store initialization, threading and
// leveldb key formatting. Actual leveldb IO calls are performed by
// ModelTypeStoreBackend.
class ModelTypeStoreImpl : public ModelTypeStore, public base::NonThreadSafe {
 public:
  ~ModelTypeStoreImpl() override;

  static void CreateInMemoryStoreForTest(const InitCallback& callback);

  // ModelTypeStore implementation.
  void ReadData(const IdList& id_list,
                const ReadRecordsCallback& callback) override;
  void ReadAllData(const ReadRecordsCallback& callback) override;
  void ReadAllMetadata(const ReadMetadataCallback& callback) override;
  scoped_ptr<WriteBatch> CreateWriteBatch() override;
  void CommitWriteBatch(scoped_ptr<WriteBatch> write_batch,
                        const CallbackWithResult& callback) override;
  void WriteData(WriteBatch* write_batch,
                 const std::string& id,
                 const std::string& value) override;
  void WriteMetadata(WriteBatch* write_batch,
                     const std::string& id,
                     const std::string& value) override;
  void WriteGlobalMetadata(WriteBatch* write_batch,
                           const std::string& value) override;
  void DeleteData(WriteBatch* write_batch, const std::string& id) override;
  void DeleteMetadata(WriteBatch* write_batch, const std::string& id) override;
  void DeleteGlobalMetadata(WriteBatch* write_batch) override;

 private:
  class WriteBatchImpl : public ModelTypeStore::WriteBatch {
   public:
  };

  static void BackendInitDone(const InitCallback& callback,
                              scoped_ptr<ModelTypeStoreImpl> store,
                              Result result);

  ModelTypeStoreImpl(
      scoped_ptr<ModelTypeStoreBackend> backend,
      scoped_refptr<base::SingleThreadTaskRunner> backend_task_runner);

  // Backend is owned by store, but should be deleted on backend thread. To
  // accomplish this store's dtor posts task to backend thread passing backend
  // ownership to task parameter.
  scoped_ptr<ModelTypeStoreBackend> backend_;
  scoped_refptr<base::SingleThreadTaskRunner> backend_task_runner_;
};

}  // namespace syncer_v2

#endif  // SYNC_INTERNAL_API_PUBLIC_MODEL_TYPE_STORE_IMPL_H_
