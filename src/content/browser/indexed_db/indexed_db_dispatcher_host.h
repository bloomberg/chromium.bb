// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "components/services/storage/public/mojom/native_file_system_context.mojom-forward.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "storage/browser/blob/mojom/blob_storage_context.mojom-forward.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class IndexedDBContextImpl;
class IndexedDBCursor;
class IndexedDBDataItemReader;
class IndexedDBTransaction;

// All calls but the constructor (including destruction) must
// happen on the IDB sequenced task runner.
class CONTENT_EXPORT IndexedDBDispatcherHost : public blink::mojom::IDBFactory {
 public:
  explicit IndexedDBDispatcherHost(IndexedDBContextImpl* indexed_db_context);
  ~IndexedDBDispatcherHost() override;

  void AddReceiver(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::IDBFactory> pending_receiver);

  void AddDatabaseBinding(
      std::unique_ptr<blink::mojom::IDBDatabase> database,
      mojo::PendingAssociatedReceiver<blink::mojom::IDBDatabase>
          pending_receiver);

  mojo::PendingAssociatedRemote<blink::mojom::IDBCursor> CreateCursorBinding(
      const url::Origin& origin,
      std::unique_ptr<IndexedDBCursor> cursor);
  void RemoveCursorBinding(mojo::ReceiverId receiver_id);

  void AddTransactionBinding(
      std::unique_ptr<blink::mojom::IDBTransaction> transaction,
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction> receiver);

  // A shortcut for accessing our context.
  IndexedDBContextImpl* context() const { return indexed_db_context_; }
  storage::mojom::BlobStorageContext* mojo_blob_storage_context();
  storage::mojom::NativeFileSystemContext* native_file_system_context();

  // Must be called on the IDB sequence.
  base::WeakPtr<IndexedDBDispatcherHost> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void CreateAndBindTransactionImpl(
      mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
          transaction_receiver,
      const url::Origin& origin,
      base::WeakPtr<IndexedDBTransaction> transaction);

  // Bind this receiver to read from this given file.
  void BindFileReader(
      const base::FilePath& path,
      base::Time expected_modification_time,
      base::RepeatingClosure release_callback,
      mojo::PendingReceiver<storage::mojom::BlobDataItemReader> receiver);
  // Removes all readers for this file path.
  void RemoveBoundReaders(const base::FilePath& path);

  // Create external objects from |objects| and store the results in
  // |mojo_objects|.  |mojo_objects| must be the same length as |objects|.
  void CreateAllExternalObjects(
      const url::Origin& origin,
      const std::vector<IndexedDBExternalObject>& objects,
      std::vector<blink::mojom::IDBExternalObjectPtr>* mojo_objects);

 private:
  friend class IndexedDBDispatcherHostTest;

  // blink::mojom::IDBFactory implementation:
  void GetDatabaseInfo(mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                           pending_callbacks) override;
  void GetDatabaseNames(
      mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
          pending_callbacks) override;
  void Open(mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                pending_callbacks,
            mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
                database_callbacks_remote,
            const base::string16& name,
            int64_t version,
            mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
                transaction_receiver,
            int64_t transaction_id) override;
  void DeleteDatabase(mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
                          pending_callbacks,
                      const base::string16& name,
                      bool force_close) override;
  void AbortTransactionsAndCompactDatabase(
      AbortTransactionsAndCompactDatabaseCallback callback) override;
  void AbortTransactionsForDatabase(
      AbortTransactionsForDatabaseCallback callback) override;

  void InvalidateWeakPtrsAndClearBindings();

  base::SequencedTaskRunner* IDBTaskRunner() const;

  // IndexedDBDispatcherHost is owned by IndexedDBContextImpl.
  IndexedDBContextImpl* indexed_db_context_;

  // Shared task runner used to read blob files on.
  scoped_refptr<base::TaskRunner> file_task_runner_;

  mojo::ReceiverSet<blink::mojom::IDBFactory, url::Origin> receivers_;
  mojo::UniqueAssociatedReceiverSet<blink::mojom::IDBDatabase>
      database_receivers_;
  mojo::UniqueAssociatedReceiverSet<blink::mojom::IDBCursor> cursor_receivers_;
  mojo::UniqueAssociatedReceiverSet<blink::mojom::IDBTransaction>
      transaction_receivers_;

  std::map<base::FilePath, std::unique_ptr<IndexedDBDataItemReader>>
      file_reader_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IndexedDBDispatcherHost> weak_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
