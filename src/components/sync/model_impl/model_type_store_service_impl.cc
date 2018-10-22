// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_service_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/model_impl/blocking_model_type_store_impl.h"
#include "components/sync/model_impl/model_type_store_backend.h"
#include "components/sync/model_impl/model_type_store_impl.h"

namespace syncer {
namespace {

constexpr base::FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

constexpr base::FilePath::CharType kLevelDBFolderName[] =
    FILE_PATH_LITERAL("LevelDB");

// Initialized ModelTypeStoreBackend, on the backend sequence.
void InitOnBackendSequence(const base::FilePath& level_db_path,
                           ModelTypeStoreBackend* store_backend) {
  base::Optional<ModelError> error = store_backend->Init(level_db_path);
  if (error) {
    LOG(ERROR) << "Failed to initialize ModelTypeStore backend: "
               << error->ToString();
  }
}

std::unique_ptr<BlockingModelTypeStoreImpl, base::OnTaskRunnerDeleter>
CreateBlockingModelTypeStoreOnBackendSequence(
    ModelType type,
    ModelTypeStoreBackend* store_backend) {
  BlockingModelTypeStoreImpl* blocking_store = nullptr;
  if (store_backend->IsInitialized()) {
    blocking_store = new BlockingModelTypeStoreImpl(type, store_backend);
  }
  return std::unique_ptr<BlockingModelTypeStoreImpl,
                         base::OnTaskRunnerDeleter /*[]*/>(
      blocking_store,
      base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
}

void CreateModelTypeStoreOnUIThread(
    ModelType type,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    ModelTypeStore::InitCallback callback,
    std::unique_ptr<BlockingModelTypeStoreImpl, base::OnTaskRunnerDeleter>
        blocking_store) {
  if (blocking_store) {
    std::move(callback).Run(
        /*error=*/base::nullopt,
        std::make_unique<ModelTypeStoreImpl>(type, std::move(blocking_store),
                                             backend_task_runner));
  } else {
    std::move(callback).Run(
        ModelError(FROM_HERE, "ModelTypeStore backend initialization failed"),
        /*store=*/nullptr);
  }
}

}  // namespace

ModelTypeStoreServiceImpl::ModelTypeStoreServiceImpl(
    const base::FilePath& base_path)
    : sync_path_(base_path.Append(base::FilePath(kSyncDataFolderName))),
      leveldb_path_(sync_path_.Append(base::FilePath(kLevelDBFolderName))),
      backend_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      store_backend_(ModelTypeStoreBackend::CreateUninitialized().release(),
                     base::OnTaskRunnerDeleter(backend_task_runner_)),
      weak_ptr_factory_(this) {
  DCHECK(backend_task_runner_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InitOnBackendSequence, leveldb_path_,
                                store_backend_.get()));
}

ModelTypeStoreServiceImpl::~ModelTypeStoreServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

const base::FilePath& ModelTypeStoreServiceImpl::GetSyncDataPath() const {
  return sync_path_;
}

RepeatingModelTypeStoreFactory ModelTypeStoreServiceImpl::GetStoreFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return base::BindRepeating(&ModelTypeStoreServiceImpl::CreateModelTypeStore,
                             weak_ptr_factory_.GetWeakPtr());
}

scoped_refptr<base::SequencedTaskRunner>
ModelTypeStoreServiceImpl::GetBackendTaskRunner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  return backend_task_runner_;
}

std::unique_ptr<BlockingModelTypeStore>
ModelTypeStoreServiceImpl::CreateBlockingStoreFromBackendSequence(
    ModelType type) {
  DCHECK(backend_task_runner_->RunsTasksInCurrentSequence());
  if (!store_backend_) {
    return nullptr;
  }
  return std::make_unique<BlockingModelTypeStoreImpl>(type,
                                                      store_backend_.get());
}

void ModelTypeStoreServiceImpl::CreateModelTypeStore(
    ModelType type,
    ModelTypeStore::InitCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  // BlockingModelTypeStoreImpl must be instantiated in the backend sequence.
  // This also guarantees that the creation is sequenced with the backend's
  // initialization, since we can't know for sure that InitOnBackendSequence()
  // has already run.
  auto task = base::BindOnce(&CreateBlockingModelTypeStoreOnBackendSequence,
                             type, store_backend_.get());

  auto reply = base::BindOnce(&CreateModelTypeStoreOnUIThread, type,
                              backend_task_runner_, std::move(callback));

  base::PostTaskAndReplyWithResult(backend_task_runner_.get(), FROM_HERE,
                                   std::move(task), std::move(reply));
}

}  // namespace syncer
