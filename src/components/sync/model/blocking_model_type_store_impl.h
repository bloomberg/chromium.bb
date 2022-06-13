// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_BLOCKING_MODEL_TYPE_STORE_IMPL_H_
#define COMPONENTS_SYNC_MODEL_BLOCKING_MODEL_TYPE_STORE_IMPL_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/blocking_model_type_store.h"

namespace syncer {

class ModelTypeStoreBackend;

class BlockingModelTypeStoreImpl : public BlockingModelTypeStore {
 public:
  // |backend| must not be null.
  BlockingModelTypeStoreImpl(ModelType type,
                             scoped_refptr<ModelTypeStoreBackend> backend);

  BlockingModelTypeStoreImpl(const BlockingModelTypeStoreImpl&) = delete;
  BlockingModelTypeStoreImpl& operator=(const BlockingModelTypeStoreImpl&) =
      delete;

  ~BlockingModelTypeStoreImpl() override;

  // BlockingModelTypeStore implementation.
  absl::optional<ModelError> ReadData(const IdList& id_list,
                                      RecordList* data_records,
                                      IdList* missing_id_list) override;
  absl::optional<ModelError> ReadAllData(RecordList* data_records) override;
  absl::optional<ModelError> ReadAllMetadata(
      MetadataBatch* metadata_batch) override;
  std::unique_ptr<WriteBatch> CreateWriteBatch() override;
  absl::optional<ModelError> CommitWriteBatch(
      std::unique_ptr<WriteBatch> write_batch) override;
  absl::optional<ModelError> DeleteAllDataAndMetadata() override;

  // For advanced uses that require cross-thread batch posting. Avoid if
  // possible.
  static std::unique_ptr<WriteBatch> CreateWriteBatchForType(ModelType type);

 private:
  const ModelType type_;
  const scoped_refptr<ModelTypeStoreBackend> backend_;

  // Key prefix for data/metadata records of this model type.
  const std::string data_prefix_;
  const std::string metadata_prefix_;

  // Key for this type's global metadata record.
  const std::string global_metadata_key_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_BLOCKING_MODEL_TYPE_STORE_IMPL_H_
