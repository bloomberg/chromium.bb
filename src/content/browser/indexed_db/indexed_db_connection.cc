// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_connection.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_storage_key_state.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {

namespace {

static int32_t g_next_indexed_db_connection_id;

}  // namespace

IndexedDBConnection::IndexedDBConnection(
    IndexedDBStorageKeyStateHandle storage_key_state_handle,
    IndexedDBClassFactory* indexed_db_class_factory,
    base::WeakPtr<IndexedDBDatabase> database,
    base::RepeatingClosure on_version_change_ignored,
    base::OnceCallback<void(IndexedDBConnection*)> on_close,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks)
    : id_(g_next_indexed_db_connection_id++),
      storage_key_state_handle_(std::move(storage_key_state_handle)),
      indexed_db_class_factory_(indexed_db_class_factory),
      database_(std::move(database)),
      on_version_change_ignored_(std::move(on_version_change_ignored)),
      on_close_(std::move(on_close)),
      callbacks_(callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

IndexedDBConnection::~IndexedDBConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected())
    return;

  // TODO(dmurph): Enforce that IndexedDBConnection cannot have any transactions
  // during destruction. This is likely the case during regular execution, but
  // is definitely not the case in unit tests.

  leveldb::Status status =
      AbortTransactionsAndClose(CloseErrorHandling::kAbortAllReturnLastError);
  if (!status.ok())
    storage_key_state_handle_.storage_key_state()->tear_down_callback().Run(
        status);
}

leveldb::Status IndexedDBConnection::AbortTransactionsAndClose(
    CloseErrorHandling error_handling) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected())
    return leveldb::Status::OK();

  DCHECK(database_);
  callbacks_ = nullptr;

  // Finish up any transaction, in case there were any running.
  IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                               "Connection is closing.");
  leveldb::Status status;
  switch (error_handling) {
    case CloseErrorHandling::kReturnOnFirstError:
      status = AbortAllTransactions(error);
      break;
    case CloseErrorHandling::kAbortAllReturnLastError:
      status = AbortAllTransactionsAndIgnoreErrors(error);
      break;
  }

  std::move(on_close_).Run(this);
  storage_key_state_handle_.Release();
  return status;
}

leveldb::Status IndexedDBConnection::CloseAndReportForceClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsConnected())
    return leveldb::Status::OK();

  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks(callbacks_);
  leveldb::Status last_error =
      AbortTransactionsAndClose(CloseErrorHandling::kAbortAllReturnLastError);
  callbacks->OnForcedClose();
  return last_error;
}

void IndexedDBConnection::VersionChangeIgnored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_version_change_ignored_.Run();
}

bool IndexedDBConnection::IsConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callbacks_.get();
}

IndexedDBTransaction* IndexedDBConnection::CreateTransaction(
    int64_t id,
    const std::set<int64_t>& scope,
    blink::mojom::IDBTransactionMode mode,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(GetTransaction(id), nullptr) << "Duplicate transaction id." << id;
  IndexedDBStorageKeyState* storage_key_state =
      storage_key_state_handle_.storage_key_state();
  std::unique_ptr<IndexedDBTransaction> transaction =
      indexed_db_class_factory_->CreateIndexedDBTransaction(
          id, this, scope, mode, database()->tasks_available_callback(),
          storage_key_state ? storage_key_state->tear_down_callback()
                            : IndexedDBTransaction::TearDownCallback(),
          backing_store_transaction);
  IndexedDBTransaction* transaction_ptr = transaction.get();
  transactions_[id] = std::move(transaction);
  return transaction_ptr;
}

void IndexedDBConnection::AbortTransactionAndTearDownOnError(
    IndexedDBTransaction* transaction,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE1("IndexedDBDatabase::Abort(error)", "txn.id", transaction->id());
  leveldb::Status status = transaction->Abort(error);
  if (!status.ok())
    storage_key_state_handle_.storage_key_state()->tear_down_callback().Run(
        status);
}

leveldb::Status IndexedDBConnection::AbortAllTransactions(
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& pair : transactions_) {
    auto& transaction = pair.second;
    if (transaction->state() != IndexedDBTransaction::FINISHED) {
      IDB_TRACE1("IndexedDBDatabase::Abort(error)", "transaction.id",
                 transaction->id());
      leveldb::Status status = transaction->Abort(error);
      if (!status.ok())
        return status;
    }
  }
  return leveldb::Status::OK();
}

leveldb::Status IndexedDBConnection::AbortAllTransactionsAndIgnoreErrors(
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status last_error;
  for (const auto& pair : transactions_) {
    auto& transaction = pair.second;
    if (transaction->state() != IndexedDBTransaction::FINISHED) {
      IDB_TRACE1("IndexedDBDatabase::Abort(error)", "transaction.id",
                 transaction->id());
      leveldb::Status status = transaction->Abort(error);
      if (!status.ok())
        last_error = status;
    }
  }
  return last_error;
}

IndexedDBTransaction* IndexedDBConnection::GetTransaction(int64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = transactions_.find(id);
  if (it == transactions_.end())
    return nullptr;
  return it->second.get();
}

base::WeakPtr<IndexedDBTransaction>
IndexedDBConnection::AddTransactionForTesting(
    std::unique_ptr<IndexedDBTransaction> transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(transactions_, transaction->id()));
  base::WeakPtr<IndexedDBTransaction> transaction_ptr =
      transaction->ptr_factory_.GetWeakPtr();
  transactions_[transaction->id()] = std::move(transaction);
  return transaction_ptr;
}

void IndexedDBConnection::RemoveTransaction(int64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transactions_.erase(id);
}

void IndexedDBConnection::ClearStateAfterClose() {
  callbacks_ = nullptr;
  storage_key_state_handle_.Release();
}

}  // namespace content
