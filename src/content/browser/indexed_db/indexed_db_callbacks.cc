// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callbacks.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/indexed_db/cursor_impl.h"
#include "content/browser/indexed_db/database_impl.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBKey;
using blink::mojom::IDBCallbacksAssociatedPtrInfo;
using std::swap;
using storage::ShareableFileReference;

namespace content {

namespace {

// The following two objects protect the given objects from being destructed
// while the current transaction task queue is being processed.
class SafeConnectionWrapper {
 public:
  SafeConnectionWrapper(std::unique_ptr<IndexedDBConnection> connection)
      : connection_(std::move(connection)),
        idb_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  ~SafeConnectionWrapper() {
    if (connection_) {
      idb_runner_->PostTask(
          FROM_HERE, base::BindOnce(
                         [](std::unique_ptr<IndexedDBConnection> connection) {
                           connection->ForceClose();
                         },
                         std::move(connection_)));
    }
  }
  SafeConnectionWrapper(SafeConnectionWrapper&& other) = default;

  std::unique_ptr<IndexedDBConnection> connection_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeConnectionWrapper);
};

class SafeCursorWrapper {
 public:
  SafeCursorWrapper(std::unique_ptr<IndexedDBCursor> cursor)
      : cursor_(std::move(cursor)),
        idb_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  ~SafeCursorWrapper() {
    if (cursor_)
      idb_runner_->DeleteSoon(FROM_HERE, cursor_.release());
  }
  SafeCursorWrapper(SafeCursorWrapper&& other) = default;

  std::unique_ptr<IndexedDBCursor> cursor_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeCursorWrapper);
};

std::unique_ptr<storage::BlobDataHandle> CreateBlobData(
    std::string uuid,
    storage::BlobStorageContext* blob_context,
    base::SequencedTaskRunner* idb_runner,
    const IndexedDBBlobInfo& blob_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (blob_info.blob_handle()) {
    // We're sending back a live blob, not a reference into our backing store.
    return std::make_unique<storage::BlobDataHandle>(*blob_info.blob_handle());
  }
  scoped_refptr<ShareableFileReference> shareable_file =
      ShareableFileReference::Get(blob_info.file_path());
  if (!shareable_file) {
    shareable_file = ShareableFileReference::GetOrCreate(
        blob_info.file_path(),
        ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE, idb_runner);
    if (!blob_info.release_callback().is_null())
      shareable_file->AddFinalReleaseCallback(blob_info.release_callback());
  }
  auto blob_data_builder = std::make_unique<storage::BlobDataBuilder>(uuid);
  blob_data_builder->set_content_type(base::UTF16ToUTF8(blob_info.type()));
  blob_data_builder->AppendFile(blob_info.file_path(), 0, blob_info.size(),
                                blob_info.last_modified());
  return blob_context->AddFinishedBlob(std::move(blob_data_builder));
}

}  // namespace

IndexedDBCallbacks::IndexedDBValueBlob::IndexedDBValueBlob(
    const IndexedDBBlobInfo& blob_info,
    blink::mojom::IDBBlobInfoPtr* blob_or_file_info)
    : blob_info_(blob_info) {
  if (blob_info_.blob_handle()) {
    uuid_ = blob_info_.blob_handle()->uuid();
  } else {
    uuid_ = base::GenerateGUID();
  }
  (*blob_or_file_info)->uuid = uuid_;
  request_ = mojo::MakeRequest(&(*blob_or_file_info)->blob);
}
IndexedDBCallbacks::IndexedDBValueBlob::IndexedDBValueBlob(
    IndexedDBValueBlob&& other) = default;
IndexedDBCallbacks::IndexedDBValueBlob::~IndexedDBValueBlob() = default;

// static
void IndexedDBCallbacks::IndexedDBValueBlob::GetIndexedDBValueBlobs(
    std::vector<IndexedDBValueBlob>* value_blobs,
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<blink::mojom::IDBBlobInfoPtr>* blob_or_file_info) {
  DCHECK(value_blobs);
  DCHECK(blob_or_file_info);
  DCHECK_EQ(blob_info.size(), blob_or_file_info->size());
  value_blobs->reserve(value_blobs->size() + blob_info.size());
  for (size_t i = 0; i < blob_info.size(); i++) {
    value_blobs->push_back(
        IndexedDBValueBlob(blob_info[i], &(*blob_or_file_info)[i]));
  }
}

// static
std::vector<IndexedDBCallbacks::IndexedDBValueBlob>
IndexedDBCallbacks::IndexedDBValueBlob::GetIndexedDBValueBlobs(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<blink::mojom::IDBBlobInfoPtr>* blob_or_file_info) {
  std::vector<IndexedDBValueBlob> value_blobs;
  IndexedDBValueBlob::GetIndexedDBValueBlobs(&value_blobs, blob_info,
                                             blob_or_file_info);
  return value_blobs;
}

// static
bool IndexedDBCallbacks::CreateAllBlobs(
    scoped_refptr<ChromeBlobStorageContext> blob_context,
    scoped_refptr<base::SequencedTaskRunner> idb_runner,
    std::vector<IndexedDBValueBlob> value_blobs) {
  DCHECK(idb_runner->RunsTasksInCurrentSequence());
  IDB_TRACE("IndexedDBCallbacks::CreateAllBlobs");

  if (value_blobs.empty())
    return true;

  // TODO(crbug.com/932869): Remove IO thread hop entirely.
  base::WaitableEvent signal_when_finished(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool result;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          [](scoped_refptr<ChromeBlobStorageContext> inner_blob_context,
             scoped_refptr<base::SequencedTaskRunner> inner_idb_runner,
             std::vector<IndexedDBValueBlob> inner_value_blobs,
             base::WaitableEvent* inner_signal_when_finished,
             bool* inner_result) {
            base::ScopedClosureRunner signal_runner(base::BindOnce(
                [](base::WaitableEvent* signal) { signal->Signal(); },
                inner_signal_when_finished));

            if (!inner_blob_context) {
              *inner_result = false;
              return;
            }

            for (size_t i = 0; i < inner_value_blobs.size(); ++i) {
              std::unique_ptr<storage::BlobDataHandle> blob_data =
                  CreateBlobData(
                      inner_value_blobs[i].uuid_, inner_blob_context->context(),
                      inner_idb_runner.get(), inner_value_blobs[i].blob_info_);
              storage::BlobImpl::Create(
                  std::move(blob_data),
                  std::move(inner_value_blobs[i].request_));
            }
            *inner_result = true;
          },
          std::move(blob_context), std::move(idb_runner),
          std::move(value_blobs), &signal_when_finished, &result));
  signal_when_finished.Wait();
  return result;
}

IndexedDBCallbacks::IndexedDBCallbacks(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    const url::Origin& origin,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info,
    scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : data_loss_(blink::mojom::IDBDataLoss::None),
      dispatcher_host_(std::move(dispatcher_host)),
      origin_(origin),
      idb_runner_(std::move(idb_runner)) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callbacks_info.is_valid()) {
    callbacks_.Bind(std::move(callbacks_info));
    // |callbacks_| is owned by |this|, so if |this| is destroyed, then
    // |callbacks_| will also be destroyed.  While |callbacks_| is otherwise
    // alive, |this| will always be valid.
    callbacks_.set_connection_error_handler(base::BindOnce(
        &IndexedDBCallbacks::OnConnectionError, base::Unretained(this)));
  }
}

IndexedDBCallbacks::~IndexedDBCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBCallbacks::OnError(const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->Error(error.code(), error.message());
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(
    std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->SuccessNamesAndVersionsList(std::move(names_and_versions));
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(const std::vector<base::string16>& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->SuccessStringList(value);
  complete_ = true;
}

void IndexedDBCallbacks::OnBlocked(int64_t existing_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (sent_blocked_)
    return;

  sent_blocked_ = true;

  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  if (callbacks_)
    callbacks_->Blocked(existing_version);
}

void IndexedDBCallbacks::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata,
    const IndexedDBDataLossInfo& data_loss_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK(!connection_created_);

  data_loss_ = data_loss_info.status;
  connection_created_ = true;

  SafeConnectionWrapper wrapper(std::move(connection));
  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }

  auto database =
      std::make_unique<DatabaseImpl>(std::move(wrapper.connection_), origin_,
                                     dispatcher_host_.get(), idb_runner_);

  blink::mojom::IDBDatabaseAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);

  dispatcher_host_->AddDatabaseBinding(std::move(database), std::move(request));
  callbacks_->UpgradeNeeded(std::move(ptr_info), old_version,
                            data_loss_info.status, data_loss_info.message,
                            metadata);
}

void IndexedDBCallbacks::OnSuccess(
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(connection_created_, !connection);

  scoped_refptr<IndexedDBCallbacks> self(this);

  // Only create a new connection if one was not previously sent in
  // OnUpgradeNeeded.
  std::unique_ptr<IndexedDBConnection> database_connection;
  if (!connection_created_)
    database_connection = std::move(connection);

  SafeConnectionWrapper wrapper(std::move(database_connection));
  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  blink::mojom::IDBDatabaseAssociatedPtrInfo ptr_info;
  if (wrapper.connection_) {
    auto database =
        std::make_unique<DatabaseImpl>(std::move(wrapper.connection_), origin_,
                                       dispatcher_host_.get(), idb_runner_);

    auto request = mojo::MakeRequest(&ptr_info);
    dispatcher_host_->AddDatabaseBinding(std::move(database),
                                         std::move(request));
  }
  callbacks_->SuccessDatabase(std::move(ptr_info), metadata);
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(std::unique_ptr<IndexedDBCursor> cursor,
                                   const IndexedDBKey& key,
                                   const IndexedDBKey& primary_key,
                                   IndexedDBValue* value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(blink::mojom::IDBDataLoss::None, data_loss_);

  blink::mojom::IDBValuePtr mojo_value;
  std::vector<IndexedDBBlobInfo> blob_info;
  if (value) {
    mojo_value = IndexedDBValue::ConvertAndEraseValue(value);
    blob_info.swap(value->blob_info);
  }

  SafeCursorWrapper cursor_wrapper(std::move(cursor));
  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  auto cursor_impl =
      std::make_unique<CursorImpl>(std::move(cursor_wrapper.cursor_), origin_,
                                   dispatcher_host_.get(), idb_runner_);
  if (mojo_value && !IndexedDBCallbacks::CreateAllBlobs(
                        dispatcher_host_->blob_storage_context(), idb_runner_,
                        IndexedDBValueBlob::GetIndexedDBValueBlobs(
                            blob_info, &mojo_value->blob_or_file_info))) {
    return;
  }

  blink::mojom::IDBCursorAssociatedPtrInfo ptr_info;
  auto request = mojo::MakeRequest(&ptr_info);
  dispatcher_host_->AddCursorBinding(std::move(cursor_impl),
                                     std::move(request));
  callbacks_->SuccessCursor(std::move(ptr_info), key, primary_key,
                            std::move(mojo_value));
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(IndexedDBReturnValue* value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(blink::mojom::IDBDataLoss::None, data_loss_);

  blink::mojom::IDBReturnValuePtr mojo_value;
  std::vector<IndexedDBBlobInfo> blob_info;
  if (value) {
    mojo_value = IndexedDBReturnValue::ConvertReturnValue(value);
    blob_info = value->blob_info;
  }

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }

  if (mojo_value &&
      !IndexedDBCallbacks::CreateAllBlobs(
          dispatcher_host_->blob_storage_context(), idb_runner_,
          IndexedDBValueBlob::GetIndexedDBValueBlobs(
              blob_info, &mojo_value->value->blob_or_file_info))) {
    return;
  }

  callbacks_->SuccessValue(std::move(mojo_value));
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccessArray(
    std::vector<IndexedDBReturnValue>* values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(blink::mojom::IDBDataLoss::None, data_loss_);

  std::vector<blink::mojom::IDBReturnValuePtr> mojo_values;
  mojo_values.reserve(values->size());
  for (size_t i = 0; i < values->size(); ++i) {
    mojo_values.push_back(
        IndexedDBReturnValue::ConvertReturnValue(&(*values)[i]));
  }

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }

  std::vector<IndexedDBValueBlob> value_blobs;
  for (size_t i = 0; i < mojo_values.size(); ++i) {
    IndexedDBValueBlob::GetIndexedDBValueBlobs(
        &value_blobs, (*values)[i].blob_info,
        &mojo_values[i]->value->blob_or_file_info);
  }

  if (!IndexedDBCallbacks::CreateAllBlobs(
          dispatcher_host_->blob_storage_context(), idb_runner_,
          std::move(value_blobs))) {
    return;
  }

  callbacks_->SuccessArray(std::move(mojo_values));
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(const IndexedDBKey& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(blink::mojom::IDBDataLoss::None, data_loss_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->SuccessKey(value);
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->SuccessInteger(value);
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(blink::mojom::IDBDataLoss::None, data_loss_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->Success();
  complete_ = true;
}

void IndexedDBCallbacks::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.reset();
  dispatcher_host_ = nullptr;
}

}  // namespace content
