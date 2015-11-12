// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/model_type_store_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "sync/internal_api/public/model_type_store_backend.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace syncer_v2 {

namespace {

void NoOpForBackendDtor(scoped_ptr<ModelTypeStoreBackend> backend) {
  // This function was intentionally left blank.
}

}  // namespace

ModelTypeStoreImpl::ModelTypeStoreImpl(
    scoped_ptr<ModelTypeStoreBackend> backend,
    scoped_refptr<base::SingleThreadTaskRunner> backend_task_runner)
    : backend_(backend.Pass()), backend_task_runner_(backend_task_runner) {
  DCHECK(backend_);
  DCHECK(backend_task_runner_);
}

ModelTypeStoreImpl::~ModelTypeStoreImpl() {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&NoOpForBackendDtor, base::Passed(&backend_)));
}

// static
void ModelTypeStoreImpl::CreateInMemoryStoreForTest(
    const InitCallback& callback) {
  DCHECK(!callback.is_null());
  // In-memory store backend works on the same thread as test.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  scoped_ptr<ModelTypeStoreBackend> backend(new ModelTypeStoreBackend());
  scoped_ptr<leveldb::Env> env = ModelTypeStoreBackend::CreateInMemoryEnv();

  std::string path;
  env->GetTestDirectory(&path);
  path += "/in-memory";

  // Env ownership will be passed to backend, but we still need to keep pointer
  // for Init call.
  leveldb::Env* env_ptr = env.get();
  backend->TakeEnvOwnership(env.Pass());
  scoped_ptr<ModelTypeStoreImpl> store(
      new ModelTypeStoreImpl(backend.Pass(), task_runner));

  auto task =
      base::Bind(&ModelTypeStoreBackend::Init,
                 base::Unretained(store->backend_.get()), path, env_ptr);
  auto reply = base::Bind(&ModelTypeStoreImpl::BackendInitDone, callback,
                          base::Passed(&store));

  base::PostTaskAndReplyWithResult(task_runner.get(), FROM_HERE, task, reply);
}

// static
void ModelTypeStoreImpl::BackendInitDone(const InitCallback& callback,
                                         scoped_ptr<ModelTypeStoreImpl> store,
                                         Result result) {
  DCHECK(store->CalledOnValidThread());
  if (result != Result::SUCCESS) {
    store.reset();
  }
  callback.Run(result, store.Pass());
}

void ModelTypeStoreImpl::ReadData(const IdList& id_list,
                                  const ReadRecordsCallback& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ModelTypeStoreImpl::ReadAllData(const ReadRecordsCallback& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ModelTypeStoreImpl::ReadAllMetadata(const ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

scoped_ptr<ModelTypeStore::WriteBatch> ModelTypeStoreImpl::CreateWriteBatch() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  return make_scoped_ptr(new WriteBatchImpl());
}

void ModelTypeStoreImpl::CommitWriteBatch(scoped_ptr<WriteBatch> write_batch,
                                          const CallbackWithResult& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ModelTypeStoreImpl::WriteData(WriteBatch* write_batch,
                                   const std::string& id,
                                   const std::string& value) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ModelTypeStoreImpl::WriteMetadata(WriteBatch* write_batch,
                                       const std::string& id,
                                       const std::string& value) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ModelTypeStoreImpl::WriteGlobalMetadata(WriteBatch* write_batch,
                                             const std::string& value) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ModelTypeStoreImpl::DeleteData(WriteBatch* write_batch,
                                    const std::string& id) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ModelTypeStoreImpl::DeleteMetadata(WriteBatch* write_batch,
                                        const std::string& id) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void ModelTypeStoreImpl::DeleteGlobalMetadata(WriteBatch* write_batch) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

}  // namespace syncer_v2
