// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/model_type_store_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "sync/internal_api/public/model_type_store_backend.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace syncer_v2 {

namespace {

// Key prefix for data/metadata records.
const char kDataPrefix[] = "-dt-";
const char kMetadataPrefix[] = "-md-";

// Key for global metadata record.
const char kGlobalMetadataKey[] = "GlobalMetadata";

void NoOpForBackendDtor(std::unique_ptr<ModelTypeStoreBackend> backend) {
  // This function was intentionally left blank.
}

}  // namespace

// static
std::string ModelTypeStoreImpl::FormatDataPrefix(const syncer::ModelType type) {
  return std::string(syncer::ModelTypeToTag(type)) + kDataPrefix;
}

// static
std::string ModelTypeStoreImpl::FormatMetaPrefix(const syncer::ModelType type) {
  return std::string(syncer::ModelTypeToTag(type)) + kMetadataPrefix;
}

// static
leveldb::WriteBatch* ModelTypeStoreImpl::GetLeveldbWriteBatch(
    WriteBatch* write_batch) {
  return static_cast<WriteBatchImpl*>(write_batch)->leveldb_write_batch_.get();
}

std::string ModelTypeStoreImpl::FormatDataKey(const std::string& id) {
  return dataPrefix_ + id;
}

std::string ModelTypeStoreImpl::FormatMetadataKey(const std::string& id) {
  return metadataPrefix_ + id;
}

ModelTypeStoreImpl::ModelTypeStoreImpl(
    const syncer::ModelType type,
    std::unique_ptr<ModelTypeStoreBackend> backend,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : backend_(std::move(backend)),
      backend_task_runner_(backend_task_runner),
      dataPrefix_(FormatDataPrefix(type)),
      metadataPrefix_(FormatMetaPrefix(type)),
      weak_ptr_factory_(this) {
  DCHECK(backend_);
  DCHECK(backend_task_runner_);
}

ModelTypeStoreImpl::~ModelTypeStoreImpl() {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&NoOpForBackendDtor, base::Passed(&backend_)));
}

// static
void ModelTypeStoreImpl::CreateStore(
    const syncer::ModelType type,
    const std::string& path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const InitCallback& callback) {
  DCHECK(!callback.is_null());

  std::unique_ptr<ModelTypeStoreBackend> backend(new ModelTypeStoreBackend());

  std::unique_ptr<ModelTypeStoreImpl> store(
      new ModelTypeStoreImpl(type, std::move(backend), blocking_task_runner));

  auto task =
      base::Bind(&ModelTypeStoreBackend::Init,
                 base::Unretained(store->backend_.get()), path, nullptr);
  auto reply = base::Bind(&ModelTypeStoreImpl::BackendInitDone, callback,
                          base::Passed(&store));

  base::PostTaskAndReplyWithResult(blocking_task_runner.get(), FROM_HERE, task,
                                   reply);
}

// static
void ModelTypeStoreImpl::CreateInMemoryStoreForTest(
    const InitCallback& callback) {
  DCHECK(!callback.is_null());

  std::unique_ptr<leveldb::Env> env =
      ModelTypeStoreBackend::CreateInMemoryEnv();

  // Env ownership will be passed to backend, but we still need to keep pointer
  // for Init call.
  leveldb::Env* env_ptr = env.get();

  std::unique_ptr<ModelTypeStoreBackend> backend(new ModelTypeStoreBackend());
  backend->TakeEnvOwnership(std::move(env));

  // In-memory store backend works on the same thread as test.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  std::unique_ptr<ModelTypeStoreImpl> store(new ModelTypeStoreImpl(
      syncer::UNSPECIFIED, std::move(backend), task_runner));

  std::string path;
  env_ptr->GetTestDirectory(&path);
  path += "/in-memory";

  auto task =
      base::Bind(&ModelTypeStoreBackend::Init,
                 base::Unretained(store->backend_.get()), path, env_ptr);
  auto reply = base::Bind(&ModelTypeStoreImpl::BackendInitDone, callback,
                          base::Passed(&store));

  base::PostTaskAndReplyWithResult(task_runner.get(), FROM_HERE, task, reply);
}

// static
void ModelTypeStoreImpl::BackendInitDone(
    const InitCallback& callback,
    std::unique_ptr<ModelTypeStoreImpl> store,
    Result result) {
  DCHECK(store->CalledOnValidThread());
  if (result != Result::SUCCESS) {
    store.reset();
  }
  callback.Run(result, std::move(store));
}

// Note on pattern for communicating with backend:
//  - API function (e.g. ReadData) allocates lists for output.
//  - API function prepares two callbacks: task that will be posted on backend
//    thread and reply which will be posted on model type thread once task
//    finishes.
//  - Task for backend thread takes raw pointers to output lists while reply
//    takes ownership of those lists. This allows backend interface to be simple
//    while ensuring proper objects' lifetime.
//  - Function bound by reply calls consumer's callback and passes ownership of
//    output lists to it.

void ModelTypeStoreImpl::ReadData(const IdList& id_list,
                                  const ReadDataCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  std::unique_ptr<IdList> missing_id_list(new IdList());

  auto task = base::Bind(&ModelTypeStoreBackend::ReadRecordsWithPrefix,
                         base::Unretained(backend_.get()), dataPrefix_, id_list,
                         base::Unretained(record_list.get()),
                         base::Unretained(missing_id_list.get()));
  auto reply = base::Bind(
      &ModelTypeStoreImpl::ReadDataDone, weak_ptr_factory_.GetWeakPtr(),
      callback, base::Passed(&record_list), base::Passed(&missing_id_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::ReadDataDone(const ReadDataCallback& callback,
                                      std::unique_ptr<RecordList> record_list,
                                      std::unique_ptr<IdList> missing_id_list,
                                      Result result) {
  DCHECK(CalledOnValidThread());
  callback.Run(result, std::move(record_list), std::move(missing_id_list));
}

void ModelTypeStoreImpl::ReadAllData(const ReadAllDataCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  std::unique_ptr<RecordList> record_list(new RecordList());
  auto task = base::Bind(&ModelTypeStoreBackend::ReadAllRecordsWithPrefix,
                         base::Unretained(backend_.get()), dataPrefix_,
                         base::Unretained(record_list.get()));
  auto reply = base::Bind(&ModelTypeStoreImpl::ReadAllDataDone,
                          weak_ptr_factory_.GetWeakPtr(), callback,
                          base::Passed(&record_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::ReadAllDataDone(
    const ReadAllDataCallback& callback,
    std::unique_ptr<RecordList> record_list,
    Result result) {
  DCHECK(CalledOnValidThread());
  callback.Run(result, std::move(record_list));
}

void ModelTypeStoreImpl::ReadAllMetadata(const ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());

  // ReadAllMetadata performs two steps sequentially: read all metadata records
  // and then read global metadata record. Start reading metadata records here.
  // When this read operation is done ReadMetadataRecordsDone callback will
  // issue read operation for global metadata record.
  std::unique_ptr<RecordList> metadata_records(new RecordList());
  auto task = base::Bind(&ModelTypeStoreBackend::ReadAllRecordsWithPrefix,
                         base::Unretained(backend_.get()), metadataPrefix_,
                         base::Unretained(metadata_records.get()));
  auto reply = base::Bind(&ModelTypeStoreImpl::ReadMetadataRecordsDone,
                          weak_ptr_factory_.GetWeakPtr(), callback,
                          base::Passed(&metadata_records));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::ReadMetadataRecordsDone(
    const ReadMetadataCallback& callback,
    std::unique_ptr<RecordList> metadata_records,
    Result result) {
  DCHECK(CalledOnValidThread());
  if (result != Result::SUCCESS) {
    callback.Run(result, std::move(metadata_records), std::string());
    return;
  }

  IdList global_metadata_id;
  global_metadata_id.push_back(kGlobalMetadataKey);
  std::unique_ptr<RecordList> global_metadata_records(new RecordList());
  std::unique_ptr<IdList> missing_id_list(new IdList());
  auto task = base::Bind(&ModelTypeStoreBackend::ReadRecordsWithPrefix,
                         base::Unretained(backend_.get()), std::string(),
                         global_metadata_id,
                         base::Unretained(global_metadata_records.get()),
                         base::Unretained(missing_id_list.get()));
  auto reply = base::Bind(
      &ModelTypeStoreImpl::ReadAllMetadataDone, weak_ptr_factory_.GetWeakPtr(),
      callback, base::Passed(&metadata_records),
      base::Passed(&global_metadata_records), base::Passed(&missing_id_list));
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::ReadAllMetadataDone(
    const ReadMetadataCallback& callback,
    std::unique_ptr<RecordList> metadata_records,
    std::unique_ptr<RecordList> global_metadata_records,
    std::unique_ptr<IdList> missing_id_list,
    Result result) {
  DCHECK(CalledOnValidThread());
  if (result != Result::SUCCESS) {
    callback.Run(result, std::move(metadata_records), std::string());
    return;
  }

  if (!missing_id_list->empty()) {
    // Missing global metadata record is not an error. We shouild return empty
    // string in this case.
    DCHECK((*missing_id_list)[0] == kGlobalMetadataKey);
    DCHECK(global_metadata_records->empty());
    callback.Run(Result::SUCCESS, std::move(metadata_records), std::string());
    return;
  }
  DCHECK(!global_metadata_records->empty());
  DCHECK((*global_metadata_records)[0].id == kGlobalMetadataKey);
  callback.Run(Result::SUCCESS, std::move(metadata_records),
               (*global_metadata_records)[0].value);
}

std::unique_ptr<ModelTypeStore::WriteBatch>
ModelTypeStoreImpl::CreateWriteBatch() {
  DCHECK(CalledOnValidThread());
  return base::WrapUnique(new WriteBatchImpl());
}

void ModelTypeStoreImpl::CommitWriteBatch(
    std::unique_ptr<WriteBatch> write_batch,
    const CallbackWithResult& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());
  WriteBatchImpl* write_batch_impl =
      static_cast<WriteBatchImpl*>(write_batch.get());
  auto task = base::Bind(&ModelTypeStoreBackend::WriteModifications,
                         base::Unretained(backend_.get()),
                         base::Passed(&write_batch_impl->leveldb_write_batch_));
  auto reply = base::Bind(&ModelTypeStoreImpl::WriteModificationsDone,
                          weak_ptr_factory_.GetWeakPtr(), callback);
  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE, task,
                                   reply);
}

void ModelTypeStoreImpl::WriteModificationsDone(
    const CallbackWithResult& callback,
    Result result) {
  DCHECK(CalledOnValidThread());
  callback.Run(result);
}

void ModelTypeStoreImpl::WriteData(WriteBatch* write_batch,
                                   const std::string& id,
                                   const std::string& value) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Put(FormatDataKey(id), value);
}

void ModelTypeStoreImpl::WriteMetadata(WriteBatch* write_batch,
                                       const std::string& id,
                                       const std::string& value) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Put(FormatMetadataKey(id), value);
}

void ModelTypeStoreImpl::WriteGlobalMetadata(WriteBatch* write_batch,
                                             const std::string& value) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Put(kGlobalMetadataKey, value);
}

void ModelTypeStoreImpl::DeleteData(WriteBatch* write_batch,
                                    const std::string& id) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Delete(FormatDataKey(id));
}

void ModelTypeStoreImpl::DeleteMetadata(WriteBatch* write_batch,
                                        const std::string& id) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Delete(FormatMetadataKey(id));
}

void ModelTypeStoreImpl::DeleteGlobalMetadata(WriteBatch* write_batch) {
  DCHECK(CalledOnValidThread());
  GetLeveldbWriteBatch(write_batch)->Delete(kGlobalMetadataKey);
}

ModelTypeStoreImpl::WriteBatchImpl::WriteBatchImpl() {
  leveldb_write_batch_.reset(new leveldb::WriteBatch());
}

ModelTypeStoreImpl::WriteBatchImpl::~WriteBatchImpl() {}

}  // namespace syncer_v2
