// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database_callbacks.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/public/browser/browser_task_traits.h"

using blink::mojom::IDBDatabaseCallbacksAssociatedPtrInfo;

namespace content {

// TODO(cmp): Flatten calls / remove this class once IDB task runner CL settles.
class IndexedDBDatabaseCallbacks::Helper {
 public:
  explicit Helper(IDBDatabaseCallbacksAssociatedPtrInfo callbacks_info,
                  base::SequencedTaskRunner* idb_runner);
  ~Helper();

  void SendForcedClose();
  void SendVersionChange(int64_t old_version, int64_t new_version);
  void SendAbort(int64_t transaction_id, const IndexedDBDatabaseError& error);
  void SendComplete(int64_t transaction_id);
  void SendChanges(blink::mojom::IDBObserverChangesPtr changes);
  void OnConnectionError();

 private:
  blink::mojom::IDBDatabaseCallbacksAssociatedPtr callbacks_;
  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(Helper);
};

IndexedDBDatabaseCallbacks::IndexedDBDatabaseCallbacks(
    scoped_refptr<IndexedDBContextImpl> context,
    IDBDatabaseCallbacksAssociatedPtrInfo callbacks_info,
    base::SequencedTaskRunner* idb_runner)
    : indexed_db_context_(std::move(context)),
      helper_(new Helper(std::move(callbacks_info), idb_runner)) {
  DCHECK(idb_runner->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

IndexedDBDatabaseCallbacks::~IndexedDBDatabaseCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBDatabaseCallbacks::OnForcedClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  DCHECK(helper_);
  helper_->SendForcedClose();
  complete_ = true;
}

void IndexedDBDatabaseCallbacks::OnVersionChange(int64_t old_version,
                                                 int64_t new_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  DCHECK(helper_);
  helper_->SendVersionChange(old_version, new_version);
}

void IndexedDBDatabaseCallbacks::OnAbort(
    const IndexedDBTransaction& transaction,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  DCHECK(helper_);
  helper_->SendAbort(transaction.id(), error);
}

void IndexedDBDatabaseCallbacks::OnComplete(
    const IndexedDBTransaction& transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (complete_)
    return;

  indexed_db_context_->TransactionComplete(transaction.database()->origin());
  DCHECK(helper_);
  helper_->SendComplete(transaction.id());
}

void IndexedDBDatabaseCallbacks::OnDatabaseChange(
    blink::mojom::IDBObserverChangesPtr changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  helper_->SendChanges(std::move(changes));
}

IndexedDBDatabaseCallbacks::Helper::Helper(
    IDBDatabaseCallbacksAssociatedPtrInfo callbacks_info,
    base::SequencedTaskRunner* idb_runner) {
  DCHECK(idb_runner->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callbacks_info.is_valid())
    return;
  callbacks_.Bind(std::move(callbacks_info));
  // |callbacks_| is owned by |this|, so if |this| is destroyed, then
  // |callbacks_| will also be destroyed.  While |callbacks_| is otherwise
  // alive, |this| will always be valid.
  callbacks_.set_connection_error_handler(
      base::BindOnce(&Helper::OnConnectionError, base::Unretained(this)));
}

IndexedDBDatabaseCallbacks::Helper::~Helper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBDatabaseCallbacks::Helper::SendForcedClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callbacks_)
    callbacks_->ForcedClose();
}

void IndexedDBDatabaseCallbacks::Helper::SendVersionChange(
    int64_t old_version,
    int64_t new_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callbacks_)
    callbacks_->VersionChange(old_version, new_version);
}

void IndexedDBDatabaseCallbacks::Helper::SendAbort(
    int64_t transaction_id,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callbacks_)
    callbacks_->Abort(transaction_id, error.code(), error.message());
}

void IndexedDBDatabaseCallbacks::Helper::SendComplete(int64_t transaction_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callbacks_)
    callbacks_->Complete(transaction_id);
}

void IndexedDBDatabaseCallbacks::Helper::SendChanges(
    blink::mojom::IDBObserverChangesPtr changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callbacks_)
    callbacks_->Changes(std::move(changes));
}

void IndexedDBDatabaseCallbacks::Helper::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.reset();
}

}  // namespace content
