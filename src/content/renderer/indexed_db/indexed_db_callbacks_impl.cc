// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/indexed_db/indexed_db_callbacks_impl.h"

#include "content/renderer/indexed_db/indexed_db_dispatcher.h"
#include "content/renderer/indexed_db/indexed_db_key_builders.h"
#include "content/renderer/indexed_db/webidbcursor_impl.h"
#include "content/renderer/indexed_db/webidbdatabase_impl.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_callbacks.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_error.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_metadata.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_value.h"

using blink::IndexedDBDatabaseMetadata;
using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBMetadata;
using blink::WebIDBValue;
using blink::WebString;
using blink::WebVector;
using blink::mojom::IDBDatabaseAssociatedPtrInfo;

namespace content {

namespace {

void ConvertIndexMetadata(const blink::IndexedDBIndexMetadata& metadata,
                          WebIDBMetadata::Index* output) {
  output->id = metadata.id;
  output->name = WebString::FromUTF16(metadata.name);
  output->key_path = WebIDBKeyPathBuilder::Build(metadata.key_path);
  output->unique = metadata.unique;
  output->multi_entry = metadata.multi_entry;
}

void ConvertObjectStoreMetadata(
    const blink::IndexedDBObjectStoreMetadata& metadata,
    WebIDBMetadata::ObjectStore* output) {
  output->id = metadata.id;
  output->name = WebString::FromUTF16(metadata.name);
  output->key_path = WebIDBKeyPathBuilder::Build(metadata.key_path);
  output->auto_increment = metadata.auto_increment;
  output->max_index_id = metadata.max_index_id;
  output->indexes = WebVector<WebIDBMetadata::Index>(metadata.indexes.size());
  size_t i = 0;
  for (const auto& iter : metadata.indexes)
    ConvertIndexMetadata(iter.second, &output->indexes[i++]);
}

void ConvertDatabaseMetadata(const IndexedDBDatabaseMetadata& metadata,
                             WebIDBMetadata* output) {
  output->id = metadata.id;
  output->name = WebString::FromUTF16(metadata.name);
  output->version = metadata.version;
  output->max_object_store_id = metadata.max_object_store_id;
  output->object_stores =
      WebVector<WebIDBMetadata::ObjectStore>(metadata.object_stores.size());
  size_t i = 0;
  for (const auto& iter : metadata.object_stores)
    ConvertObjectStoreMetadata(iter.second, &output->object_stores[i++]);
}

WebIDBValue ConvertReturnValue(const blink::mojom::IDBReturnValuePtr& value) {
  if (!value)
    return WebIDBValue(WebData(), WebVector<WebBlobInfo>());

  WebIDBValue web_value = IndexedDBCallbacksImpl::ConvertValue(value->value);
  web_value.SetInjectedPrimaryKey(WebIDBKeyBuilder::Build(value->primary_key),
                                  WebIDBKeyPathBuilder::Build(value->key_path));
  return web_value;
}

}  // namespace

// static
WebIDBValue IndexedDBCallbacksImpl::ConvertValue(
    const blink::mojom::IDBValuePtr& value) {
  if (!value || value->bits.empty())
    return WebIDBValue(WebData(), WebVector<WebBlobInfo>());

  WebVector<WebBlobInfo> local_blob_info;
  local_blob_info.reserve(value->blob_or_file_info.size());
  for (size_t i = 0; i < value->blob_or_file_info.size(); ++i) {
    const auto& info = value->blob_or_file_info[i];
    if (info->file) {
      local_blob_info.emplace_back(WebString::FromUTF8(info->uuid),
                                   blink::FilePathToWebString(info->file->path),
                                   WebString::FromUTF16(info->file->name),
                                   WebString::FromUTF16(info->mime_type),
                                   info->file->last_modified.ToDoubleT(),
                                   info->size, info->blob.PassHandle());
    } else {
      local_blob_info.emplace_back(WebString::FromUTF8(info->uuid),
                                   WebString::FromUTF16(info->mime_type),
                                   info->size, info->blob.PassHandle());
    }
  }

  return WebIDBValue(WebData(&*value->bits.begin(), value->bits.size()),
                     std::move(local_blob_info));
}

IndexedDBCallbacksImpl::IndexedDBCallbacksImpl(
    std::unique_ptr<WebIDBCallbacks> callbacks,
    int64_t transaction_id,
    const base::WeakPtr<WebIDBCursorImpl>& cursor)
    : callbacks_(std::move(callbacks)),
      cursor_(cursor),
      transaction_id_(transaction_id) {}

IndexedDBCallbacksImpl::~IndexedDBCallbacksImpl() = default;

void IndexedDBCallbacksImpl::Error(int32_t code,
                                   const base::string16& message) {
  callbacks_->OnError(
      blink::WebIDBDatabaseError(code, WebString::FromUTF16(message)));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessStringList(
    const std::vector<base::string16>& value) {
  WebVector<WebString> web_value(value.size());
  std::transform(
      value.begin(), value.end(), web_value.begin(),
      [](const base::string16& s) { return WebString::FromUTF16(s); });
  callbacks_->OnSuccess(web_value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::Blocked(int64_t existing_version) {
  callbacks_->OnBlocked(existing_version);
  // Not resetting |callbacks_|.
}

void IndexedDBCallbacksImpl::UpgradeNeeded(
    IDBDatabaseAssociatedPtrInfo database_info,
    int64_t old_version,
    blink::WebIDBDataLoss data_loss,
    const std::string& data_loss_message,
    const IndexedDBDatabaseMetadata& metadata) {
  WebIDBDatabase* database = new WebIDBDatabaseImpl(std::move(database_info));
  WebIDBMetadata web_metadata;
  ConvertDatabaseMetadata(metadata, &web_metadata);
  callbacks_->OnUpgradeNeeded(old_version, database, web_metadata, data_loss,
                              WebString::FromUTF8(data_loss_message));
  // Not resetting |callbacks_|.
}

void IndexedDBCallbacksImpl::SuccessDatabase(
    IDBDatabaseAssociatedPtrInfo database_info,
    const IndexedDBDatabaseMetadata& metadata) {
  WebIDBDatabase* database = nullptr;
  if (database_info.is_valid()) {
    database = new WebIDBDatabaseImpl(std::move(database_info));
  }

  WebIDBMetadata web_metadata;
  ConvertDatabaseMetadata(metadata, &web_metadata);
  callbacks_->OnSuccess(database, web_metadata);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursor(
    blink::mojom::IDBCursorAssociatedPtrInfo cursor_info,
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    blink::mojom::IDBValuePtr value) {
  WebIDBCursorImpl* cursor =
      new WebIDBCursorImpl(std::move(cursor_info), transaction_id_);
  callbacks_->OnSuccess(cursor, WebIDBKeyBuilder::Build(key),
                        WebIDBKeyBuilder::Build(primary_key),
                        ConvertValue(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessValue(
    blink::mojom::IDBReturnValuePtr value) {
  callbacks_->OnSuccess(ConvertReturnValue(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursorContinue(
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key,
    blink::mojom::IDBValuePtr value) {
  callbacks_->OnSuccess(WebIDBKeyBuilder::Build(key),
                        WebIDBKeyBuilder::Build(primary_key),
                        ConvertValue(value));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessCursorPrefetch(
    const std::vector<IndexedDBKey>& keys,
    const std::vector<IndexedDBKey>& primary_keys,
    std::vector<blink::mojom::IDBValuePtr> values) {
  std::vector<WebIDBValue> web_values;
  web_values.reserve(values.size());
  for (const blink::mojom::IDBValuePtr& value : values)
    web_values.emplace_back(ConvertValue(value));

  if (cursor_) {
    cursor_->SetPrefetchData(keys, primary_keys, std::move(web_values));
    cursor_->CachedContinue(callbacks_.get());
  }
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessArray(
    std::vector<blink::mojom::IDBReturnValuePtr> values) {
  WebVector<WebIDBValue> web_values;
  web_values.reserve(values.size());
  for (const blink::mojom::IDBReturnValuePtr& value : values)
    web_values.emplace_back(ConvertReturnValue(value));
  callbacks_->OnSuccess(std::move(web_values));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessKey(const IndexedDBKey& key) {
  callbacks_->OnSuccess(WebIDBKeyBuilder::Build(key));
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::SuccessInteger(int64_t value) {
  callbacks_->OnSuccess(value);
  callbacks_.reset();
}

void IndexedDBCallbacksImpl::Success() {
  callbacks_->OnSuccess();
  callbacks_.reset();
}

}  // namespace content
