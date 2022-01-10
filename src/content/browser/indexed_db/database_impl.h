// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_DATABASE_IMPL_H_
#define CONTENT_BROWSER_INDEXED_DB_DATABASE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace blink {
class IndexedDBKeyRange;
}

namespace content {
class IndexedDBConnection;
class IndexedDBContextImpl;
class IndexedDBDispatcherHost;

class DatabaseImpl : public blink::mojom::IDBDatabase {
 public:
  explicit DatabaseImpl(std::unique_ptr<IndexedDBConnection> connection,
                        const blink::StorageKey& storage_key,
                        IndexedDBDispatcherHost* dispatcher_host,
                        scoped_refptr<base::SequencedTaskRunner> idb_runner);

  DatabaseImpl(const DatabaseImpl&) = delete;
  DatabaseImpl& operator=(const DatabaseImpl&) = delete;

  ~DatabaseImpl() override;

  // blink::mojom::IDBDatabase implementation
  void RenameObjectStore(int64_t transaction_id,
                         int64_t object_store_id,
                         const std::u16string& new_name) override;
  void CreateTransaction(
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
          transaction_receiver,
      int64_t transaction_id,
      const std::vector<int64_t>& object_store_ids,
      blink::mojom::IDBTransactionMode mode,
      blink::mojom::IDBTransactionDurability durability) override;
  void Close() override;
  void VersionChangeIgnored() override;
  void Get(int64_t transaction_id,
           int64_t object_store_id,
           int64_t index_id,
           const blink::IndexedDBKeyRange& key_range,
           bool key_only,
           blink::mojom::IDBDatabase::GetCallback callback) override;
  void GetAll(int64_t transaction_id,
              int64_t object_store_id,
              int64_t index_id,
              const blink::IndexedDBKeyRange& key_range,
              bool key_only,
              int64_t max_count,
              blink::mojom::IDBDatabase::GetAllCallback callback) override;
  void SetIndexKeys(
      int64_t transaction_id,
      int64_t object_store_id,
      const blink::IndexedDBKey& primary_key,
      const std::vector<blink::IndexedDBIndexKeys>& index_keys) override;
  void SetIndexesReady(int64_t transaction_id,
                       int64_t object_store_id,
                       const std::vector<int64_t>& index_ids) override;
  void OpenCursor(
      int64_t transaction_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKeyRange& key_range,
      blink::mojom::IDBCursorDirection direction,
      bool key_only,
      blink::mojom::IDBTaskType task_type,
      blink::mojom::IDBDatabase::OpenCursorCallback callback) override;
  void Count(int64_t transaction_id,
             int64_t object_store_id,
             int64_t index_id,
             const blink::IndexedDBKeyRange& key_range,
             mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                 pending_callbacks) override;
  void DeleteRange(int64_t transaction_id,
                   int64_t object_store_id,
                   const blink::IndexedDBKeyRange& key_range,
                   mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                       pending_callbacks) override;
  void GetKeyGeneratorCurrentNumber(
      int64_t transaction_id,
      int64_t object_store_id,
      mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
          pending_callbacks) override;
  void Clear(int64_t transaction_id,
             int64_t object_store_id,
             mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                 pending_callbacks) override;
  void CreateIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const std::u16string& name,
                   const blink::IndexedDBKeyPath& key_path,
                   bool unique,
                   bool multi_entry) override;
  void DeleteIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id) override;
  void RenameIndex(int64_t transaction_id,
                   int64_t object_store_id,
                   int64_t index_id,
                   const std::u16string& new_name) override;
  void Abort(int64_t transaction_id) override;

 private:
  // This raw pointer is safe because all DatabaseImpl instances are owned by
  // an IndexedDBDispatcherHost.
  raw_ptr<IndexedDBDispatcherHost> dispatcher_host_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  std::unique_ptr<IndexedDBConnection> connection_;
  const blink::StorageKey storage_key_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_DATABASE_IMPL_H_
