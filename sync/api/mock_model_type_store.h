// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_MOCK_MODEL_TYPE_STORE_H_
#define SYNC_API_MOCK_MODEL_TYPE_STORE_H_

#include <string>

#include "base/callback.h"
#include "sync/api/model_type_store.h"

namespace syncer_v2 {

// MockModelTypeStore is implementation of ModelTypeStore that does nothing.
// Use it when testing components that depend on ModelTypeStore.
//
// By default all methods return SUCCESS and empty results. It is possible to
// register custom handlers for certain functions to override behavior. It is
// responsibility of handler to post callback with result.
// Here is an example:
// ===
// void OnReadData(const ModelTypeStore::IdList& id_list,
//                 const ModelTypeStore::ReadRecordsCallback& callback) {
//   // Verify id_list here.
//   // Prepare fake response.
//   scoped_ptr<ModelTypeStore::RecordList> record_list(
//       new ModelTypeStore::RecordList);
//   record_list->push_back(ModelTypeStore::Record("id1", "value1"));
//   base::ThreadTaskRunnerHandle::Get()->PostTask(
//       FROM_HERE, base::Bind(callback, Result::SUCCESS,
//                             base::Passed(record_list)));
// }
//
// MockModelTypeStore mock_model_type_store;
// mock_model_type_store.RegisterReadDataHandler(base::Bind(&OnReadData));
// ModelTypeStore::IdList id_list;
// id_list.push_back("id1");
// mock_model_type_store.ReadData(id_list, base::Bind(&ReadDone));
// ===
// TODO(pavel): When in-memory store is available this class should delegate all
// calls to it instead of returning empty successful results.
class MockModelTypeStore : public ModelTypeStore {
 public:
  // Signatures for all ModelTypeStore virtual functions.
  typedef base::Callback<void(const ReadRecordsCallback&)>
      ReadAllRecordsSignature;
  typedef base::Callback<void(const IdList&, const ReadRecordsCallback&)>
      ReadRecordsSignature;
  typedef base::Callback<void(const ReadMetadataCallback& callback)>
      ReadAllMetadataSignature;
  typedef base::Callback<void(scoped_ptr<WriteBatch>, CallbackWithResult)>
      CommitWriteBatchSignature;
  typedef base::Callback<void(WriteBatch*,
                              const std::string&,
                              const std::string&)> WriteRecordSignature;
  typedef base::Callback<void(WriteBatch*, const std::string&)>
      WriteGlobalMetadataSignature;
  typedef base::Callback<void(WriteBatch*, const std::string&)>
      DeleteRecordSignature;
  typedef base::Callback<void(WriteBatch*)> DeleteGlobalMetadataSignature;

  MockModelTypeStore();
  ~MockModelTypeStore() override;

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

  // Register handler functions.
  void RegisterReadDataHandler(const ReadRecordsSignature& handler);
  void RegisterReadAllDataHandler(const ReadAllRecordsSignature& handler);
  void RegisterReadAllMetadataHandler(const ReadAllMetadataSignature& handler);
  void RegisterCommitWriteBatchHandler(
      const CommitWriteBatchSignature& handler);
  void RegisterWriteDataHandler(const WriteRecordSignature& handler);
  void RegisterWriteMetadataHandler(const WriteRecordSignature& handler);
  void RegisterWriteGlobalMetadataHandler(
      const WriteGlobalMetadataSignature& handler);
  void RegisterDeleteDataHandler(const DeleteRecordSignature& handler);
  void RegisterDeleteMetadataHandler(const DeleteRecordSignature& handler);
  void RegisterDeleteGlobalMetadataHandler(
      const DeleteGlobalMetadataSignature& handler);

 private:
  ReadRecordsSignature read_data_handler_;
  ReadAllRecordsSignature read_all_data_handler_;
  ReadAllMetadataSignature read_all_metadata_handler_;
  CommitWriteBatchSignature commit_write_batch_handler_;
  WriteRecordSignature write_data_handler_;
  WriteRecordSignature write_metadata_handler_;
  WriteGlobalMetadataSignature write_global_metadata_handler_;
  DeleteRecordSignature delete_data_handler_;
  DeleteRecordSignature delete_metadata_handler_;
  DeleteGlobalMetadataSignature delete_global_metadata_handler_;
};

}  // namespace syncer_v2

#endif  // SYNC_API_MOCK_MODEL_TYPE_STORE_H_
