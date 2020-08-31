// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/database_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/transaction_impl.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

using blink::IndexedDBIndexKeys;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using std::swap;

namespace blink {
class IndexedDBKeyRange;
}

namespace content {
namespace {

const char kBadTransactionMode[] = "Bad transaction mode";
const char kTransactionAlreadyExists[] = "Transaction already exists";

}  // namespace

DatabaseImpl::DatabaseImpl(std::unique_ptr<IndexedDBConnection> connection,
                           const url::Origin& origin,
                           IndexedDBDispatcherHost* dispatcher_host,
                           scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : dispatcher_host_(dispatcher_host),
      indexed_db_context_(dispatcher_host->context()),
      connection_(std::move(connection)),
      origin_(origin),
      idb_runner_(std::move(idb_runner)) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection_);
  indexed_db_context_->ConnectionOpened(origin_, connection_.get());
}

DatabaseImpl::~DatabaseImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status status;
  if (connection_->IsConnected()) {
    status = connection_->AbortTransactionsAndClose(
        IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
  }
  indexed_db_context_->ConnectionClosed(origin_, connection_.get());
  if (!status.ok()) {
    indexed_db_context_->GetIDBFactory()->OnDatabaseError(
        origin_, status, "Error during rollbacks.");
  }
}

void DatabaseImpl::RenameObjectStore(int64_t transaction_id,
                                     int64_t object_store_id,
                                     const base::string16& new_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "RenameObjectStore must be called from a version change transaction.");
    return;
  }

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::RenameObjectStoreOperation,
                        connection_->database()->AsWeakPtr(), object_store_id,
                        new_name));
}

void DatabaseImpl::CreateTransaction(
    mojo::PendingAssociatedReceiver<blink::mojom::IDBTransaction>
        transaction_receiver,
    int64_t transaction_id,
    const std::vector<int64_t>& object_store_ids,
    blink::mojom::IDBTransactionMode mode,
    blink::mojom::IDBTransactionDurability durability) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  if (mode != blink::mojom::IDBTransactionMode::ReadOnly &&
      mode != blink::mojom::IDBTransactionMode::ReadWrite) {
    mojo::ReportBadMessage(kBadTransactionMode);
    return;
  }

  if (connection_->GetTransaction(transaction_id)) {
    mojo::ReportBadMessage(kTransactionAlreadyExists);
    return;
  }

  IndexedDBTransaction* transaction = connection_->CreateTransaction(
      transaction_id,
      std::set<int64_t>(object_store_ids.begin(), object_store_ids.end()), mode,
      new IndexedDBBackingStore::Transaction(
          connection_->database()->backing_store()->AsWeakPtr(), durability,
          mode));
  connection_->database()->RegisterAndScheduleTransaction(transaction);

  dispatcher_host_->CreateAndBindTransactionImpl(
      std::move(transaction_receiver), origin_, transaction->AsWeakPtr());
}

void DatabaseImpl::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  leveldb::Status status = connection_->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kReturnOnFirstError);

  if (!status.ok()) {
    indexed_db_context_->GetIDBFactory()->OnDatabaseError(
        origin_, status, "Error during rollbacks.");
  }
}

void DatabaseImpl::VersionChangeIgnored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  connection_->VersionChangeIgnored();
}

void DatabaseImpl::AddObserver(int64_t transaction_id,
                               int32_t observer_id,
                               bool include_transaction,
                               bool no_records,
                               bool values,
                               uint32_t operation_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  IndexedDBObserver::Options options(include_transaction, no_records, values,
                                     operation_types);
  connection_->database()->AddPendingObserver(transaction, observer_id,
                                              options);
}

void DatabaseImpl::RemoveObservers(const std::vector<int32_t>& observers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  connection_->RemoveObservers(observers);
}

void DatabaseImpl::Get(int64_t transaction_id,
                       int64_t object_store_id,
                       int64_t index_id,
                       const IndexedDBKeyRange& key_range,
                       bool key_only,
                       blink::mojom::IDBDatabase::GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBDatabase::GetCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBDatabase::GetCallback,
                                    blink::mojom::IDBDatabaseGetResultPtr>(
          std::move(callback), transaction->AsWeakPtr());

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetOperation, connection_->database()->AsWeakPtr(),
      dispatcher_host_->AsWeakPtr(), object_store_id, index_id,
      std::make_unique<IndexedDBKeyRange>(key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      std::move(aborting_callback)));
}

void DatabaseImpl::GetAll(int64_t transaction_id,
                          int64_t object_store_id,
                          int64_t index_id,
                          const IndexedDBKeyRange& key_range,
                          bool key_only,
                          int64_t max_count,
                          blink::mojom::IDBDatabase::GetAllCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBDatabase::GetAllCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBDatabase::GetAllCallback,
                                    blink::mojom::IDBDatabaseGetAllResultPtr>(
          std::move(callback), transaction->AsWeakPtr());

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetAllOperation, connection_->database()->AsWeakPtr(),
      dispatcher_host_->AsWeakPtr(), object_store_id, index_id,
      std::make_unique<IndexedDBKeyRange>(key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      max_count, std::move(aborting_callback)));
}

void DatabaseImpl::SetIndexKeys(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKey& primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "SetIndexKeys must be called from a version change transaction.");
    return;
  }

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::SetIndexKeysOperation,
                        connection_->database()->AsWeakPtr(), object_store_id,
                        std::make_unique<IndexedDBKey>(primary_key),
                        index_keys));
}

void DatabaseImpl::SetIndexesReady(int64_t transaction_id,
                                   int64_t object_store_id,
                                   const std::vector<int64_t>& index_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "SetIndexesReady must be called from a version change transaction.");
    return;
  }

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::SetIndexesReadyOperation,
                        connection_->database()->AsWeakPtr(),
                        index_ids.size()));
}

void DatabaseImpl::OpenCursor(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    blink::mojom::IDBCursorDirection direction,
    bool key_only,
    blink::mojom::IDBTaskType task_type,
    blink::mojom::IDBDatabase::OpenCursorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseOpenCursorResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    std::move(callback).Run(
        blink::mojom::IDBDatabaseOpenCursorResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBDatabase::OpenCursorCallback aborting_callback =
      CreateCallbackAbortOnDestruct<
          blink::mojom::IDBDatabase::OpenCursorCallback,
          blink::mojom::IDBDatabaseOpenCursorResultPtr>(
          std::move(callback), transaction->AsWeakPtr());

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange &&
      task_type == blink::mojom::IDBTaskType::Preemptive) {
    mojo::ReportBadMessage(
        "OpenCursor with |Preemptive| task type must be called from a version "
        "change transaction.");
    return;
  }

  std::unique_ptr<IndexedDBDatabase::OpenCursorOperationParams> params(
      std::make_unique<IndexedDBDatabase::OpenCursorOperationParams>());
  params->object_store_id = object_store_id;
  params->index_id = index_id;
  params->key_range = std::make_unique<IndexedDBKeyRange>(key_range);
  params->direction = direction;
  params->cursor_type =
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE;
  params->task_type = task_type;
  params->callback = std::move(aborting_callback);
  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::OpenCursorOperation,
                        connection_->database()->AsWeakPtr(), std::move(params),
                        origin_, dispatcher_host_->AsWeakPtr()));
}

void DatabaseImpl::Count(
    int64_t transaction_id,
    int64_t object_store_id,
    int64_t index_id,
    const IndexedDBKeyRange& key_range,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(pending_callbacks), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::CountOperation, connection_->database()->AsWeakPtr(),
      object_store_id, index_id,
      std::make_unique<blink::IndexedDBKeyRange>(key_range),
      std::move(callbacks)));
}

void DatabaseImpl::DeleteRange(
    int64_t transaction_id,
    int64_t object_store_id,
    const IndexedDBKeyRange& key_range,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(pending_callbacks), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::DeleteRangeOperation,
      connection_->database()->AsWeakPtr(), object_store_id,
      std::make_unique<IndexedDBKeyRange>(key_range), std::move(callbacks)));
}

void DatabaseImpl::GetKeyGeneratorCurrentNumber(
    int64_t transaction_id,
    int64_t object_store_id,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(pending_callbacks), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetKeyGeneratorCurrentNumberOperation,
      connection_->database()->AsWeakPtr(), object_store_id,
      std::move(callbacks)));
}

void DatabaseImpl::Clear(
    int64_t transaction_id,
    int64_t object_store_id,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
        pending_callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(dispatcher_host_->AsWeakPtr(), origin_,
                             std::move(pending_callbacks), idb_runner_));
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::ClearOperation, connection_->database()->AsWeakPtr(),
      object_store_id, std::move(callbacks)));
}

void DatabaseImpl::CreateIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const base::string16& name,
                               const IndexedDBKeyPath& key_path,
                               bool unique,
                               bool multi_entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "CreateIndex must be called from a version change transaction.");
    return;
  }

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::CreateIndexOperation,
                        connection_->database()->AsWeakPtr(), object_store_id,
                        index_id, name, key_path, unique, multi_entry));
}

void DatabaseImpl::DeleteIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "DeleteIndex must be called from a version change transaction.");
    return;
  }

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::DeleteIndexOperation,
      connection_->database()->AsWeakPtr(), object_store_id, index_id));
}

void DatabaseImpl::RenameIndex(int64_t transaction_id,
                               int64_t object_store_id,
                               int64_t index_id,
                               const base::string16& new_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  if (transaction->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "RenameIndex must be called from a version change transaction.");
    return;
  }

  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::RenameIndexOperation,
                        connection_->database()->AsWeakPtr(), object_store_id,
                        index_id, new_name));
}

void DatabaseImpl::Abort(int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connection_->IsConnected())
    return;

  IndexedDBTransaction* transaction =
      connection_->GetTransaction(transaction_id);
  if (!transaction)
    return;

  connection_->AbortTransactionAndTearDownOnError(
      transaction,
      IndexedDBDatabaseError(blink::mojom::IDBException::kAbortError,
                             "Transaction aborted by user."));
}

}  // namespace content
