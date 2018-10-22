// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_SERVICE_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {

class BlockingModelTypeStore;

// Handles the shared resources for ModelTypeStore and related classes,
// including a shared background sequence runner.
class ModelTypeStoreService : public KeyedService {
 public:
  // Returns the root directory under which sync stores data.
  // This doesn't belong here strictly speaking, but it is convenient to
  // centralize all storage-related paths in one class.
  virtual const base::FilePath& GetSyncDataPath() const = 0;

  // Returns a factory to create instances of ModelTypeStore.
  virtual RepeatingModelTypeStoreFactory GetStoreFactory() = 0;

  virtual scoped_refptr<base::SequencedTaskRunner> GetBackendTaskRunner() = 0;

  // Creates a BlockingModelTypeStore. Must be called from the backend
  // sequence as returned in GetBackendTaskRunner(). Can return null if there
  // was an error.
  virtual std::unique_ptr<BlockingModelTypeStore>
  CreateBlockingStoreFromBackendSequence(ModelType type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_STORE_SERVICE_H_
