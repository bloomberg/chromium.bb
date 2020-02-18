// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_database.h"

namespace content {
namespace {

using blink::IndexedDBKey;
using blink::IndexedDBKeyRange;

}  // namespace

IndexedDBFakeBackingStore::IndexedDBFakeBackingStore()
    : IndexedDBBackingStore(IndexedDBBackingStore::Mode::kInMemory,
                            nullptr /* indexed_db_factory */,
                            indexed_db::LevelDBFactory::Get(),
                            url::Origin::Create(GURL("http://localhost:81")),
                            base::FilePath(),
                            std::unique_ptr<TransactionalLevelDBDatabase>(),
                            base::SequencedTaskRunnerHandle::Get().get()) {}
IndexedDBFakeBackingStore::IndexedDBFakeBackingStore(
    IndexedDBFactory* factory,
    base::SequencedTaskRunner* task_runner)
    : IndexedDBBackingStore(IndexedDBBackingStore::Mode::kOnDisk,
                            factory,
                            indexed_db::LevelDBFactory::Get(),
                            url::Origin::Create(GURL("http://localhost:81")),
                            base::FilePath(),
                            std::unique_ptr<TransactionalLevelDBDatabase>(),
                            task_runner) {}
IndexedDBFakeBackingStore::~IndexedDBFakeBackingStore() {}

leveldb::Status IndexedDBFakeBackingStore::DeleteDatabase(
    const base::string16& name,
    TransactionalLevelDBTransaction* transaction) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::PutRecord(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey& key,
    IndexedDBValue* value,
    RecordIdentifier* record) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::ClearObjectStore(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::DeleteRecord(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    const RecordIdentifier&) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::GetKeyGeneratorCurrentNumber(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    int64_t* current_number) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::MaybeUpdateKeyGeneratorCurrentNumber(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    int64_t new_number,
    bool check_current) {
  return leveldb::Status::OK();
}
leveldb::Status IndexedDBFakeBackingStore::KeyExistsInObjectStore(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKey&,
    RecordIdentifier* found_record_identifier,
    bool* found) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::ClearIndex(Transaction*,
                                                      int64_t database_id,
                                                      int64_t object_store_id,
                                                      int64_t index_id) {
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBFakeBackingStore::PutIndexDataForRecord(
    Transaction*,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKey&,
    const RecordIdentifier&) {
  return leveldb::Status::OK();
}

void IndexedDBFakeBackingStore::ReportBlobUnused(int64_t database_id,
                                                 int64_t blob_key) {}

std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    leveldb::Status* s) {
  return std::unique_ptr<IndexedDBBackingStore::Cursor>();
}
std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenObjectStoreCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    leveldb::Status* s) {
  return std::unique_ptr<IndexedDBBackingStore::Cursor>();
}
std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexKeyCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    leveldb::Status* s) {
  return std::unique_ptr<IndexedDBBackingStore::Cursor>();
}
std::unique_ptr<IndexedDBBackingStore::Cursor>
IndexedDBFakeBackingStore::OpenIndexCursor(
    IndexedDBBackingStore::Transaction* transaction,
    int64_t database_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection,
    leveldb::Status* s) {
  return std::unique_ptr<IndexedDBBackingStore::Cursor>();
}

IndexedDBFakeBackingStore::FakeTransaction::FakeTransaction(
    leveldb::Status result)
    : IndexedDBBackingStore::Transaction(nullptr, true), result_(result) {}
void IndexedDBFakeBackingStore::FakeTransaction::Begin(
    std::vector<ScopeLock> locks) {}
leveldb::Status IndexedDBFakeBackingStore::FakeTransaction::CommitPhaseOne(
    BlobWriteCallback callback) {
  return std::move(callback).Run(
      IndexedDBBackingStore::BlobWriteResult::kRunPhaseTwoAndReturnResult);
}
leveldb::Status IndexedDBFakeBackingStore::FakeTransaction::CommitPhaseTwo() {
  return result_;
}
uint64_t IndexedDBFakeBackingStore::FakeTransaction::GetTransactionSize() {
  return 0;
}
leveldb::Status IndexedDBFakeBackingStore::FakeTransaction::Rollback() {
  return leveldb::Status::OK();
}

std::unique_ptr<IndexedDBBackingStore::Transaction>
IndexedDBFakeBackingStore::CreateTransaction(bool relaxed_durability) {
  return std::make_unique<FakeTransaction>(leveldb::Status::OK());
}

}  // namespace content
